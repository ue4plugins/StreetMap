#pragma once

class FTiledMap
{
private:
	struct FBounds
	{
		double MinX;
		double MinY;
		double MaxX;
		double MaxY;
	};

	int TileWidth;
	int TileHeight;
	int NumLevels;
	FBounds Bounds;


	FTiledMap() {};

public:
	
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
	}


};



