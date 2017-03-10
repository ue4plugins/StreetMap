// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "StreetMapRuntime.h"
#include "StreetMapActor.h"
#include "StreetMapComponent.h"
#include "Landscape.h"

#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "HttpManager.h"
#include "Misc/ScopedSlowTask.h"

#define LOCTEXT_NAMESPACE "StreetMap"

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

	void OnDownloadSucceeded(FHttpResponsePtr Response)
	{
		// unpack data
		TArray<uint8> Out;
		if (Response.IsValid())
		{
			Out.Append(Response->GetContent());
		}

		// TODO: write data to cache

		WasDownloadASuccess = true;
	}

	void DownloadFile()
	{
		FString URL = FString::Printf(*URLTemplate, Z, X, Y);

		HttpRequest = FHttpModule::Get().CreateRequest();
		HttpRequest->SetVerb(TEXT("GET"));

		HttpRequest->SetURL(URL);
		if(!HttpRequest->ProcessRequest())
		{
			Failed = true;
		}
	}

public:

	bool HasFinished() const
	{
		return WasDownloadASuccess || Failed;
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

AStreetMapActor::AStreetMapActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	StreetMapComponent = CreateDefaultSubobject<UStreetMapComponent>(TEXT("StreetMapComp"));
	RootComponent = StreetMapComponent;

	UWorld* const World = GetWorld();
	if (World && !Landscape)
	{
		Landscape = World->SpawnActor<ALandscape>(ALandscape::StaticClass());

		FScopedSlowTask SlowTask(2.0f, LOCTEXT("GeneratingLandscape", "Generating Landscape"));
		SlowTask.MakeDialog(true);

		// TODO: download correct data

		TArray<TSharedPtr<FCachedElevationFile>> FilesToDownload;
		FilesToDownload.Add(MakeShared<FCachedElevationFile>(1, 1, 2));
		FilesToDownload.Add(MakeShared<FCachedElevationFile>(2, 1, 2));
		FilesToDownload.Add(MakeShared<FCachedElevationFile>(1, 2, 2));
		FilesToDownload.Add(MakeShared<FCachedElevationFile>(2, 2, 2));

		TArray<TSharedPtr<FCachedElevationFile>> FilesDownloaded;
		const uint32 NumFilesToDownload = FilesToDownload.Num();
		while (FilesToDownload.Num())
		{
			FHttpModule::Get().GetHttpManager().Tick(0);

			if (GWarn->ReceivedUserCancel())
			{
				break;
			}

			float progress = 0.0f;
			for (auto FileToDownload : FilesToDownload)
			{
				FileToDownload->Tick();

				if (FileToDownload->HasFinished())
				{
					FilesToDownload.Remove(FileToDownload);
					FilesDownloaded.Add(FileToDownload);
					progress = 1.0f / (float)NumFilesToDownload;
					break;
				}
			}

			FFormatNamedArguments Arguments;
			Arguments.Add(TEXT("NumFilesDownloaded"), FText::AsNumber(FilesDownloaded.Num()));
			Arguments.Add(TEXT("NumFilesToDownload"), FText::AsNumber(NumFilesToDownload));
			SlowTask.EnterProgressFrame(progress, FText::Format(LOCTEXT("DownloadingElevationModel", "Downloading Elevation Model ({NumFilesDownloaded} of {NumFilesToDownload})"), Arguments));

			FPlatformProcess::Sleep(0.1f);
		}

		/*Landscape->Import(
			const FGuid Guid,
			const int32 MinX, const int32 MinY, const int32 MaxX, const int32 MaxY,
			const int32 InNumSubsections, const int32 InSubsectionSizeQuads,
			const uint16* const HeightData, const TCHAR* const HeightmapFileName,
			const TArray<FLandscapeImportLayerInfo>& ImportLayerInfos, const ELandscapeImportAlphamapType ImportLayerType);
		*/
	}
}