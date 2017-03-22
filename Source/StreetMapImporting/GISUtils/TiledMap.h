#pragma once

class FTiledMap
{
private:

	FTiledMap() {};

public:

	struct FBounds
	{
		double MinX;
		double MinY;
		double MaxX;
		double MaxY;
	};

	uint32 TileWidth;
	uint32 TileHeight;
	uint32 NumLevels;
	FBounds Bounds;
	FString URLTemplate;
	
	static FTiledMap MapzenElevation()
	{
		FTiledMap TiledMap;
		TiledMap.TileWidth = 256;
		TiledMap.TileHeight = 256;
		TiledMap.NumLevels = 15;
		TiledMap.Bounds.MinX = -20037508.34;
		TiledMap.Bounds.MinY =  20037508.34;
		TiledMap.Bounds.MaxX =  20037508.34;
		TiledMap.Bounds.MaxY = -20037508.34;
		TiledMap.URLTemplate = TEXT("http://s3.amazonaws.com/elevation-tiles-prod/terrarium/%d/%d/%d.png");
		return TiledMap;
	}

	FIntPoint GetTileXY(double X, double Y, uint32 LevelIndex) const
	{
		const double RelativeX = (X - Bounds.MinX) / (Bounds.MaxX - Bounds.MinX);
		const double RelativeY = (Y - Bounds.MinY) / (Bounds.MaxY - Bounds.MinY);
		const uint32 NumTiles = 1 << LevelIndex;
		return FIntPoint((int32)(RelativeX * NumTiles), (int32)(RelativeY * NumTiles));
	}

	FIntPoint GetTileXY(double X, double Y, uint32 LevelIndex, FVector2D& OutPixelXY) const
	{
		const double RelativeX = (X - Bounds.MinX) / (Bounds.MaxX - Bounds.MinX);
		const double RelativeY = (Y - Bounds.MinY) / (Bounds.MaxY - Bounds.MinY);
		const uint32 NumTiles = 1 << LevelIndex;
		const double AbsoluteX = RelativeX * NumTiles;
		const double AbsoluteY = RelativeY * NumTiles;
		const FIntPoint TileXY = FIntPoint((int32)AbsoluteX, (int32)AbsoluteY);

		OutPixelXY.X = (AbsoluteX - TileXY.X) * TileWidth;
		OutPixelXY.Y = (AbsoluteY - TileXY.Y) * TileHeight;

		return TileXY;
	}
};



