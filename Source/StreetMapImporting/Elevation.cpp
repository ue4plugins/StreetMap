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

class FCachedElevationFile
{
private:
	const FString URLTemplate = TEXT("http://s3.amazonaws.com/elevation-tiles-prod/terrarium/%d/%d/%d.png");

	bool WasDownloadASuccess;
	bool Failed;

	uint32 X, Y, Z;

	FDateTime StartTime;
	FTimespan TimeSpan;

	TSharedPtr<IHttpRequest> HttpRequest;

	bool UnpackElevation(const TArray<uint8>& RawData)
	{
		IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));

		IImageWrapperPtr PngImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);
		if (PngImageWrapper.IsValid() && PngImageWrapper->SetCompressed(RawData.GetData(), RawData.Num()))
		{
			int32 BitDepth = PngImageWrapper->GetBitDepth();
			ERGBFormat::Type Format = PngImageWrapper->GetFormat();

			if ((Format != ERGBFormat::RGBA && Format != ERGBFormat::BGRA) || BitDepth > 8)
			{
				GWarn->Logf(ELogVerbosity::Error, TEXT("PNG file contains elevation data in an unsupported format."));
				return false;
			}

			if (BitDepth <= 8)
			{
				BitDepth = 8;
			}

			PngImageWrapper->GetWidth();
			PngImageWrapper->GetHeight();
			const TArray<uint8>* RawPNG = nullptr;
			if (PngImageWrapper->GetRaw(Format, BitDepth, RawPNG))
			{

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

			// TODO: write data to cache
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
		if (WasDownloadASuccess || Failed) return;

		if (TimeSpan.GetSeconds() > 10)
		{
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

	FCachedElevationFile(uint32 X, uint32 Y, uint32 Z)
		: WasDownloadASuccess(false)
		, Failed(false)
		, X(X)
		, Y(Y)
		, Z(Z)
		, StartTime(FDateTime::UtcNow())
	{
		// TODO: try to load data from cache first
		DownloadFile();
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
		ShowErrorMessage(LOCTEXT("DownloadElevationFailed", "Could not download all necessary elevation model files."));
		World->DestroyActor(Landscape);
		return NULL;
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
