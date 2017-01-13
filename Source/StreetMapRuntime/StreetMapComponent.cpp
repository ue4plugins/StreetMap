// Copyright 2017 Mike Fricker. All Rights Reserved.

#include "StreetMapRuntime.h"
#include "StreetMapComponent.h"
#include "StreetMap.h"
#include "StreetMapSceneProxy.h"
#include "Runtime/Engine/Classes/Engine/StaticMesh.h"
#include "Runtime/Engine/Public/StaticMeshResources.h"
#include "PolygonTools.h"


UStreetMapComponent::UStreetMapComponent( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer ),
	  CachedLocalBounds( FBox( 0 ) )
{
	SetCollisionProfileName( UCollisionProfile::NoCollision_ProfileName );

	// We don't currently need to be ticked.  This can be overridden in a derived class though.
	PrimaryComponentTick.bCanEverTick = false;
	this->bAutoActivate = false;	// NOTE: Components instantiated through C++ are not automatically active, so they'll only tick once and then go to sleep!

	// We don't currently need InitializeComponent() to be called on us.  This can be overridden in a
	// derived class though.
	bWantsInitializeComponent = false;

	// Turn on shadows.  It looks better.
	CastShadow = true;

	// Our mesh is too complicated to be a useful occluder.
	bUseAsOccluder = false;

	// We have no nav mesh support yet.
	bCanEverAffectNavigation = false;
}


FPrimitiveSceneProxy* UStreetMapComponent::CreateSceneProxy()
{
	BuildMeshIfNeeded();
	
	FStreetMapSceneProxy* StreetMapSceneProxy = nullptr;

	if( HasValidMesh() )
	{
		StreetMapSceneProxy = new FStreetMapSceneProxy( this );
		StreetMapSceneProxy->Init( this, Vertices, Indices );
	}
	
	return StreetMapSceneProxy;
}


int32 UStreetMapComponent::GetNumMaterials() const
{
	// NOTE: This is a bit of a weird thing about Unreal that we need to deal with when defining a component that
	// can have materials assigned.  UPrimitiveComponent::GetNumMaterials() will return 0, so we need to override it 
	// to return the number of overridden materials, which are the actual materials assigned to the component.
	return GetNumOverrideMaterials();
}


void UStreetMapComponent::SetStreetMap( class UStreetMap* NewStreetMap )
{
	if( StreetMap != NewStreetMap )
	{
		StreetMap = NewStreetMap;

		InvalidateMesh();
	}
}


