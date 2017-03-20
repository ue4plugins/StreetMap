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
	
	static FTiledMap MapzenElevation()
	{
		FTiledMap TiledMap;
		TiledMap.TileWidth = 256;
		TiledMap.TileHeight = 256;
		TiledMap.NumLevels = 15;
		TiledMap.Bounds.MinX = -20037508.34;
		TiledMap.Bounds.MinY = -20037508.34;
		TiledMap.Bounds.MaxX =  20037508.34;
		TiledMap.Bounds.MaxY =  20037508.34;
		return TiledMap;
	}

	FIntPoint GetTileXY(double X, double Y, uint32 LevelIndex) const
	{
		const double RelativeX = (X - Bounds.MinX) / (Bounds.MaxX - Bounds.MinX);
		const double RelativeY = (Y - Bounds.MinY) / (Bounds.MaxY - Bounds.MinY);
		const uint32 NumTiles = 1 << LevelIndex;
		return FIntPoint((int32)(RelativeX * NumTiles), (int32)(RelativeY * NumTiles));
	}
};



