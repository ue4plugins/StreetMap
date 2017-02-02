// Copyright 2017 Mike Fricker. All Rights Reserved.

#include "StreetMapRuntime.h"
#include "StreetMapSceneProxy.h"
#include "StreetMapComponent.h"
#include "Runtime/Engine/Public/SceneManagement.h"


void FStreetMapVertexBuffer::InitRHI()
{
	if( Vertices.Num() > 0 )
	{
		// Allocate our vertex buffer
		FRHIResourceCreateInfo CreateInfo;
		VertexBufferRHI = RHICreateVertexBuffer( Vertices.Num() * sizeof( Vertices[0] ), BUF_Static, CreateInfo );
		
		// Load the vertex buffer with data
		void* VertexBufferData = RHILockVertexBuffer( VertexBufferRHI, 0, Vertices.Num() * sizeof( Vertices[0] ), RLM_WriteOnly );
		FMemory::Memcpy( VertexBufferData, Vertices.GetData(), Vertices.Num() * sizeof( FStreetMapVertex ) );
		RHIUnlockVertexBuffer( VertexBufferRHI );
	}
}


void FStreetMapIndexBuffer::InitRHI()
{
	const int IndexCount = FMath::Max( Indices16.Num(), Indices32.Num() );
	if( IndexCount > 0 )
	{
		const bool b32BitIndices = Indices32.Num() > Indices16.Num();
		const uint8 IndexSize = b32BitIndices ? sizeof( Indices32[ 0 ] ) : sizeof( Indices16[ 0 ] );
		const void* IndexSourceData;
		if( b32BitIndices )
		{
			IndexSourceData = Indices32.GetData();
		}
		else
		{
			IndexSourceData = Indices16.GetData();
		}
		
		// Allocate our index buffer and load it with data
		FRHIResourceCreateInfo CreateInfo;
		IndexBufferRHI = RHICreateIndexBuffer( IndexSize, IndexCount * IndexSize, BUF_Static, CreateInfo );
		void* IndexBufferData = RHILockIndexBuffer( IndexBufferRHI, 0, IndexCount * IndexSize, RLM_WriteOnly );
		FMemory::Memcpy( IndexBufferData, IndexSourceData, IndexCount * IndexSize );
		RHIUnlockIndexBuffer( IndexBufferRHI );
	}
}


void FStreetMapVertexFactory::InitVertexFactory( const FStreetMapVertexBuffer& VertexBuffer )
{
	// Setup the vertex factory streams
	FDataType DataType;
	DataType.PositionComponent = STRUCTMEMBER_VERTEXSTREAMCOMPONENT( &VertexBuffer, FStreetMapVertex, Position, VET_Float3 );
	DataType.TextureCoordinates.Add( STRUCTMEMBER_VERTEXSTREAMCOMPONENT( &VertexBuffer, FStreetMapVertex, TextureCoordinate, VET_Float2 ) );
	DataType.TangentBasisComponents[0] = STRUCTMEMBER_VERTEXSTREAMCOMPONENT( &VertexBuffer, FStreetMapVertex, TangentX, VET_PackedNormal);
	DataType.TangentBasisComponents[1] = STRUCTMEMBER_VERTEXSTREAMCOMPONENT( &VertexBuffer, FStreetMapVertex, TangentZ, VET_PackedNormal);
	DataType.ColorComponent = STRUCTMEMBER_VERTEXSTREAMCOMPONENT( &VertexBuffer, FStreetMapVertex, Color, VET_Color );
	
	// Send it off to the rendering thread
	check( !IsInActualRenderingThread() );
	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
											   InitStreetMapVertexFactory,
											   FStreetMapVertexFactory*, VertexFactory, this,
											   FDataType, DataType, DataType,
											   {
												   VertexFactory->SetData( DataType );
											   });
}


FStreetMapSceneProxy::FStreetMapSceneProxy(const UStreetMapComponent* InComponent)
	: FPrimitiveSceneProxy(InComponent),
	StreetMapComp(InComponent),
	CollisionResponse(InComponent->GetCollisionResponseToChannels())
{

}


