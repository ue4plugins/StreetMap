#pragma once

/**
 * Transforms all points relative to the given longitude/latitude 
 * so that we get as much precision as possible and minimize projection distortion.
 * Locations/coordinates referred to as "local" are in Sanson-Flamsteed (sinusoidal) Projection (unit = meters)
 * (see http://www.progonos.com/furuti/MapProj/Normal/CartHow/HowSanson/howSanson.html)
 */
class FSpatialReferenceSystem
{
private:
	double OriginLongitude;
	double OriginLatitude;
public:
	FSpatialReferenceSystem(const double OriginLongitude, const double OriginLatitude);

	/** Converts WGS84 latitude and longitude (degrees) to local coordinates (meters). 
	 * (see http://spatialreference.org/ref/epsg/4326/)
	 */
	FVector2D FromEPSG4326(const double Longitude, const double Latitude) const;

	/** Converts local coordinates (meters) to WGS84 latitude and longitude (degrees). 
	 * (see http://spatialreference.org/ref/epsg/4326/)
	 */
	void ToEPSG4326(const FVector2D& Location, double& OutLongitude, double& OutLatitude) const;

	/** Convertes local coordinates (meters) into Web Mercator (pseudo meters).
	 * (see http://spatialreference.org/ref/sr-org/7483/) 
	 */
	void ToEPSG3857(const FVector2D& Location, double& OutX, double& OutY) const;

	inline double GetOriginLongitude() const { return OriginLongitude; }
	inline double GetOriginLatitude() const { return OriginLatitude; }
};
