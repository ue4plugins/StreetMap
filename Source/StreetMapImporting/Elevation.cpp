// Copyright 2017 Richard Schubert. All Rights Reserved.

#include "StreetMapImporting.h"
#include "Elevation.h"

#include "DesktopPlatformModule.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "HttpManager.h"
#include "Misc/ScopedSlowTask.h"
#include "Interfaces/IImageWrapperModule.h"
#include "SNotificationList.h"
#include "NotificationManager.h"

#define LOCTEXT_NAMESPACE "StreetMapImporting"

static void ShowErrorMessage(const FText& MessageText)
{
	FNotificationInfo Info(MessageText);
	Info.ExpireDuration = 8.0f;
	Info.bUseLargeFont = false;
	TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info);
	if (Notification.IsValid())
	{
		Notification->SetCompletionState(SNotificationItem::CS_Fail);
		Notification->ExpireAndFadeout();
	}
}

static const FString& GetElevationCacheDir()
{
	static FString ElevationCacheDir;

	if (!ElevationCacheDir.Len())
	{
		auto UserTempDir = FPaths::ConvertRelativePathToFull(FDesktopPlatformModule::Get()->GetUserTempPath());
		ElevationCacheDir = FString::Printf(TEXT("%s%s"), *UserTempDir, TEXT("ElevationCache/"));
	}
	return ElevationCacheDir;
}

static FString GetCachedFilePath(uint32 X, uint32 Y, uint32 Z)
{
	FString FilePath = GetElevationCacheDir();
	FilePath.Append("elevation_");
	FilePath.AppendInt(Z);
	FilePath.Append("_");
	FilePath.AppendInt(X);
	FilePath.Append("_");
	FilePath.AppendInt(Y);
	FilePath.Append(".png");
	return FilePath;
}

static const int32 ExpectedElevationTileSize = 256;

class FCachedElevationFile
{
private:
	const FString URLTemplate = TEXT("http://s3.amazonaws.com/elevation-tiles-prod/terrarium/%d/%d/%d.png");

	bool WasInitialized;
	bool WasDownloadASuccess;
	bool Failed;

	uint32 X, Y, Z;

	FDateTime StartTime;
	FTimespan TimeSpan;

	TArray<float> Elevation;

	TSharedPtr<IHttpRequest> HttpRequest;

	bool UnpackElevation(const TArray<uint8>& RawData)
	{
		IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));

		IImageWrapperPtr PngImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);
		if (PngImageWrapper.IsValid() && PngImageWrapper->SetCompressed(RawData.GetData(), RawData.Num()))
		{
			int32 BitDepth = PngImageWrapper->GetBitDepth();
			const ERGBFormat::Type Format = PngImageWrapper->GetFormat();
			const int32 Width = PngImageWrapper->GetWidth();
			const int32 Height = PngImageWrapper->GetHeight();

			if (Width != ExpectedElevationTileSize || Height != ExpectedElevationTileSize)
			{
				GWarn->Logf(ELogVerbosity::Error, TEXT("PNG file has wrong dimensions. Expected %dx%d"), ExpectedElevationTileSize, ExpectedElevationTileSize);
				return false;
			}

			if ((Format != ERGBFormat::RGBA && Format != ERGBFormat::BGRA) || BitDepth > 8)
			{
				GWarn->Logf(ELogVerbosity::Error, TEXT("PNG file contains elevation data in an unsupported format."));
				return false;
			}

			if (BitDepth <= 8)
			{
				BitDepth = 8;
			}

			const TArray<uint8>* RawPNG = nullptr;
			if (PngImageWrapper->GetRaw(Format, BitDepth, RawPNG))
			{
				const uint8* Data = RawPNG->GetData();
				Elevation.Reserve(Width * Height);
				float* ElevationData = Elevation.GetData();
				const float* ElevationDataEnd = ElevationData + (Width * Height);

				while (ElevationData < ElevationDataEnd)
				{
					float ElevationValue = (Data[0] * 256.0f + Data[1] + Data[2] / 256.0f) - 32768.0f;

					*ElevationData = ElevationValue;

					ElevationData++;
					Data += 4;
				}				
			}

			return true;
		}

		return false;
	}

	void OnDownloadSucceeded(FHttpResponsePtr Response)
	{
		// unpack data
		if (Response.IsValid())
		{
			auto Content = Response->GetContent();
			if (!UnpackElevation(Content))
			{
				Failed = true;
				return;
			}

			// write data to cache
			FFileHelper::SaveArrayToFile(Content, *GetCachedFilePath(X, Y, Z));
		}

		WasDownloadASuccess = true;
	}

	void DownloadFile()
	{
		FString URL = FString::Printf(*URLTemplate, Z, X, Y);

		HttpRequest = FHttpModule::Get().CreateRequest();
		HttpRequest->SetVerb(TEXT("GET"));

		HttpRequest->SetURL(URL);
		if (!HttpRequest->ProcessRequest())
		{
			Failed = true;
		}
	}

	void Initialize()
	{
		WasInitialized = true;

		// try to load data from cache first
		{
			TArray<uint8> RawData;
			if (FFileHelper::LoadFileToArray(RawData, *GetCachedFilePath(X, Y, Z)))
			{
				if (UnpackElevation(RawData))
				{
					WasDownloadASuccess = true;
					return;
				}
			}
		}
		DownloadFile();
	}