void UStreetMapComponent::GenerateMesh()
{
	/////////////////////////////////////////////////////////
	// Visual tweakables for generated Street Map mesh
	//
	const float RoadZ = 0.0f;
	const bool bWant3DBuildings = true;
	const bool bWantLitBuildings = true;
	const bool bWantBuildingBorderOnGround = !bWant3DBuildings;
	const float StreetThickness = 800.0f;
	const FColor StreetColor = FLinearColor( 0.05f, 0.75f, 0.05f ).ToFColor( false );
	const float MajorRoadThickness = 1000.0f;
	const FColor MajorRoadColor = FLinearColor( 0.15f, 0.85f, 0.15f ).ToFColor( false );
	const float HighwayThickness = 1400.0f;
	const FColor HighwayColor = FLinearColor( 0.25f, 0.95f, 0.25f ).ToFColor( false );
	const float BuildingBorderThickness = 20.0f;
	FLinearColor BuildingBorderLinearColor( 0.85f, 0.85f, 0.85f );
	const float BuildingBorderZ = 10.0f;
	const FColor BuildingBorderColor( BuildingBorderLinearColor.ToFColor( false ) );
	const FColor BuildingFillColor( FLinearColor( BuildingBorderLinearColor * 0.33f ).CopyWithNewOpacity( 1.0f ).ToFColor( false ) );
	/////////////////////////////////////////////////////////


	CachedLocalBounds = FBox( 0 );
	Vertices.Reset();
	Indices.Reset();

	if( StreetMap != nullptr )
	{
		FBox MeshBoundingBox;
		MeshBoundingBox.Init();

		const auto& Roads = StreetMap->GetRoads();
		const auto& Nodes = StreetMap->GetNodes();
		const auto& Buildings = StreetMap->GetBuildings();

		for( const auto& Road : Roads )
		{
			float RoadThickness = StreetThickness;
			FColor RoadColor = StreetColor;
			switch( Road.RoadType )
			{
				case EStreetMapRoadType::Highway:
					RoadThickness = HighwayThickness;
					RoadColor = HighwayColor;
					break;
					
				case EStreetMapRoadType::MajorRoad:
					RoadThickness = MajorRoadThickness;
					RoadColor = MajorRoadColor;
					break;
					
				case EStreetMapRoadType::Street:
				case EStreetMapRoadType::Other:
					break;
					
				default:
					check( 0 );
					break;
			}
			
			for( int32 PointIndex = 0; PointIndex < Road.RoadPoints.Num() - 1; ++PointIndex )
			{
				AddThick2DLine( 
					Road.RoadPoints[ PointIndex ],
					Road.RoadPoints[ PointIndex + 1 ],
					RoadZ,
					RoadThickness,
					RoadColor,
					RoadColor,
					MeshBoundingBox );
			}
		}
		
		TArray< int32 > TempIndices;
		TArray< int32 > TriangulatedVertexIndices;
		TArray< FVector > TempPoints;
		for( int32 BuildingIndex = 0; BuildingIndex < Buildings.Num(); ++BuildingIndex )
		{
			const auto& Building = Buildings[ BuildingIndex ];

			// Building mesh (or filled area, if the building has no height)

			// Triangulate this building
			// @todo: Performance: Triangulating lots of building polygons is quite slow.  We could easily do this 
			//        as part of the import process and store tessellated geometry instead of doing this at load time.
			bool WindsClockwise;
			if( FPolygonTools::TriangulatePolygon( Building.BuildingPoints, TempIndices, /* Out */ TriangulatedVertexIndices, /* Out */ WindsClockwise ) )
			{
				// @todo: Performance: We could preprocess the building shapes so that the points always wind
				//        in a consistent direction, so we can skip determining the winding above.

				const int32 FirstTopVertexIndex = this->Vertices.Num();
				const float BuildingFillZ = bWant3DBuildings ? Building.Height : 0.0f;

				// Top of building
				{
					TempPoints.SetNum( Building.BuildingPoints.Num(), false );
					for( int32 PointIndex = 0; PointIndex < Building.BuildingPoints.Num(); ++PointIndex )
					{
						TempPoints[ PointIndex ] = FVector( Building.BuildingPoints[ ( Building.BuildingPoints.Num() - PointIndex ) - 1 ], BuildingFillZ );
					}
					AddTriangles( TempPoints, TriangulatedVertexIndices, FVector::ForwardVector, FVector::UpVector, BuildingFillColor, MeshBoundingBox );
				}

				if( bWant3DBuildings && Building.Height > KINDA_SMALL_NUMBER )
				{
					// NOTE: Lit buildings can't share vertices beyond quads (all quads have their own face normals), so this uses a lot more geometry!
					if( bWantLitBuildings )
					{
						// Create edges for the walls of the 3D buildings
						for( int32 LeftPointIndex = 0; LeftPointIndex < Building.BuildingPoints.Num(); ++LeftPointIndex )
						{
							const int32 RightPointIndex = ( LeftPointIndex + 1 ) % Building.BuildingPoints.Num();

							TempPoints.SetNum( 4, false );

							const int32 TopLeftVertexIndex = 0;
							TempPoints[ TopLeftVertexIndex ] = FVector( Building.BuildingPoints[ WindsClockwise ? RightPointIndex : LeftPointIndex ], BuildingFillZ );

							const int32 TopRightVertexIndex = 1;
							TempPoints[ TopRightVertexIndex ] = FVector( Building.BuildingPoints[ WindsClockwise ? LeftPointIndex : RightPointIndex ], BuildingFillZ );

							const int32 BottomRightVertexIndex = 2;
							TempPoints[ BottomRightVertexIndex ] = FVector( Building.BuildingPoints[ WindsClockwise ? LeftPointIndex : RightPointIndex ], 0.0f );

							const int32 BottomLeftVertexIndex = 3;
							TempPoints[ BottomLeftVertexIndex ] = FVector( Building.BuildingPoints[ WindsClockwise ? RightPointIndex : LeftPointIndex ], 0.0f );


							TempIndices.SetNum( 6, false );

							TempIndices[ 0 ] = BottomLeftVertexIndex;
							TempIndices[ 1 ] = TopLeftVertexIndex;
							TempIndices[ 2 ] = BottomRightVertexIndex;

							TempIndices[ 3 ] = BottomRightVertexIndex;
							TempIndices[ 4 ] = TopLeftVertexIndex;
							TempIndices[ 5 ] = TopRightVertexIndex;

							const FVector FaceNormal = FVector::CrossProduct( ( TempPoints[ 0 ] - TempPoints[ 2 ] ).GetSafeNormal(), ( TempPoints[ 0 ] - TempPoints[ 1 ] ).GetSafeNormal() );
							const FVector ForwardVector = FVector::UpVector;
							const FVector UpVector = FaceNormal;
							AddTriangles( TempPoints, TempIndices, ForwardVector, UpVector, BuildingFillColor, MeshBoundingBox );
						}
					}
					else
					{
						// Create vertices for the bottom
						const int32 FirstBottomVertexIndex = this->Vertices.Num();
						for( int32 PointIndex = 0; PointIndex < Building.BuildingPoints.Num(); ++PointIndex )
						{
							const FVector2D Point = Building.BuildingPoints[ PointIndex ];

							FStreetMapVertex& NewVertex = *new( this->Vertices )FStreetMapVertex();
							NewVertex.Position = FVector( Point, 0.0f );
							NewVertex.TextureCoordinate = FVector2D( 0.0f, 0.0f );	// NOTE: We're not using texture coordinates for anything yet
							NewVertex.TangentX = FVector::ForwardVector;	 // NOTE: Tangents aren't important for these unlit buildings
							NewVertex.TangentZ = FVector::UpVector;
							NewVertex.Color = BuildingFillColor;

							MeshBoundingBox += NewVertex.Position;
						}

						// Create edges for the walls of the 3D buildings
						for( int32 LeftPointIndex = 0; LeftPointIndex < Building.BuildingPoints.Num(); ++LeftPointIndex )
						{
							const int32 RightPointIndex = ( LeftPointIndex + 1 ) % Building.BuildingPoints.Num();

							const int32 BottomLeftVertexIndex = FirstBottomVertexIndex + LeftPointIndex;
							const int32 BottomRightVertexIndex = FirstBottomVertexIndex + RightPointIndex;
							const int32 TopRightVertexIndex = FirstTopVertexIndex + RightPointIndex;
							const int32 TopLeftVertexIndex = FirstTopVertexIndex + LeftPointIndex;

							this->Indices.Add( BottomLeftVertexIndex );
							this->Indices.Add( TopLeftVertexIndex );
							this->Indices.Add( BottomRightVertexIndex );

							this->Indices.Add( BottomRightVertexIndex );
							this->Indices.Add( TopLeftVertexIndex );
							this->Indices.Add( TopRightVertexIndex );
						}
					}
				}
			}
			else
			{
				// @todo: Triangulation failed for some reason, possibly due to degenerate polygons.  We can
				//        probably improve the algorithm to avoid this happening.
			}

			// Building border
			if( bWantBuildingBorderOnGround )
			{
				for( int32 PointIndex = 0; PointIndex < Building.BuildingPoints.Num(); ++PointIndex )
				{
					AddThick2DLine(
						Building.BuildingPoints[ PointIndex ],
						Building.BuildingPoints[ ( PointIndex + 1 ) % Building.BuildingPoints.Num() ],
						BuildingBorderZ,
						BuildingBorderThickness,		// Thickness
						BuildingBorderColor,
						BuildingBorderColor,
						MeshBoundingBox );
				}
			}
		}

		CachedLocalBounds = MeshBoundingBox;
	}
}


