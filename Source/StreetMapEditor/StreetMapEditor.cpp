// Copyright 2017 Mike Fricker. All Rights Reserved.

#include "StreetMapEditor.h"
#include "StreetMapStyle.h"



class FStreetMapEditor : public IModuleInterface
{

public:

	// IModuleInterface interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

};


IMPLEMENT_MODULE(FStreetMapEditor, StreetMapEditor)



void FStreetMapEditor::StartupModule()
{
	FStreetMapStyle::Initialize();

	// Register StreetMapComponent Detail Customization
	FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomClassLayout("StreetMapComponent", FOnGetDetailCustomizationInstance::CreateStatic(&FStreetMapComponentDetails::MakeInstance));
	PropertyModule.NotifyCustomizationModuleChanged();
}


void FStreetMapEditor::ShutdownModule()
{

	FStreetMapStyle::Shutdown();

	if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.UnregisterCustomClassLayout("StreetMapComponent");
		PropertyModule.NotifyCustomizationModuleChanged();
	}
}
