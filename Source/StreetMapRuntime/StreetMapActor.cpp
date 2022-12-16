#include "StreetMapActor.h"
#include "StreetMapComponent.h"

AStreetMapActor::AStreetMapActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	StreetMapComponent = CreateDefaultSubobject<UStreetMapComponent>(TEXT("StreetMapComp"));
	RootComponent = StreetMapComponent;
}