// Copyright 2017 Mike Fricker. All Rights Reserved.

#pragma once

#include "IDetailCustomization.h"

class  FStreetMapComponentDetails : public IDetailCustomization
{
public:

	FStreetMapComponentDetails();

	/** Makes a new instance of this detail layout class for a specific detail view requesting it. */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** IDetailCustomization interface */
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

	/** Returns true if we selected a valid cached streetMapComp , having valid cached mesh data. */
	bool HasValidMeshData() const;

	/** Returns true if we selected a valid cached streetMapComp having valid street map representation. */
	bool HasValidMapObject() const;

	/** Handles create static mesh asset button clicking */
	FReply OnCreateStaticMeshAssetClicked();

	/** Handles build/rebuild mesh button clicking */
	FReply OnBuildMeshClicked();

	/** Handles clear mesh button clicking */
	FReply OnClearMeshClicked();

	/** Refreshes the details view and regenerates all the customized layouts. */
	void RefreshDetails();

	/** Handles build landscape button clicking */
	FReply OnBuildLandscapeClicked();

	/** Returns whether Landscape can be created or not */
	bool BuildLandscapeIsEnabled() const;

	/** Updates available landscape layers based on selected material */
	void RefreshLandscapeLayersList();

protected:
	/** Holds Selected Street Map Component */
	 class UStreetMapComponent* SelectedStreetMapComponent;

	 /** Holds Last Detail Builder Pointer */
	 IDetailLayoutBuilder*  LastDetailBuilderPtr;

};
