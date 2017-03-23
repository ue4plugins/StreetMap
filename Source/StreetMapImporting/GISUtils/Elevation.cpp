#include "StreetMapImporting.h"
#include "StreetMapComponent.h"
#include "Elevation.h"

#include "DesktopPlatformModule.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "HttpManager.h"
#include "Misc/ScopedSlowTask.h"
#include "Interfaces/IImageWrapperModule.h"
#include "SNotificationList.h"
#include "NotificationManager.h"
#include "ScopedTransaction.h"
#include "SpatialReferenceSystem.h"
#include "TiledMap.h"

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

class FCachedElevationFile
{
private:
	const int32 MaxNumPendingDownloads = 10;
	static int32 NumPendingDownloads;

	const FTiledMap& TiledMap;

	bool WasInitialized;
	bool WasDownloadASuccess;
	bool Failed;

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
			const ERGBFormat::Type Format = PngImageWrapper->GetFormat();
			const int32 Width = PngImageWrapper->GetWidth();
			const int32 Height = PngImageWrapper->GetHeight();

			if (Width != TiledMap.TileWidth || Height != TiledMap.TileHeight)
			{
				GWarn->Logf(ELogVerbosity::Error, TEXT("PNG file has wrong dimensions. Expected %dx%d"), TiledMap.TileWidth, TiledMap.TileHeight);
				return false;
			}

			if ((Format != ERGBFormat::RGBA) || BitDepth != 8)
			{
				GWarn->Logf(ELogVerbosity::Error, TEXT("PNG file contains elevation data in an unsupported format."));
				return false;
			}