#if WITH_EDITOR
void UStreetMapComponent::PostEditChangeProperty( FPropertyChangedEvent& PropertyChangedEvent )
{
	bool bNeedNewData = false;

	// Check to see if the "StreetMap" property changed.  If so, we'll need to rebuild our mesh.
	if( PropertyChangedEvent.Property != nullptr )
	{
		const FName PropertyName( PropertyChangedEvent.Property->GetFName() );
		if( PropertyName == GET_MEMBER_NAME_CHECKED( UStreetMapComponent, StreetMap ) )
		{
			bNeedNewData = true;
		}
	}

	if( bNeedNewData )
	{
		InvalidateMesh();
	}

	// Call the parent implementation of this function
	Super::PostEditChangeProperty( PropertyChangedEvent );
}
#endif	// WITH_EDITOR


void UStreetMapComponent::BuildMeshIfNeeded()
{
	const bool bNeedNewMesh = !HasValidMesh();
	if( bNeedNewMesh )
	{
		GenerateMesh();

		if( HasValidMesh() )
		{
			// We have a new bounding box
			UpdateBounds();
		}
		else
		{
			// No mesh was generated
		}
	}
}


FBoxSphereBounds UStreetMapComponent::CalcBounds( const FTransform& LocalToWorld ) const
{
	if( HasValidMesh() )
	{
		FBoxSphereBounds WorldSpaceBounds = CachedLocalBounds.TransformBy( LocalToWorld );
		WorldSpaceBounds.BoxExtent *= BoundsScale;
		WorldSpaceBounds.SphereRadius *= BoundsScale;
		return WorldSpaceBounds;
	}
	else
	{
		return FBoxSphereBounds( LocalToWorld.GetLocation(), FVector::ZeroVector, 0.0f );
	}
}


