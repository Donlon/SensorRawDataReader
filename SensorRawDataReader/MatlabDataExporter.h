#pragma once

#include "DataReader.h"

namespace MatlabDataExporter
{
	void Export(DataReader& reader);
	void Export(sensor_entity& sensor);
};

