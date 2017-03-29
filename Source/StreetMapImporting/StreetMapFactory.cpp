// Copyright 2017 Mike Fricker. All Rights Reserved.

#include "StreetMapImporting.h"
#include "StreetMapFactory.h"
#include "OSMFile.h"
#include "StreetMap.h"


UStreetMapFactory::UStreetMapFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UStreetMap::StaticClass();

	Formats.Add( TEXT( "osm;OpenStreetMap XML" ) );
	bCreateNew = false;
	bEditorImport = true;
	bEditAfterNew = false;
	bText = true;
}


UObject* UStreetMapFactory::FactoryCreateText( UClass* Class, UObject* Parent, FName Name, EObjectFlags Flags, UObject* Context, const TCHAR* Type, const TCHAR*& Buffer, const TCHAR* BufferEnd, FFeedbackContext* Warn )
{
	UStreetMap* StreetMap = NewObject<UStreetMap>( Parent, Name, Flags | RF_Transactional );

	StreetMap->AssetImportData->Update( this->GetCurrentFilename() );

	// @todo: Performance: This will copy the entire text buffer into an FString.  We need to do this
	//        because the FFastXml parser is expecting a buffer that it can mutate as it parses.
	const int32 CharacterCount = BufferEnd - Buffer;
	FString MutableTextBuffer( CharacterCount, Buffer );

	const bool bIsFilePathActuallyTextBuffer = true;
	const bool bLoadedOkay = LoadFromOpenStreetMapXMLFile( StreetMap, MutableTextBuffer, bIsFilePathActuallyTextBuffer, Warn );

	if( !bLoadedOkay )
	{
		StreetMap->MarkPendingKill();
		StreetMap = nullptr;
	}

	return StreetMap;
}


