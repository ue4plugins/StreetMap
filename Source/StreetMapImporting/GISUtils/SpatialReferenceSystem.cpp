
#include "StreetMapImporting.h"
#include "SpatialReferenceSystem.h"

// Latitude/longitude scale factor
//			- https://en.wikipedia.org/wiki/Equator#Exact_length
static const double EarthCircumference = 40075036.0;
static const double LatitudeLongitudeScale = EarthCircumference / 360.0; // meters per degree



FSpatialReferenceSystem::FSpatialReferenceSystem( const double OriginLongitude,
												const double OriginLatitude)
	: OriginLongitude(OriginLongitude)
	, OriginLatitude(OriginLatitude)
{
}


// Converts longitude to meters
static double ConvertEPSG4326LongitudeToMeters(const double Longitude, const double Latitude)
{
	return Longitude * LatitudeLongitudeScale * FMath::Cos(FMath::DegreesToRadians(Latitude));
};
// Converts latitude to meters
static double ConvertEPSG4326LatitudeToMeters(const double Latitude)
{
	return -Latitude * LatitudeLongitudeScale;
};

// Converts latitude and longitude to this SRS' X/Y coordinates
FVector2D FSpatialReferenceSystem::FromEPSG4326(const double Longitude, const double Latitude) const
{
	return FVector2D(
		(float)(ConvertEPSG4326LongitudeToMeters(Longitude, Latitude) - ConvertEPSG4326LongitudeToMeters(OriginLongitude, Latitude)),
		(float)(ConvertEPSG4326LatitudeToMeters(Latitude) - ConvertEPSG4326LatitudeToMeters(OriginLatitude)));
};