			const TArray<uint8>* RawPNG = nullptr;
			if (PngImageWrapper->GetRaw(Format, BitDepth, RawPNG))
			{
				const uint8* Data = RawPNG->GetData();
				Elevation.SetNumUninitialized(Width * Height);
				float* ElevationData = Elevation.GetData();
				const float* ElevationDataEnd = ElevationData + (Width * Height);

				while (ElevationData < ElevationDataEnd)
				{
					float ElevationValue = (Data[0] * 256.0f + Data[1] + Data[2] / 256.0f);

					const bool IsValid = (ElevationValue > 0.0f) && (ElevationValue < 41768.0f); // smaller than Mount Everest?
					if (IsValid)
					{
						ElevationValue -= 32768.0f;
						ElevationMin = FMath::Min(ElevationMin, ElevationValue);
						ElevationMax = FMath::Max(ElevationMax, ElevationValue);
					}

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
		NumPendingDownloads--;

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
		FString URL = FString::Printf(*TiledMap.URLTemplate, Z, X, Y);

		HttpRequest = FHttpModule::Get().CreateRequest();
		HttpRequest->SetVerb(TEXT("GET"));

		HttpRequest->SetURL(URL);
		if (HttpRequest->ProcessRequest())
		{
			NumPendingDownloads++;
		}
		else
		{
			Failed = true;
		}
	}

	void Initialize()
	{
		WasInitialized = true;
		StartTime = FDateTime::UtcNow();

		// try to load data from cache first
		{
			TArray<uint8> RawData;
			if (FFileHelper::LoadFileToArray(RawData, *GetCachedFilePath(X, Y, Z), FILEREAD_Silent))
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

protected:
	TArray<float> Elevation;
	uint32 X, Y, Z;

	float ElevationMin;
	float ElevationMax;

	friend class FElevationModel;

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
		NumPendingDownloads--;
		HttpRequest->CancelRequest();
	}

	void Tick()
	{
		if (!WasInitialized)
		{
			if (NumPendingDownloads >= MaxNumPendingDownloads)
			{
				return;
			}

			Initialize();
		}

		if (WasDownloadASuccess || Failed) return;

		if (TimeSpan.GetSeconds() > 10)
		{
			GWarn->Logf(ELogVerbosity::Error, TEXT("Download time-out. Check your internet connection!"));
			NumPendingDownloads--;
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
			NumPendingDownloads--;
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

	FCachedElevationFile(const FTiledMap& TiledMap, uint32 X, uint32 Y, uint32 Z)
		: TiledMap(TiledMap)
		, WasInitialized(false)
		, WasDownloadASuccess(false)
		, Failed(false)
		, StartTime(FDateTime::UtcNow())
		, X(X)
		, Y(Y)
		, Z(Z)
		, ElevationMin(TNumericLimits<float>::Max())
		, ElevationMax(TNumericLimits<float>::Lowest())
	{
	}
};

int32 FCachedElevationFile::NumPendingDownloads = 0;

static int32 GetNumVerticesForRadius(const FStreetMapLandscapeBuildSettings& BuildSettings)
{
	return FMath::RoundToInt(BuildSettings.Radius / BuildSettings.QuadSize);
}

class FElevationModel
{
public:

	FElevationModel(const FTiledMap& TiledMap)
		: TiledMap(TiledMap)
		, ElevationMin(TNumericLimits<float>::Max())
		, ElevationMax(TNumericLimits<float>::Lowest())
	{
	}

	const FTransform& GetTransform() const
	{
		return Transform;
	}

	bool LoadElevationData(UStreetMapComponent* StreetMapComponent, const FStreetMapLandscapeBuildSettings& BuildSettings, FScopedSlowTask& SlowTask)
	{
		TArray<TSharedPtr<FCachedElevationFile>> FilesToDownload;

		// 1.) collect all elevation tiles needed based on StreetMap location and Landscape size
		{
			const UStreetMap* StreetMap = StreetMapComponent->GetStreetMap();
			const FSpatialReferenceSystem SRS(StreetMap->GetOriginLongitude(), StreetMap->GetOriginLatitude());

			const FVector2D SouthWest(-BuildSettings.Radius,  BuildSettings.Radius);
			const FVector2D NorthEast( BuildSettings.Radius, -BuildSettings.Radius);
			double South, West, North, East;
			if (!SRS.ToEPSG3857(SouthWest, West, South) || !SRS.ToEPSG3857(NorthEast, East, North))
			{
				ShowErrorMessage(LOCTEXT("ElevationBoundsInvalid", "Chosen elevation bounds are invalid. Stay within WebMercator bounds!"));
				return false;
			}

			// download highest resolution available
			const uint32 LevelIndex = TiledMap.NumLevels - 1;
			const FIntPoint SouthWestTileXY = TiledMap.GetTileXY(West, South, LevelIndex);
			const FIntPoint NorthEastTileXY = TiledMap.GetTileXY(East, North, LevelIndex);

			// since we may not know the direction of tile order the source uses we need to order them
			const int32 MinX = FMath::Min(SouthWestTileXY.X, NorthEastTileXY.X);
			const int32 MinY = FMath::Min(SouthWestTileXY.Y, NorthEastTileXY.Y);
			const int32 MaxX = FMath::Max(SouthWestTileXY.X, NorthEastTileXY.X);
			const int32 MaxY = FMath::Max(SouthWestTileXY.Y, NorthEastTileXY.Y);

			for (int32 Y = MinY; Y <= MaxY; Y++)
			{
				for (int32 X = MinX; X <= MaxX; X++)
				{
					FilesToDownload.Add(MakeShared<FCachedElevationFile>(TiledMap, X, Y, LevelIndex));
				}
			}
		}

		// 2.) download the data from web service or disk if already cached
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

						ElevationMin = FMath::Min(ElevationMin, FileToDownload->ElevationMin);
						ElevationMax = FMath::Max(ElevationMax, FileToDownload->ElevationMax);
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

			if (Progress == 0.0f)
			{
				FPlatformProcess::Sleep(0.1f);
			}
		}

		if (FilesDownloaded.Num() < NumFilesToDownload)
		{
			ShowErrorMessage(LOCTEXT("DownloadElevationFailed", "Could not download all necessary elevation model files. See Log for details!"));
			return false;
		}

		return true;
	}

	void ReprojectData(UStreetMapComponent* StreetMapComponent, const FStreetMapLandscapeBuildSettings& BuildSettings, FScopedSlowTask& SlowTask, TArray<uint16>& OutElevationData)
	{
		SlowTask.EnterProgressFrame(0.0f, LOCTEXT("ReprojectingElevationModel", "Reprojecting Elevation Model"));

		const UStreetMap* StreetMap = StreetMapComponent->GetStreetMap();
		const FSpatialReferenceSystem SRS(StreetMap->GetOriginLongitude(), StreetMap->GetOriginLatitude());

		const uint32 LevelIndex = TiledMap.NumLevels - 1;
		const int32 NumVerticesForRadius = GetNumVerticesForRadius(BuildSettings);
		const int32 Size = NumVerticesForRadius * 2;
		const float ElevationRange = ElevationMax - ElevationMin;
		const float ElevationScale = 65535.0f / ElevationRange;

		// sample elevation value for each height map vertex
		OutElevationData.SetNumUninitialized(Size * Size);
		uint16* Elevation = OutElevationData.GetData();
		for (int32 Y = -NumVerticesForRadius; Y < NumVerticesForRadius; Y++)
		{
			for (int32 X = -NumVerticesForRadius; X < NumVerticesForRadius; X++)
			{
				uint16 QuantizedElevation = 32768;

				double WebMercatorX, WebMercatorY;
				FVector2D VertexLocation(X * BuildSettings.QuadSize, Y * BuildSettings.QuadSize);
				if (SRS.ToEPSG3857(VertexLocation, WebMercatorX, WebMercatorY))
				{
					FVector2D PixelXY;
					const auto TileXY = TiledMap.GetTileXY(WebMercatorX, WebMercatorY, LevelIndex, PixelXY);
					const auto Tile = GetTile(TileXY, LevelIndex);

					const float* ElevationData = Tile->Elevation.GetData();

					// @todo: sample elevation using Lanczos filtering

					int32 ElevationX = (int32)PixelXY.X;
					int32 ElevationY = (int32)PixelXY.Y;
					float ElevationValue = ElevationData[TiledMap.TileWidth * ElevationY + ElevationX];
					float ScaledElevationValue = (ElevationValue - ElevationMin) * ElevationScale;

					QuantizedElevation = (uint16)FMath::RoundToInt(ScaledElevationValue);
				}

				*Elevation = QuantizedElevation;
				Elevation++;
			}
		}

		// compute exact scale of landscape
		// Landscape docs say: At Z Scale = 100 landscape has an height range limit of -256m:256. 
		const float OSMToCentimetersScaleFactor = 100.0f;
		const float DefaultLandscapeScaleXY = 128.0f;
		const float DefaultLandscapeScaleZ = 256.0f;
		const float LandscapeInternalScaleZ = 512.0f / 100.0f;
		const float ScaleXY = OSMToCentimetersScaleFactor * BuildSettings.QuadSize / DefaultLandscapeScaleXY;
		const float ScaleZ = ElevationRange / DefaultLandscapeScaleZ / LandscapeInternalScaleZ;
		const auto Scale3D = FVector(ScaleXY, ScaleXY, ScaleZ);
		Transform.SetScale3D(Scale3D);
	}

private:
	const FTiledMap TiledMap;
	TArray<TSharedPtr<FCachedElevationFile>> FilesDownloaded;
	FTransform Transform;

	float ElevationMin;
	float ElevationMax;

	FCachedElevationFile* GetTile(const FIntPoint& XY, uint32 LevelIndex)
	{
		for (auto& Tile : FilesDownloaded)
		{
			if (Tile->X == XY.X && Tile->Y == XY.Y && Tile->Z == LevelIndex)
			{
				return Tile.Get();
			}
		}
		return nullptr;
	}
};


ALandscape* CreateLandscape(UWorld* World, const FStreetMapLandscapeBuildSettings& BuildSettings, const FTransform& Transform, const TArray<uint16>& ElevationData, FScopedSlowTask& SlowTask)
{
	SlowTask.EnterProgressFrame(0.0f, LOCTEXT("CreatingLandscape", "Filling Landscape with data"));

	FScopedTransaction Transaction(LOCTEXT("Undo", "Creating New Landscape"));

	ALandscape* Landscape = World->SpawnActor<ALandscape>(ALandscape::StaticClass(), Transform);

	const int32 NumVerticesForRadius = GetNumVerticesForRadius(BuildSettings);
	const int32 Size = NumVerticesForRadius * 2;

	// create import layers
	TArray<FLandscapeImportLayerInfo> ImportLayers;
	{
		const auto& ImportLandscapeLayersList = BuildSettings.Layers;
		ImportLayers.Reserve(ImportLandscapeLayersList.Num());

		// Fill in LayerInfos array and allocate data
		for (const FLandscapeImportLayerInfo& UIImportLayer : ImportLandscapeLayersList)
		{
			FLandscapeImportLayerInfo ImportLayer = FLandscapeImportLayerInfo(UIImportLayer.LayerName);
			ImportLayer.LayerInfo = UIImportLayer.LayerInfo;
			ImportLayer.SourceFilePath = "";
			ImportLayer.LayerData = TArray<uint8>();
			ImportLayers.Add(MoveTemp(ImportLayer));
		}

		// @todo: fill the blend weights based on land use
		// for now Fill the first weight-blended layer to 100%
		{
			ImportLayers[0].LayerData.SetNumUninitialized(Size * Size);

			uint8* ByteData = ImportLayers[0].LayerData.GetData();
			for (int32 i = 0; i < Size * Size; i++)
			{
				ByteData[i] = 255;
			}
		}
	}

	int32 SubsectionSizeQuads = FMath::RoundUpToPowerOfTwo(Size) / 32 - 1;

	Landscape->LandscapeMaterial = BuildSettings.Material;
	Landscape->Import(FGuid::NewGuid(), 
		-NumVerticesForRadius, -NumVerticesForRadius,
		NumVerticesForRadius - 1, NumVerticesForRadius - 1,
		2, SubsectionSizeQuads, ElevationData.GetData(), nullptr,
		ImportLayers, ELandscapeImportAlphamapType::Additive);

	// automatically calculate a lighting LOD that won't crash lightmass (hopefully)
	// < 2048x2048 -> LOD0
	// >=2048x2048 -> LOD1
	// >= 4096x4096 -> LOD2
	// >= 8192x8192 -> LOD3
	Landscape->StaticLightingLOD = FMath::DivideAndRoundUp(FMath::CeilLogTwo((Size * Size) / (2048 * 2048) + 1), (uint32)2);

	/*ULandscapeInfo* LandscapeInfo = Landscape->CreateLandscapeInfo();
	LandscapeInfo->UpdateLayerInfoMap(Landscape);

	// Import doesn't fill in the LayerInfo for layers with no data, do that now
	const TArray<FLandscapeImportLayerInfo>& ImportLandscapeLayersList = BuildSettings.Layers;
	for (int32 i = 0; i < ImportLandscapeLayersList.Num(); i++)
	{
		if (ImportLandscapeLayersList[i].LayerInfo != nullptr)
		{
			Landscape->EditorLayerSettings.Add(FLandscapeEditorLayerSettings(ImportLandscapeLayersList[i].LayerInfo));

			int32 LayerInfoIndex = LandscapeInfo->GetLayerInfoIndex(ImportLandscapeLayersList[i].LayerName);
			if (ensure(LayerInfoIndex != INDEX_NONE))
			{
				FLandscapeInfoLayerSettings& LayerSettings = LandscapeInfo->Layers[LayerInfoIndex];
				LayerSettings.LayerInfoObj = ImportLandscapeLayersList[i].LayerInfo;
			}
		}
	}*/

	return Landscape;
}


ALandscape* BuildLandscape(UStreetMapComponent* StreetMapComponent, UWorld* World, const FStreetMapLandscapeBuildSettings& BuildSettings)
{
	FScopedSlowTask SlowTask(2.0f, LOCTEXT("GeneratingLandscape", "Generating Landscape"));
	SlowTask.MakeDialog(true);

	FElevationModel ElevationModel(FTiledMap::MapzenElevation());
	if (!ElevationModel.LoadElevationData(StreetMapComponent, BuildSettings, SlowTask))
	{
		return nullptr;
	}

	TArray<uint16> ElevationData;
	ElevationModel.ReprojectData(StreetMapComponent, BuildSettings, SlowTask, ElevationData);

	return CreateLandscape(World, BuildSettings, ElevationModel.GetTransform(), ElevationData, SlowTask);
}