bool UStreetMapFactory::LoadFromOpenStreetMapXMLFile( UStreetMap* StreetMap, FString& OSMFilePath, const bool bIsFilePathActuallyTextBuffer, FFeedbackContext* FeedbackContext )
{
	// OSM data is stored in meters.  This is the scale factor to convert those units into UE4's native units (cm)
	// Keep in mind that if this is changed, UStreetMapComponent sizes for roads may need to be updated too!
	// @todo: We should make this scale factor customizable as an import option
	const float OSMToCentimetersScaleFactor = 100.0f;

	// Adds a road to the street map using the OpenStreetMap data, flattening the road's coordinates into our map's space
	auto AddRoadForWay = [OSMToCentimetersScaleFactor](
		const FOSMFile& OSMFile, 
		UStreetMap& StreetMapRef, 
		const FOSMFile::FOSMWayInfo& OSMWay, 
		int32& OutRoadIndex ) -> bool
	{
		EStreetMapRoadType RoadType = EStreetMapRoadType::Other;
		if ((OSMWay.Category == TEXT("residential")) || // ~32% of all highways
			(OSMWay.Category == TEXT("service")) ||		// ~15% of all highways
			(OSMWay.Category == TEXT("unclassified")) ||
			(OSMWay.Category == TEXT("road"))) // @todo: Consider excluding "Road" from our data set, as it could be a highway that wasn't properly tagged in OSM yet
			RoadType = EStreetMapRoadType::Street;
		else if (
			(OSMWay.Category == TEXT("tertiary")) ||	// ~4% of all highways
			(OSMWay.Category == TEXT("secondary")) ||	// ~2% of all highways
			(OSMWay.Category == TEXT("secondary_link")) ||
			(OSMWay.Category == TEXT("tertiary_link")))
			RoadType = EStreetMapRoadType::MajorRoad;
		else if( 
			(OSMWay.Category == TEXT("primary")) || // ~2% of all highways
			(OSMWay.Category == TEXT("primary_link")) ||
			(OSMWay.Category == TEXT("motorway")) ||
			(OSMWay.Category == TEXT("motorway_link")) ||
			(OSMWay.Category == TEXT("trunk")) ||
			(OSMWay.Category == TEXT("trunk_link")))
			RoadType = EStreetMapRoadType::Highway;

		if (RoadType == EStreetMapRoadType::Other)
		{
			// There are other types that we don't recognize yet.  See http://wiki.openstreetmap.org/wiki/Key:highway
			return false;
		}

		// Require at least two points!
		if (OSMWay.Nodes.Num() < 2)
		{
			// NOTE: Skipped adding road for way because it has less than 2 points
			// @todo: Log this for the user as an import warning
			return false;
		}

		// Create a road for this way
		OutRoadIndex = StreetMapRef.Roads.Num();
		FStreetMapRoad& NewRoad = *new( StreetMapRef.Roads )FStreetMapRoad();

		FVector2D BoundsMin( TNumericLimits<float>::Max(), TNumericLimits<float>::Max() );
		FVector2D BoundsMax( TNumericLimits<float>::Lowest(), TNumericLimits<float>::Lowest() );

		NewRoad.RoadPoints.AddUninitialized( OSMWay.Nodes.Num() );
		int32 CurRoadPoint = 0;

		// Set defaults for each node index on this road.  INDEX_NONE means the node is not valid, which may be the case
		// for nodes that we filter out entirely.  This will be filled in by valid indices to nodes later on.
		NewRoad.NodeIndices.AddUninitialized( OSMWay.Nodes.Num() );
		for( int32& NodeIndex : NewRoad.NodeIndices )
		{
			NodeIndex = INDEX_NONE;
		}


		for( const FOSMFile::FOSMNodeInfo* OSMNodePtr : OSMWay.Nodes )
		{
			const FOSMFile::FOSMNodeInfo& OSMNode = *OSMNodePtr;
			const FVector2D NodePos = OSMFile.SpatialReferenceSystem.FromEPSG4326(OSMNode.Longitude, OSMNode.Latitude) * OSMToCentimetersScaleFactor;

			// Update bounding box
			{
				if( NodePos.X < BoundsMin.X )
				{
					BoundsMin.X = NodePos.X;
				}
				if( NodePos.Y < BoundsMin.Y )
				{
					BoundsMin.Y = NodePos.Y;
				}
				if( NodePos.X > BoundsMax.X )
				{
					BoundsMax.X = NodePos.X;
				}
				if( NodePos.Y > BoundsMax.Y )
				{
					BoundsMax.Y = NodePos.Y;
				}
			}

			// Fill in the points
			NewRoad.RoadPoints[ CurRoadPoint++ ] = NodePos;
		}


		NewRoad.RoadName = OSMWay.Name;
		if( NewRoad.RoadName.IsEmpty() )
		{
			NewRoad.RoadName = OSMWay.Ref;
		}
		NewRoad.RoadType = RoadType;
		NewRoad.BoundsMin = BoundsMin;
		NewRoad.BoundsMax = BoundsMax;

		NewRoad.bIsOneWay = OSMWay.bIsOneWay;

		StreetMapRef.BoundsMin.X = FMath::Min( StreetMapRef.BoundsMin.X, BoundsMin.X );
		StreetMapRef.BoundsMin.Y = FMath::Min( StreetMapRef.BoundsMin.Y, BoundsMin.Y );
		StreetMapRef.BoundsMax.X = FMath::Max( StreetMapRef.BoundsMax.X, BoundsMax.X );
		StreetMapRef.BoundsMax.Y = FMath::Max( StreetMapRef.BoundsMax.Y, BoundsMax.Y );

		return true;
	};


	// Adds a building to the street map using the OpenStreetMap data, flattening the road's coordinates into our map's space
	auto AddBuildingForWay = [OSMToCentimetersScaleFactor]( 
		const FOSMFile& OSMFile, 
		UStreetMap& StreetMapRef, 
		const FOSMFile::FOSMWayInfo& OSMWay ) -> bool
	{
		// Require at least three points so that we don't have degenerate polygon!
		if (OSMWay.Nodes.Num() < 3)
		{
			// NOTE: Skipped adding building for way because it has less than 3 points
			// @todo: Log this for the user as an import warning
			return false;
		}

		// Create a building for this way
		FStreetMapBuilding& NewBuilding = *new( StreetMapRef.Buildings )FStreetMapBuilding();

		FVector2D BoundsMin( TNumericLimits<float>::Max(), TNumericLimits<float>::Max() );
		FVector2D BoundsMax( TNumericLimits<float>::Lowest(), TNumericLimits<float>::Lowest() );

		NewBuilding.BuildingPoints.AddUninitialized( OSMWay.Nodes.Num() );
		int32 CurBuildingPoint = 0;

		for( const FOSMFile::FOSMNodeInfo* OSMNodePtr : OSMWay.Nodes )
		{
			const FOSMFile::FOSMNodeInfo& OSMNode = *OSMNodePtr;
			const FVector2D NodePos = OSMFile.SpatialReferenceSystem.FromEPSG4326(OSMNode.Longitude, OSMNode.Latitude) * OSMToCentimetersScaleFactor;

			// Update bounding box
			{
				if( NodePos.X < BoundsMin.X )
				{
					BoundsMin.X = NodePos.X;
				}
				if( NodePos.Y < BoundsMin.Y )
				{
					BoundsMin.Y = NodePos.Y;
				}
				if( NodePos.X > BoundsMax.X )
				{
					BoundsMax.X = NodePos.X;
				}
				if( NodePos.Y > BoundsMax.Y )
				{
					BoundsMax.Y = NodePos.Y;
				}
			}

			// Fill in the points
			NewBuilding.BuildingPoints[ CurBuildingPoint++ ] = NodePos;
		}

		// Make sure the building ended up with a closed polygon, then remove the final (redundant) point
		const bool bIsClosed = NewBuilding.BuildingPoints[ 0 ].Equals( NewBuilding.BuildingPoints[ NewBuilding.BuildingPoints.Num() - 1 ], KINDA_SMALL_NUMBER );
		if( bIsClosed )
		{
			// Remove the final redundant point
			NewBuilding.BuildingPoints.Pop();
		}
		else
		{
			// Wasn't expecting to have an unclosed shape.  Our tolerances might be off, or the data was malformed.
			// Either way, it shouldn't be a problem as we'll close the shape ourselves below.
			// @todo: Log this for the user as an import warning
		}

		NewBuilding.BuildingName = OSMWay.Name;
		if( NewBuilding.BuildingName.IsEmpty() )
		{
			NewBuilding.BuildingName = OSMWay.Ref;
		}

		NewBuilding.Height = OSMWay.Height * OSMToCentimetersScaleFactor;
		NewBuilding.BuildingLevels = OSMWay.BuildingLevels;

		NewBuilding.BoundsMin = BoundsMin;
		NewBuilding.BoundsMax = BoundsMax;

		StreetMapRef.BoundsMin.X = FMath::Min( StreetMapRef.BoundsMin.X, BoundsMin.X );
		StreetMapRef.BoundsMin.Y = FMath::Min( StreetMapRef.BoundsMin.Y, BoundsMin.Y );
		StreetMapRef.BoundsMax.X = FMath::Max( StreetMapRef.BoundsMax.X, BoundsMax.X );
		StreetMapRef.BoundsMax.Y = FMath::Max( StreetMapRef.BoundsMax.Y, BoundsMax.Y );

		return true;
	};

	// Adds a remaining recognized ways to the street map using the OpenStreetMap data
	auto AddMiscWay = [OSMToCentimetersScaleFactor](
		const FOSMFile& OSMFile,
		UStreetMap& StreetMapRef,
		const FOSMFile::FOSMWayInfo& OSMWay) -> bool
	{
		if (OSMWay.WayType == FOSMFile::EOSMWayType::Other)
		{
			return false;
		}

		// Create a MiscWay for this way
		FStreetMapMiscWay& NewMiscWay = *new(StreetMapRef.MiscWays)FStreetMapMiscWay();

		FVector2D BoundsMin(TNumericLimits<float>::Max(), TNumericLimits<float>::Max());
		FVector2D BoundsMax(TNumericLimits<float>::Lowest(), TNumericLimits<float>::Lowest());

		NewMiscWay.Points.AddUninitialized(OSMWay.Nodes.Num());
		int32 CurBuildingPoint = 0;

		for (const FOSMFile::FOSMNodeInfo* OSMNodePtr : OSMWay.Nodes)
		{
			const FOSMFile::FOSMNodeInfo& OSMNode = *OSMNodePtr;
			const FVector2D NodePos = OSMFile.SpatialReferenceSystem.FromEPSG4326(OSMNode.Longitude, OSMNode.Latitude) * OSMToCentimetersScaleFactor;

			// Update bounding box
			{
				if (NodePos.X < BoundsMin.X)
				{
					BoundsMin.X = NodePos.X;
				}
				if (NodePos.Y < BoundsMin.Y)
				{
					BoundsMin.Y = NodePos.Y;
				}
				if (NodePos.X > BoundsMax.X)
				{
					BoundsMax.X = NodePos.X;
				}
				if (NodePos.Y > BoundsMax.Y)
				{
					BoundsMax.Y = NodePos.Y;
				}
			}

			// Fill in the points
			NewMiscWay.Points[CurBuildingPoint++] = NodePos;
		}

		// Test if the building ended up with a closed polygon, then remove the final (redundant) point
		const bool bIsClosed = NewMiscWay.Points[0].Equals(NewMiscWay.Points[NewMiscWay.Points.Num() - 1], KINDA_SMALL_NUMBER);
		if (bIsClosed)
		{
			// Remove the final redundant point
			NewMiscWay.Points.Pop();
		}
		else
		{
			// Unclosed shapes are total fine (e.g. tree_row)
		}


		NewMiscWay.Type = EStreetMapMiscWayType::Unknown;
		switch (OSMWay.WayType)
		{
			case FOSMFile::EOSMWayType::Leisure: NewMiscWay.Type = EStreetMapMiscWayType::Leisure; break;
			case FOSMFile::EOSMWayType::Natural: NewMiscWay.Type = EStreetMapMiscWayType::Natural; break;
			case FOSMFile::EOSMWayType::LandUse: NewMiscWay.Type = EStreetMapMiscWayType::LandUse; break;
		}

		NewMiscWay.Name = OSMWay.Name;
		if (NewMiscWay.Name.IsEmpty())
		{
			NewMiscWay.Name = OSMWay.Ref;
		}

		NewMiscWay.Category = OSMWay.Category;

		NewMiscWay.BoundsMin = BoundsMin;
		NewMiscWay.BoundsMax = BoundsMax;
		NewMiscWay.bIsClosed = bIsClosed;

		StreetMapRef.BoundsMin.X = FMath::Min(StreetMapRef.BoundsMin.X, BoundsMin.X);
		StreetMapRef.BoundsMin.Y = FMath::Min(StreetMapRef.BoundsMin.Y, BoundsMin.Y);
		StreetMapRef.BoundsMax.X = FMath::Max(StreetMapRef.BoundsMax.X, BoundsMax.X);
		StreetMapRef.BoundsMax.Y = FMath::Max(StreetMapRef.BoundsMax.Y, BoundsMax.Y);

		return true;
	};


	// Load up the OSM file.  It's in XML format.
	FOSMFile OSMFile;
	if( !OSMFile.LoadOpenStreetMapFile( OSMFilePath, bIsFilePathActuallyTextBuffer, FeedbackContext ) )
	{
		// Loading failed.  The actual error message will be sent to the FeedbackContext's log.
		return false;
	}

	StreetMap->OriginLongitude = OSMFile.SpatialReferenceSystem.GetOriginLongitude();
	StreetMap->OriginLatitude = OSMFile.SpatialReferenceSystem.GetOriginLatitude();

	// @todo: The loaded OSMFile stores data in double precision, but our runtime representation (UStreetMap)
	//        truncates everything to single precision, after transposing coordinates to be relative to the
	//        center of the map's 2D bounds.  Large maps will suffer from floating point precision issues.
	//        To solve this we'd need to either store everything in double precision, or store map elements
	//        in integral grid cells with coordinates relative to their cell.  Of course, there will be many
	//        other considerations for handling huge maps (loading, rendering, collision, etc.)

	// Maps OSMWayInfos to the RoadIndex we created for that way
	TMap< const FOSMFile::FOSMWayInfo*, int32 > OSMWayToRoadIndexMap;

	StreetMap->BoundsMin = FVector2D( TNumericLimits<float>::Max(), TNumericLimits<float>::Max() );
	StreetMap->BoundsMax = FVector2D( TNumericLimits<float>::Lowest(), TNumericLimits<float>::Lowest() );

	for( const FOSMFile::FOSMWayInfo* OSMWay : OSMFile.Ways )
	{
		// Handle buildings differently than roads
		if( OSMWay->WayType == FOSMFile::EOSMWayType::Building )
		{
			if( AddBuildingForWay( OSMFile, *StreetMap, *OSMWay ) )
			{
				// ...
			}
		}
		else if( OSMWay->WayType == FOSMFile::EOSMWayType::Highway )
		{
			int32 RoadIndex = INDEX_NONE;
			if( AddRoadForWay( OSMFile, *StreetMap, *OSMWay, RoadIndex ) )
			{
				OSMWayToRoadIndexMap.Add( OSMWay, RoadIndex );
			}
		}
		else if (AddMiscWay(OSMFile, *StreetMap, *OSMWay))
		{
			// ...
		}
	}

	for( const auto& NodeMapHashPair : OSMFile.NodeMap )
	{
		const FOSMFile::FOSMNodeInfo& OSMNode = *NodeMapHashPair.Value;

		// Any ways touching this node?
		if( OSMNode.WayRefs.Num() > 0 )
		{
			FStreetMapNode NewNode;

			for( const FOSMFile::FOSMWayRef& OSMWayRef : OSMNode.WayRefs )
			{
				const int32* FoundRoundIndexPtr = OSMWayToRoadIndexMap.Find( OSMWayRef.Way );
				if( FoundRoundIndexPtr != nullptr )
				{
					const int32 FoundRoadIndex = *FoundRoundIndexPtr;

					FStreetMapRoadRef RoadRef;
					RoadRef.RoadIndex = FoundRoadIndex;

					const int32 RoadPointIndex = OSMWayRef.NodeIndex;
					RoadRef.RoadPointIndex = RoadPointIndex;
					NewNode.RoadRefs.Add( RoadRef );
				}
				else
				{
					// Skipped ref because we didn't keep this road in our data set							
				}
			}

			// Only store nodes that are attached to at least one road.  We must have at least a connection to a single
			// road, otherwise we've filtered this node's road out and there's no point in wasting memory on the node itself.
			if( NewNode.RoadRefs.Num() > 0 )
			{
				// Most nodes from OpenStreetMap will only be touching a single road.  These nodes usually make up the points
				// along the length of the road, even for roads with no intersections except at the beginning and end.  We
				// don't need to store these points unless they are at the ends of the road.  Keeping the points at the
				// beginning and end of the road is useful when calculating navigation data, but the other nodes can go!
				// In the road's NodeIndices array, any nodes we filter out here will simply have an INDEX_NONE value in that
				// array, and we'll only store the positions of the road at these points in the road's RoadPoints array.

				const FStreetMapRoadRef& FirstRoadRef = NewNode.RoadRefs[ 0 ];
				const FStreetMapRoad& FirstRoad = StreetMap->Roads[ FirstRoadRef.RoadIndex ];

				if( NewNode.RoadRefs.Num() > 1 ||					// Does the node connect to more than one road?
					FirstRoadRef.RoadPointIndex == 0 ||				// Does the node connect to the beginning of the road?
					FirstRoadRef.RoadPointIndex == ( FirstRoad.NodeIndices.Num() - 1 ) )	// Does the node connect to the end of the road?
				{
					const int32 NewNodeIndex = StreetMap->Nodes.Num();
					StreetMap->Nodes.Add( NewNode );

					// Update the roads that are overlapping this node
					for( const FStreetMapRoadRef& RoadRef : NewNode.RoadRefs )
					{
						FStreetMapRoad& Road = StreetMap->Roads[ RoadRef.RoadIndex ];
						check( Road.NodeIndices[ RoadRef.RoadPointIndex ] == INDEX_NONE );
						Road.NodeIndices[ RoadRef.RoadPointIndex ] = NewNodeIndex;
					}
				}
				else
				{
					// Node has only one road that is references, and it wasn't the beginning or end of the road, so filter it out!
				}
			}
			else
			{
				// Node doesn't reference any roads that we kept, or the data was malformed.  Filter it out.
			}
		}
	}

	// Validation test: Make sure that all roads have at least two nodes referencing them, one at the beginning and
	// one at the end.
	for( const FStreetMapRoad& Road : StreetMap->Roads )
	{
		const bool bHasNodeAtBeginning = Road.NodeIndices[ 0 ] != INDEX_NONE;
		const bool bHasNodeAtEnd = Road.NodeIndices[ Road.NodeIndices.Num() - 1 ] != INDEX_NONE;

		// All roads should have at least two nodes referencing them, one at the beginning and one at the end
		ensure( bHasNodeAtBeginning && bHasNodeAtEnd );
	}

	return true;
}


