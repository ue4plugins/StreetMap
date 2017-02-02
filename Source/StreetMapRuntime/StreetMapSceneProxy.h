// Copyright 2017 Mike Fricker. All Rights Reserved.
#pragma once

#include "Runtime/Engine/Public/PrimitiveSceneProxy.h"
#include "Runtime/Engine/Public/LocalVertexFactory.h"
#include "StreetMapSceneProxy.generated.h"

/**	A single vertex on a street map mesh */
USTRUCT()
struct FStreetMapVertex
{

	GENERATED_USTRUCT_BODY()

	/** Location of the vertex in local space */
	UPROPERTY()
		FVector Position;

	/** Texture coordinate */
	UPROPERTY()
		FVector2D TextureCoordinate;

	/** Tangent vector X */
	UPROPERTY()
		FVector TangentX;

	/** Tangent vector Z (normal) */
	UPROPERTY()
		FVector TangentZ;

	/** Color */
	UPROPERTY()
		FColor Color;


	/** Default constructor, leaves everything uninitialized */
	FStreetMapVertex()
	{
	}

	/** Construct with a supplied position and tangents for the vertex */
	FStreetMapVertex(const FVector InitLocation, const FVector2D InitTextureCoordinate, const FVector InitTangentX, const FVector InitTangentZ, const FColor InitColor)
		: Position(InitLocation),
		TextureCoordinate(InitTextureCoordinate),
		TangentX(InitTangentX),
		TangentZ(InitTangentZ),
		Color(InitColor)
	{
	}
};


/** Street map mesh vertex buffer */
class FStreetMapVertexBuffer : public FVertexBuffer
{

public:

	/** All of the vertices in this mesh */
	TArray< FStreetMapVertex > Vertices;


	// FRenderResource interface
	virtual void InitRHI() override;
};


/** Street map mesh index buffer */
class FStreetMapIndexBuffer : public FIndexBuffer
{

public:

	/** 16-bit indices */
	TArray< uint16 > Indices16;

	/** 32-bit indices */
	TArray< uint32 > Indices32;


	// FRenderResource interface
	virtual void InitRHI() override;
};


/** Street map mesh vertex factory */
class FStreetMapVertexFactory : public FLocalVertexFactory
{

public:

	/** Initialize this vertex factory */
	void InitVertexFactory( const FStreetMapVertexBuffer& VertexBuffer );
};


/** Scene proxy for rendering a section of a street map mesh on the rendering thread */
class FStreetMapSceneProxy : public FPrimitiveSceneProxy
{

public:

	/** Construct this scene proxy */
	FStreetMapSceneProxy(const class UStreetMapComponent* InComponent);

	/**
	* Init this street map mesh scene proxy for the specified component (32-bit indices)
	*
	* @param	InComponent			The street map mesh component to initialize this with
	* @param	Vertices			The vertices for this street map mesh
	* @param	Indices				The vertex indices for this street map mesh
	*/
	void Init(const UStreetMapComponent* InComponent, const TArray< FStreetMapVertex >& Vertices, const TArray< uint32 >& Indices);

	/**
	* Init this street map mesh scene proxy for the specified component (16-bit indices)
	*
	* @param	InComponent			The street map mesh component to initialize this with
	* @param	Vertices			The vertices for this street map mesh
	* @param	Indices				The vertex indices for this street map mesh
	*/
	void Init(const UStreetMapComponent* InComponent, const TArray< FStreetMapVertex >& Vertices, const TArray< uint16 >& Indices);

	/** Destructor that cleans up our rendering data */
	virtual ~FStreetMapSceneProxy();


protected:

	/** Called from the constructor to finish construction after the index buffer is setup */
	void InitAfterIndexBuffer(const class UStreetMapComponent* StreetMapComponent, const TArray< FStreetMapVertex >& Vertices);

	/** Initializes this scene proxy's vertex buffer, index buffer and vertex factory (on the render thread.) */
	void InitResources();

	/** Makes a MeshBatch for rendering.  Called every time the mesh is drawn */
	void MakeMeshBatch(struct FMeshBatch& Mesh, class FMaterialRenderProxy* WireframeMaterialRenderProxyOrNull, bool bDrawCollision = false) const;

	/** Checks to see if this mesh must be drawn during the dynamic pass.  Note that even when this returns false, we may still
	have other (debug) geometry to render as dynamic */
	bool MustDrawMeshDynamically(const class FSceneView& View) const;

	/** Returns true , if in a collision view */
	bool IsInCollisionView(const FEngineShowFlags& EngineShowFlags) const;

	// FPrimitiveSceneProxy interface
	virtual void DrawStaticElements(class FStaticPrimitiveDrawInterface* PDI) override;
	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, class FMeshElementCollector& Collector) const override;
	virtual uint32 GetMemoryFootprint(void) const override;
	virtual FPrimitiveViewRelevance GetViewRelevance(const class FSceneView* View) const override;
	virtual bool CanBeOccluded() const override;



protected:



	/** Contains all of the vertices in our street map mesh */
	FStreetMapVertexBuffer VertexBuffer;

	/** All of the vertex indices in our street map mesh */
	FStreetMapIndexBuffer IndexBuffer;

	/** Our vertex factory specific to street map meshes */
	FStreetMapVertexFactory VertexFactory;

	/** Cached material relevance */
	FMaterialRelevance MaterialRelevance;

	/** The material we'll use to render this street map mesh */
	class UMaterialInterface* MaterialInterface;

	const UStreetMapComponent* StreetMapComp;

	// The Collision Response of the component being proxied
	FCollisionResponseContainer CollisionResponse;


};
