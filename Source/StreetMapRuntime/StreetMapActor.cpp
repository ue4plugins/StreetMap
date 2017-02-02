// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "StreetMapRuntime.h"
#include "StreetMapActor.h"
#include "StreetMapComponent.h"


AStreetMapActor::AStreetMapActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	StreetMapComponent = CreateDefaultSubobject<UStreetMapComponent>(TEXT("StreetMapComp"));
	RootComponent = StreetMapComponent;
}