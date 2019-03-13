#include "pch.h"

#include "MatlabDataExporter.h"

#include <mat.h>
#include <mex.h>
#include <iostream>

#pragma comment(lib,"libmat.lib")
#pragma comment(lib,"libmx.lib")

using namespace std;

void AppendToMatFile(SensorData<FLOAT>& data, MATFile *pmat, LPCSTR varName);

MATFile* CreateMatFile(LPCSTR path) {
	return matOpen(path, "w");
}
bool CloseMatFile(MATFile* pmat) {
	return matClose(pmat) == 0;
}

void MatlabDataExporter::Export(DataReader& reader)
{
	MATFile *pmat = CreateMatFile("D:\\Links\\Documents\\MATLAB\\testmat.mat");
	if (pmat == NULL) {
		cout << "Error in creating mat file." << endl;
		//EXIT_FAILURE
		return;
	}

	if (reader.m_status == DataReader::PARSED && reader.GetSensorCount() > 0)
	{
		int index = 0;
		for (sensor_entity& sensor : reader.GetSensors())
		{
			string varName = "sensor_data_";
			varName.append(to_string(index));
			AppendToMatFile(sensor.data, pmat, varName.c_str());
			index++;
		}
	}

	CloseMatFile(pmat);
}

void MatlabDataExporter::Export(sensor_entity& sensor) {
	throw "Not implemented yet.";
}

void AppendToMatFile(SensorData<FLOAT>& data, MATFile *pmat, LPCSTR varName)
{
	int data_count = data.Size();
	//mxArray* arr = mxCreateDoubleMatrix(sensor->data_dimension, sensor->data_count, mxREAL);
	mxArray* arr = mxCreateNumericMatrix(data.Dimension(), data_count, mxSINGLE_CLASS, mxREAL);
	mxSingle* arr_data = reinterpret_cast<mxSingle*>(mxMalloc(data.Dimension() * data_count * sizeof(mxSingle)));
	
	for (int i = 0; i < data.Dimension(); i++) {
		for (int j = 0; j < data_count; j++) {
			//mxSetSingles(sensor->data[i][j],);
			arr_data[i * data_count + j] = data.At(i, j);
		}
	}

	mxSetSingles(arr, arr_data);

	if (matPutVariable(pmat, "SensorData", arr) != 0) {
		cout << "Error using matPutVariable" << endl;
		return;
	}

	mxDestroyArray(arr);
}