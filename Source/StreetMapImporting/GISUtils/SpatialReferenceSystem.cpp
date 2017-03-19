
#include "StreetMapImporting.h"
#include "SpatialReferenceSystem.h"

// Latitude/longitude scale factor
//			- https://en.wikipedia.org/wiki/Equator#Exact_length
static const double EarthCircumference = 40075036.0;
static const double EarthRadius = 6378137.0;
static const double LatitudeLongitudeScale = EarthCircumference / 360.0; // meters per degree
static const double InvLatitudeLongitudeScale = 1.0 / LatitudeLongitudeScale; // degrees per meter



FSpatialReferenceSystem::FSpatialReferenceSystem( const double OriginLongitude,
												const double OriginLatitude)
	: OriginLongitude(OriginLongitude)
	, OriginLatitude(OriginLatitude)
{
}

static double ConvertEPSG4326LongitudeToMeters(const double Longitude, const double Latitude)
{
	return Longitude * LatitudeLongitudeScale * cos(FMath::DegreesToRadians(Latitude));
};

static double ConvertEPSG4326LatitudeToMeters(const double Latitude)
{
	return -Latitude * LatitudeLongitudeScale;
};

FVector2D FSpatialReferenceSystem::FromEPSG4326(const double Longitude, const double Latitude) const
{
	return FVector2D(
		(float)(ConvertEPSG4326LongitudeToMeters(Longitude, Latitude) - ConvertEPSG4326LongitudeToMeters(OriginLongitude, Latitude)),
		(float)(ConvertEPSG4326LatitudeToMeters(Latitude) - ConvertEPSG4326LatitudeToMeters(OriginLatitude)));
};

void FSpatialReferenceSystem::ToEPSG4326(const FVector2D& Location, double& OutLongitude, double& OutLatitude) const
{
	OutLatitude = OriginLatitude - Location.Y * InvLatitudeLongitudeScale;
	OutLongitude = OriginLongitude;

	const double Cos = cos(FMath::DegreesToRadians(OutLatitude));
	if (Cos > 0.0)
	{
		OutLongitude += Location.X * InvLatitudeLongitudeScale / Cos;
	}
};

bool FSpatialReferenceSystem::ToEPSG3857(const FVector2D& Location, double& OutX, double& OutY) const
{
	// convert to lon/lat first
	ToEPSG4326(Location, OutX, OutY);

	if (OutY < -85.05112878 || OutY > 85.05112878)
	{
		return false;
	}

	OutX = FMath::DegreesToRadians(OutX) * EarthRadius;
	OutY = log(tan(FMath::DegreesToRadians(OutY) * 0.5 + PI * 0.25)) * EarthRadius;

	return true;
};
