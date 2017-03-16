#pragma once

#include "Landscape.h"
#include "StreetMap.h"

ALandscape* BuildLandscape(UStreetMapComponent* StreetMapComponent, UWorld* World, const FStreetMapLandscapeBuildSettings& BuildSettings);