public:

	bool HasFinished() const
	{
		return WasDownloadASuccess || Failed;
	}

	bool Succeeded() const
	{
		return WasDownloadASuccess;
	}

	void CancelRequest()
	{
		Failed = true;
		HttpRequest->CancelRequest();
	}

	void Tick()
	{
		if (!WasInitialized)
		{
			Initialize();
		}

		if (WasDownloadASuccess || Failed) return;

		if (TimeSpan.GetSeconds() > 10)
		{
			GWarn->Logf(ELogVerbosity::Error, TEXT("Download time-out. Check your internet connection!"));
			Failed = true;
			HttpRequest->CancelRequest();
			return;
		}
		else
		{
			TimeSpan = FDateTime::UtcNow() - StartTime;
		}

		if (HttpRequest->GetStatus() == EHttpRequestStatus::Failed ||
			HttpRequest->GetStatus() == EHttpRequestStatus::Failed_ConnectionError)
		{
			GWarn->Logf(ELogVerbosity::Error, TEXT("Download connection failure. Check your internet connection!"));
			Failed = true;
			HttpRequest->CancelRequest();
			return;
		}

		if (HttpRequest->GetStatus() == EHttpRequestStatus::Succeeded)
		{
			OnDownloadSucceeded(HttpRequest->GetResponse());
			return;
		}

		HttpRequest->Tick(0);
	}

	const TArray<float>& GetElevationData()
	{
		return Elevation;
	}

	FCachedElevationFile(uint32 X, uint32 Y, uint32 Z)
		: WasInitialized(false)
		, WasDownloadASuccess(false)
		, Failed(false)
		, X(X)
		, Y(Y)
		, Z(Z)
		, StartTime(FDateTime::UtcNow())
	{
	}
};

ALandscape* BuildLandscape(UWorld* World, const FStreetMapLandscapeBuildSettings& BuildSettings)
{
	ALandscape* Landscape = World->SpawnActor<ALandscape>(ALandscape::StaticClass());

	FScopedSlowTask SlowTask(2.0f, LOCTEXT("GeneratingLandscape", "Generating Landscape"));
	SlowTask.MakeDialog(true);

	// TODO: download correct data

	TArray<TSharedPtr<FCachedElevationFile>> FilesToDownload;
	FilesToDownload.Add(MakeShared<FCachedElevationFile>(1, 1, 2));
	FilesToDownload.Add(MakeShared<FCachedElevationFile>(2, 1, 2));
	FilesToDownload.Add(MakeShared<FCachedElevationFile>(1, 2, 2));
	FilesToDownload.Add(MakeShared<FCachedElevationFile>(2, 2, 2));

	TArray<TSharedPtr<FCachedElevationFile>> FilesDownloaded;
	const int32 NumFilesToDownload = FilesToDownload.Num();
	while (FilesToDownload.Num())
	{
		FHttpModule::Get().GetHttpManager().Tick(0);

		if (GWarn->ReceivedUserCancel())
		{
			break;
		}

		float Progress = 0.0f;
		for (auto FileToDownload : FilesToDownload)
		{
			FileToDownload->Tick();

			if (FileToDownload->HasFinished())
			{
				FilesToDownload.Remove(FileToDownload);
				Progress = 1.0f / (float)NumFilesToDownload;

				if (FileToDownload->Succeeded())
				{
					FilesDownloaded.Add(FileToDownload);
				}
				else
				{
					// We failed to download one file so cancel the rest because we cannot proceed without it.
					for (auto FileToCancel : FilesToDownload)
					{
						FileToCancel->CancelRequest();
					}
					FilesToDownload.Empty();
				}
				break;
			}
		}

		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("NumFilesDownloaded"), FText::AsNumber(FilesDownloaded.Num()));
		Arguments.Add(TEXT("NumFilesToDownload"), FText::AsNumber(NumFilesToDownload));
		SlowTask.EnterProgressFrame(Progress, FText::Format(LOCTEXT("DownloadingElevationModel", "Downloading Elevation Model ({NumFilesDownloaded} of {NumFilesToDownload})"), Arguments));

		FPlatformProcess::Sleep(0.1f);
	}

	if (FilesDownloaded.Num() < NumFilesToDownload)
	{
		ShowErrorMessage(LOCTEXT("DownloadElevationFailed", "Could not download all necessary elevation model files. See Log for details!"));
		World->DestroyActor(Landscape);
		return nullptr;
	}

	/*Landscape->Import(
	const FGuid Guid,
	const int32 MinX, const int32 MinY, const int32 MaxX, const int32 MaxY,
	const int32 InNumSubsections, const int32 InSubsectionSizeQuads,
	const uint16* const HeightData, const TCHAR* const HeightmapFileName,
	const TArray<FLandscapeImportLayerInfo>& ImportLayerInfos, const ELandscapeImportAlphamapType ImportLayerType);
	*/

	return Landscape;
}