void UStreetMapComponent::AddThick2DLine( const FVector2D Start, const FVector2D End, const float Z, const float Thickness, const FColor& StartColor, const FColor& EndColor, FBox& MeshBoundingBox )
{
	const float HalfThickness = Thickness * 0.5f;

	const FVector2D LineDirection = ( End - Start ).GetSafeNormal();
	const FVector2D RightVector( -LineDirection.Y, LineDirection.X );

	const int32 BottomLeftVertexIndex = Vertices.Num();
	FStreetMapVertex& BottomLeftVertex = *new( Vertices )FStreetMapVertex();
	BottomLeftVertex.Position = FVector( Start - RightVector * HalfThickness, Z );
	BottomLeftVertex.TextureCoordinate = FVector2D( 0.0f, 0.0f );
	BottomLeftVertex.TangentX = FVector( LineDirection, 0.0f );
	BottomLeftVertex.TangentZ = FVector::UpVector;
	BottomLeftVertex.Color = StartColor;
	MeshBoundingBox += BottomLeftVertex.Position;

	const int32 BottomRightVertexIndex = Vertices.Num();
	FStreetMapVertex& BottomRightVertex = *new( Vertices )FStreetMapVertex();
	BottomRightVertex.Position = FVector( Start + RightVector * HalfThickness, Z );
	BottomRightVertex.TextureCoordinate = FVector2D( 1.0f, 0.0f );
	BottomRightVertex.TangentX = FVector( LineDirection, 0.0f );
	BottomRightVertex.TangentZ = FVector::UpVector;
	BottomRightVertex.Color = StartColor;
	MeshBoundingBox += BottomRightVertex.Position;

	const int32 TopRightVertexIndex = Vertices.Num();
	FStreetMapVertex& TopRightVertex = *new( Vertices )FStreetMapVertex();
	TopRightVertex.Position = FVector( End + RightVector * HalfThickness, Z );
	TopRightVertex.TextureCoordinate = FVector2D( 1.0f, 1.0f );
	TopRightVertex.TangentX = FVector( LineDirection, 0.0f );
	TopRightVertex.TangentZ = FVector::UpVector;
	TopRightVertex.Color = EndColor;
	MeshBoundingBox += TopRightVertex.Position;

	const int32 TopLeftVertexIndex = Vertices.Num();
	FStreetMapVertex& TopLeftVertex = *new( Vertices )FStreetMapVertex();
	TopLeftVertex.Position = FVector( End - RightVector * HalfThickness, Z );
	TopLeftVertex.TextureCoordinate = FVector2D( 0.0f, 1.0f );
	TopLeftVertex.TangentX = FVector( LineDirection, 0.0f );
	TopLeftVertex.TangentZ = FVector::UpVector;
	TopLeftVertex.Color = EndColor;
	MeshBoundingBox += TopLeftVertex.Position;

	Indices.Add( BottomLeftVertexIndex );
	Indices.Add( BottomRightVertexIndex );
	Indices.Add( TopRightVertexIndex );

	Indices.Add( BottomLeftVertexIndex );
	Indices.Add( TopRightVertexIndex );
	Indices.Add( TopLeftVertexIndex );
};


void UStreetMapComponent::AddTriangles( const TArray<FVector>& Points, const TArray<int32>& PointIndices, const FVector& ForwardVector, const FVector& UpVector, const FColor& Color, FBox& MeshBoundingBox )
{
	const int32 FirstVertexIndex = Vertices.Num();

	for( FVector Point : Points )
	{
		FStreetMapVertex& NewVertex = *new( Vertices )FStreetMapVertex();
		NewVertex.Position = Point;
		NewVertex.TextureCoordinate = FVector2D( 0.0f, 0.0f );	// NOTE: We're not using texture coordinates for anything yet
		NewVertex.TangentX = ForwardVector;
		NewVertex.TangentZ = UpVector;
		NewVertex.Color = Color;

		MeshBoundingBox += NewVertex.Position;
	}

	for( int32 PointIndex : PointIndices )
	{
		Indices.Add( FirstVertexIndex + PointIndex );
	}
};


void UStreetMapComponent::InvalidateMesh()
{
	Vertices.Reset();
	Indices.Reset();
	CachedLocalBounds = FBoxSphereBounds( FBox( 0 ) );

	// Mark our render state dirty so that CreateSceneProxy can refresh it on demand
	MarkRenderStateDirty();
}



