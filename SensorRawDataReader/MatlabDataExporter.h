#pragma once

#include "DataReader.h"

class MatlabDataExporter
{
public:
	MatlabDataExporter();
	static void Export(DataReader& reader);
	static void Export(sensor_entity* sensor);
	~MatlabDataExporter();
};

