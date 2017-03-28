// Copyright 2017 Mike Fricker. All Rights Reserved.
#pragma once

#include "FastXml.h"
#include "GISUtils/SpatialReferenceSystem.h"


/** OpenStreetMap file loader */
class FOSMFile : public IFastXmlCallback
{
	
public:
	
	/** Default constructor for FOSMFile */
	FOSMFile();

	/** Destructor for FOSMFile */
	virtual ~FOSMFile();

	/** Loads the map from an OpenStreetMap XML file.  Note that in the case of the file path containing the XML data, the string must be mutable for us to parse it quickly. */
	bool LoadOpenStreetMapFile( FString& OSMFilePath, const bool bIsFilePathActuallyTextBuffer, class FFeedbackContext* FeedbackContext );


	struct FOSMWayInfo;

	/** Types of ways */
	enum class EOSMWayType
	{
		/** Used for identifying any kind of road, street or path. */
		Highway,

		/** Used to mark areas as a building. */
		Building,

		/** The leisure tag is for places people go in their spare time (e.g. parks, pitches). */
		Leisure,

		/** Used to describe natural and physical land features (e.g. wood, beach, water). */
		Natural,

		/** Used to describe the primary use of land by humans (e.g. grass, meadow, forest). */
		LandUse,

		/** Currently unrecognized type */
		Other,
	};


	struct FOSMWayRef
	{
		// Way that we're referencing at this node
		FOSMWayInfo* Way;
			
		// Index of the node in the way's array of nodes
		int32 NodeIndex;
	};

		
	struct FOSMNodeInfo
	{
		double Latitude;
		double Longitude;
		TArray<FOSMWayRef> WayRefs;
	};
		

	struct FOSMWayInfo
	{
		FString Name;
		FString Ref;
		
		TArray<FOSMNodeInfo*> Nodes;
		EOSMWayType WayType;
		/** subtype according to WayType */
		FString Category;

		///
		/// BUILDING
		///
		double Height;
		int32 BuildingLevels;

		///
		/// HIGHWAY
		///
		/** If true, way is only traversable in the order the nodes are listed in the Nodes list */
		uint8 bIsOneWay : 1;
	};

	// Minimum latitude/longitude bounds
	double MinLatitude = MAX_dbl;
	double MinLongitude = MAX_dbl;
	double MaxLatitude = -MAX_dbl;
	double MaxLongitude = -MAX_dbl;

	// Average Latitude (roughly the center of the map)
	double AverageLatitude = 0.0;
	double AverageLongitude = 0.0;

	FSpatialReferenceSystem SpatialReferenceSystem;

	// All ways we've parsed
	TArray<FOSMWayInfo*> Ways;
		
	// Maps node IDs to info about each node
	TMap<int64, FOSMNodeInfo*> NodeMap;

protected:

	// IFastXmlCallback overrides
	virtual bool ProcessXmlDeclaration( const TCHAR* ElementData, int32 XmlFileLineNumber ) override;
	virtual bool ProcessComment( const TCHAR* Comment ) override;
	virtual bool ProcessElement( const TCHAR* ElementName, const TCHAR* ElementData, int32 XmlFileLineNumber ) override;
	virtual bool ProcessAttribute( const TCHAR* AttributeName, const TCHAR* AttributeValue ) override;
	virtual bool ProcessClose( const TCHAR* Element ) override;

	
protected:
	
	enum class ParsingState
	{
		Root,
		Node,
		Way,
		Way_NodeRef,
		Way_Tag
	};
		
	// Current state of parser
	ParsingState ParsingState;
		
	// ID of node that is currently being parsed
	int64 CurrentNodeID;
		
	// Node that is currently being parsed
	FOSMNodeInfo* CurrentNodeInfo;
		
	// Way that is currently being parsed
	FOSMWayInfo* CurrentWayInfo;
		
	// Current way's tag key string
	const TCHAR* CurrentWayTagKey;
};


