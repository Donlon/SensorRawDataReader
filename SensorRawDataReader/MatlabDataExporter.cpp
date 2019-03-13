#include "pch.h"

#include "MatlabDataExporter.h"

#include <mat.h>
#include <mex.h>
#include <iostream>

#pragma comment(lib,"libmat.lib")
#pragma comment(lib,"libmx.lib")

using namespace std;

MatlabDataExporter::MatlabDataExporter()
{
}

void MatlabDataExporter::Export(DataReader& reader)
{
	if (reader.m_status == DataReader::PARSED && reader.GetSensorCount() > 0) {
		Export(&reader.GetSensors()[3]);
	}
}

void MatlabDataExporter::Export(sensor_entity * sensor)
{
	MATFile *pmat = matOpen("D:\\Links\\Documents\\MATLAB\\testmat.mat", "w");
	if (pmat == NULL) {
		cout << "Error in creating mat file." << endl;
		//EXIT_FAILURE
		return;
	}
	//mxArray* arr = mxCreateDoubleMatrix(sensor->data_dimension, sensor->data_count, mxREAL);
	mxArray* arr = mxCreateNumericMatrix(sensor->data_dimension, sensor->data_count, mxSINGLE_CLASS, mxREAL);
	mxSingle* arr_data = reinterpret_cast<mxSingle*>(mxMalloc(sensor->data_dimension * sensor->data_count * sizeof(mxSingle)));
	
	for (int i = 0; i < sensor->data_dimension; i++) {
		for (int j = 0; j < sensor->data_count; j++) {
			//mxSetSingles(sensor->data[i][j],);
			arr_data[i * sensor->data_count + j] = sensor->data[i][j];
		}
	}

	mxSetSingles(arr, arr_data);

	if (matPutVariable(pmat, "SensorData", arr) != 0) {
		cout << "Error using matPutVariable" << endl;
		return;
	}

	mxDestroyArray(arr);

	if (matClose(pmat) != 0) {
		cout << "Error closing MAT file" << endl;
		return;
	}
}


MatlabDataExporter::~MatlabDataExporter()
{
}
