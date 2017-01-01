// Copyright 2017 Mike Fricker. All Rights Reserved.
#pragma once

#include "Components/MeshComponent.h"
#include "StreetMapSceneProxy.h"
#include "StreetMapComponent.generated.h"


/**
 * Component that represents a section of street map roads and buildings
 */
UCLASS( meta=(BlueprintSpawnableComponent) )
class STREETMAPRUNTIME_API UStreetMapComponent : public UMeshComponent
{
	GENERATED_BODY()

public:

	/** UStreetMapComponent constructor */
	UStreetMapComponent( const class FObjectInitializer& ObjectInitializer );

	/** @return Gets the street map object associated with this component */
	class UStreetMap* GetStreetMap()
	{
		return StreetMap;
	}

	/** 
	 * Assigns a street map asset to this component.  Render state will be updated immediately.
	 * 
	 * @param NewStreetMap The street map to use
	 * 
	 * @return Sets the street map object 
	 */
	UFUNCTION( BlueprintCallable, Category="StreetMap" )
	void SetStreetMap( class UStreetMap* NewStreetMap );


	// UPrimitiveComponent interface
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual FBoxSphereBounds CalcBounds( const FTransform& LocalToWorld ) const override;
	virtual int32 GetNumMaterials() const override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty( FPropertyChangedEvent& PropertyChangedEvent ) override;
#endif

protected:

	/** Generates a cached mesh from raw street map data */
	void GenerateMesh();

	/** Returns true if we have valid cached mesh data from our assigned street map asset */
	bool HasValidMesh() const
	{
		return Vertices.Num() != 0 && Indices.Num() != 0;
	}
	
	/** Wipes out our cached mesh data.  It will be recreated on demand. */
	void InvalidateMesh();

	/** Rebuilds the graphics and physics mesh representation if we don't have one right now.  Designed to be called on demand. */
	void BuildMeshIfNeeded();

	/** Adds a 2D line to the raw mesh */
	void AddThick2DLine( const FVector2D Start, const FVector2D End, const float Z, const float Thickness, const FColor& StartColor, const FColor& EndColor, FBox& MeshBoundingBox );

	/** Adds 3D triangles to the raw mesh */
	void AddTriangles( const TArray<FVector>& Points, const TArray<int32>& PointIndices, const FVector& ForwardVector, const FVector& UpVector, const FColor& Color, FBox& MeshBoundingBox );


protected:
	
	/** The street map we're representing */
	UPROPERTY( EditAnywhere )
	class UStreetMap* StreetMap;


	//
	// Cached mesh representation
	//

	/** Cached raw mesh vertices */
	TArray< struct FStreetMapVertex > Vertices;

	/** Cached raw mesh triangle indices */
	TArray< uint32 > Indices;

	/** Cached bounding box */
	FBoxSphereBounds CachedLocalBounds;

};