void FStreetMapSceneProxy::Init( const UStreetMapComponent* InComponent, const TArray< FStreetMapVertex >& Vertices, const TArray< uint16 >& Indices )
{
	// Copy 16-bit index data
	IndexBuffer.Indices16 = Indices;
	
	InitAfterIndexBuffer( InComponent, Vertices );
}


void FStreetMapSceneProxy::Init( const UStreetMapComponent* InComponent, const TArray< FStreetMapVertex >& Vertices, const TArray< uint32 >& Indices )
{
	// If we fit into a 16-bit index buffer, just use that
	if( Vertices.Num() < 0xffff )
	{
		const int32 IndexCount = Indices.Num();
		IndexBuffer.Indices16.AddUninitialized( IndexCount );
		for( int32 Index = 0; Index < IndexCount; ++Index )
		{
			IndexBuffer.Indices16[ Index ] = Indices[ Index ];
		}
	}
	else
	{
		// Copy 32-bit index data
		IndexBuffer.Indices32 = Indices;
	}
	
	InitAfterIndexBuffer( InComponent, Vertices );
}

void FStreetMapSceneProxy::InitAfterIndexBuffer( const UStreetMapComponent* StreetMapComponent, const TArray< FStreetMapVertex >& Vertices )
{
	MaterialInterface = nullptr;
	this->MaterialRelevance = StreetMapComponent->GetMaterialRelevance( GetScene().GetFeatureLevel() );
	
	// Copy vertex data
	VertexBuffer.Vertices = Vertices;
	InitResources();
	
	// Set a material
	{
		if( StreetMapComponent->GetNumMaterials() > 0 )
		{
			MaterialInterface = StreetMapComponent->GetMaterial( 0 );
		}
		
		// Use the default material if we don't have one set
		if( MaterialInterface == nullptr )
		{
			MaterialInterface = UMaterial::GetDefaultMaterial( MD_Surface );
		}
	}
}



FStreetMapSceneProxy::~FStreetMapSceneProxy()
{
	VertexBuffer.ReleaseResource();
	IndexBuffer.ReleaseResource();
	VertexFactory.ReleaseResource();
}


void FStreetMapSceneProxy::InitResources()
{
	// Start initializing our vertex buffer, index buffer, and vertex factory.  This will be kicked off on the render thread.
	BeginInitResource( &VertexBuffer );
	BeginInitResource( &IndexBuffer );
	
	VertexFactory.InitVertexFactory( VertexBuffer );
	BeginInitResource( &VertexFactory );
}


bool FStreetMapSceneProxy::MustDrawMeshDynamically( const FSceneView& View ) const
{
	return ( AllowDebugViewmodes() && View.Family->EngineShowFlags.Wireframe ) || IsSelected();
}


bool FStreetMapSceneProxy::IsInCollisionView(const FEngineShowFlags& EngineShowFlags) const
{
	return  EngineShowFlags.CollisionVisibility || EngineShowFlags.CollisionPawn;
}

FPrimitiveViewRelevance FStreetMapSceneProxy::GetViewRelevance( const FSceneView* View ) const
{
	FPrimitiveViewRelevance Result;
	Result.bDrawRelevance = IsShown(View);
	Result.bShadowRelevance = IsShadowCast(View);
	
	const bool bAlwaysHasDynamicData = false;

	// Only draw dynamically if we're drawing in wireframe or we're selected in the editor
	Result.bDynamicRelevance = MustDrawMeshDynamically( *View ) || bAlwaysHasDynamicData;
	Result.bStaticRelevance = !MustDrawMeshDynamically( *View );
	
	MaterialRelevance.SetPrimitiveViewRelevance(Result);
	return Result;
}


bool FStreetMapSceneProxy::CanBeOccluded() const
{
	return !MaterialRelevance.bDisableDepthTest;
}


