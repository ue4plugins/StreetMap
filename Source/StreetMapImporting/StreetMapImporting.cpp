// Copyright 2017 Mike Fricker. All Rights Reserved.

#include "StreetMapImporting.h"
#include "StreetMapAssetTypeActions.h"
#include "ModuleManager.h"


class FStreetMapImportingModule : public IModuleInterface
{

public:

	// IModuleInterface interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	TSharedPtr< FStreetMapAssetTypeActions > StreetMapAssetTypeActions;
};


IMPLEMENT_MODULE( FStreetMapImportingModule, StreetMapImporting )



void FStreetMapImportingModule::StartupModule()
{
	// Register asset types
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>( "AssetTools" ).Get();
	StreetMapAssetTypeActions = MakeShareable( new FStreetMapAssetTypeActions() );
	AssetTools.RegisterAssetTypeActions( StreetMapAssetTypeActions.ToSharedRef() );
}


void FStreetMapImportingModule::ShutdownModule()
{
	// Unregister all the asset types that we registered
	if( FModuleManager::Get().IsModuleLoaded( "AssetTools" ) )
	{
		IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>( "AssetTools" ).Get();
		AssetTools.UnregisterAssetTypeActions( StreetMapAssetTypeActions.ToSharedRef() );
		StreetMapAssetTypeActions.Reset();
	}
}
