#pragma once

#include "Landscape.h"
#include "StreetMap.h"

ALandscape* BuildLandscape(UStreetMapComponent* StreetMapComponent, const FStreetMapLandscapeBuildSettings& BuildSettings);