void FStreetMapSceneProxy::MakeMeshBatch( FMeshBatch& Mesh, FMaterialRenderProxy* WireframeMaterialRenderProxyOrNull, bool bDrawCollision) const
{
	FMaterialRenderProxy* MaterialProxy = NULL;
	if( WireframeMaterialRenderProxyOrNull != nullptr )
	{
		MaterialProxy = WireframeMaterialRenderProxyOrNull;
	}
	else
	{
		if (bDrawCollision)
		{
			MaterialProxy = new FColoredMaterialRenderProxy(GEngine->ShadedLevelColorationUnlitMaterial->GetRenderProxy(IsSelected(), IsHovered()), FColor::Cyan);
		}
		else if (MaterialProxy == nullptr)
		{
			MaterialProxy = StreetMapComp->GetDefaultMaterial()->GetRenderProxy(IsSelected());
		}
	}
	
	FMeshBatchElement& BatchElement = Mesh.Elements[0];
	BatchElement.IndexBuffer = &IndexBuffer;
	Mesh.bWireframe = WireframeMaterialRenderProxyOrNull != nullptr;
	Mesh.VertexFactory = &VertexFactory;
	Mesh.MaterialRenderProxy = MaterialProxy;
	Mesh.CastShadow = true;
	BatchElement.PrimitiveUniformBuffer = CreatePrimitiveUniformBufferImmediate(GetLocalToWorld(), GetBounds(), GetLocalBounds(), true, UseEditorDepthTest());
	BatchElement.FirstIndex = 0;
	const int IndexCount = FMath::Max( IndexBuffer.Indices16.Num(), IndexBuffer.Indices32.Num() );
	BatchElement.NumPrimitives = IndexCount / 3;
	BatchElement.MinVertexIndex = 0;
	BatchElement.MaxVertexIndex = VertexBuffer.Vertices.Num() - 1;
	Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
	Mesh.Type = PT_TriangleList;
	Mesh.DepthPriorityGroup = SDPG_World;
}


void FStreetMapSceneProxy::DrawStaticElements( FStaticPrimitiveDrawInterface* PDI )
{
	const int IndexCount = FMath::Max( IndexBuffer.Indices16.Num(), IndexBuffer.Indices32.Num() );
	if( VertexBuffer.Vertices.Num() > 0 && IndexCount > 0 )
	{
		const float ScreenSize = 1.0f;

		FMeshBatch MeshBatch;
		MakeMeshBatch( MeshBatch, nullptr);
		PDI->DrawMesh( MeshBatch, ScreenSize );
	}
}


void FStreetMapSceneProxy::GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, class FMeshElementCollector& Collector) const
{
	const int IndexCount = FMath::Max(IndexBuffer.Indices16.Num(), IndexBuffer.Indices32.Num());
	if (VertexBuffer.Vertices.Num() > 0 && IndexCount > 0)
	{
		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
		{
			const FSceneView& View = *Views[ViewIndex];

			const bool bIsWireframe = AllowDebugViewmodes() && View.Family->EngineShowFlags.Wireframe;

			FColoredMaterialRenderProxy* WireframeMaterialRenderProxy = GEngine->WireframeMaterial && bIsWireframe ? new FColoredMaterialRenderProxy(GEngine->WireframeMaterial->GetRenderProxy(IsSelected()), FLinearColor(0, 0.5f, 1.f)) : NULL;


			if (MustDrawMeshDynamically(View))
			{
				const bool bInCollisionView = IsInCollisionView(ViewFamily.EngineShowFlags);
				const bool bCanDrawCollision = bInCollisionView && IsCollisionEnabled();

				if (!IsCollisionEnabled() && bInCollisionView)
				{
					continue;
				}

				// Draw the mesh!
				FMeshBatch& MeshBatch = Collector.AllocateMesh();
				MakeMeshBatch(MeshBatch, WireframeMaterialRenderProxy, bCanDrawCollision);
				Collector.AddMesh(ViewIndex, MeshBatch);
			}
		}
	}
}


uint32 FStreetMapSceneProxy::GetMemoryFootprint( void ) const
{ 
	return sizeof( *this ) + GetAllocatedSize();
}
