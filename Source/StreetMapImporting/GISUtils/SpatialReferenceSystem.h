#pragma once

/**
 * Transforms all points relative to the given longitude/latitude 
 * so that we get as much precision as possible and minimize projection distortion.
 * Applies Sanson-Flamsteed (sinusoidal) Projection 
 * (see http://www.progonos.com/furuti/MapProj/Normal/CartHow/HowSanson/howSanson.html)
 */
class FSpatialReferenceSystem
{
private:
	double OriginLongitude;
	double OriginLatitude;
public:
	FSpatialReferenceSystem(const double OriginLongitude, const double OriginLatitude);

	FVector2D FromEPSG4326(const double Longitude, const double Latitude) const;
};
