#pragma once
#include "pch.h"

#include <iostream>
#include <string>
#include <vector>

using namespace std;

class sensor_entity {
public:
	DWORD index;
	DWORD id;

	DWORD name_len;
	DWORD vendor_name_len;
	string name;
	string vendor_name;

	DWORD version;
	DWORD type;
	FLOAT maximum_range;
	FLOAT resolution;
	FLOAT power;
	DWORD min_delay;

	DWORD	data_dimension;
	FLOAT** data;
	vector<FLOAT> data_accuracy;
	vector<time_t> data_timestamp;

	DWORD data_count;
	DWORD curr_data_index;

	sensor_entity() : index(),
		id(),
		name_len(),
		vendor_name_len(),
		name(),
		vendor_name(),
		version(),
		type(),
		maximum_range(),
		resolution(),
		power(),
		min_delay(),
		data_dimension(),
		data(),
		data_accuracy(),
		data_timestamp(),
		data_count(),
		curr_data_index()
	{
		static int count = 0;
		cout << "construct " << id << ", index " << count << ", this=0x" << reinterpret_cast<void*>(this) << endl;
		count++;
	}

	~sensor_entity() {
		cout << "destroy " << id << ", index "<< index << ", this=0x" << reinterpret_cast<void*>(this) << endl;
		for (int i = 0; i < data_dimension; i++) {
			ptr_free(data[i])
		}
		ptr_free(data)
	}
};

class recode_frame {
public:
	DWORD index;
	DWORD size;

	time_t time;
	DWORD group_count;

	DWORD pos_offset;

	recode_frame() : index(), size(), time(), group_count(), pos_offset() {}
};

struct recode_data_item {
	DWORD index;
	DWORD size;
};

DWORD endian_inv(void* pos);

void print_sensor_info(sensor_entity& sensor);

class DataReader
{
	TCHAR m_file_path[_MAX_PATH] = {0};
	HANDLE m_file_handle = INVALID_HANDLE_VALUE;
	DWORD m_file_size = 0;
	BYTE* m_file_data = nullptr;

	int m_sensor_count = 0;
	int m_frame_count = 0;
	vector<sensor_entity> m_sensors;
	vector<recode_frame> m_frames;

public:
	enum Status {
		X,
		LOADED,
		LOAD_FAILED,
		PARSED,
		PARSE_FAILED,
		//SUCCEED
	} m_status = X;

	DataReader(LPCTSTR path);

	void Load();

	BOOL Parse();

	int GetSensorCount() {
		return m_sensor_count;
	}

	int GetDataCount() {
		return m_file_size;
	}
	vector<sensor_entity>& GetSensors() {
		return m_sensors;
	}

	~DataReader();

private:
	int _LoadFile();

	int ParseSensorsInfo(UCHAR* base);

	int ParseFrame(UCHAR* base, recode_frame& frame);

	void ReadString(UCHAR* base, string& str);

	BOOL inline BufferSufficient(UCHAR* base, DWORD bytesToRead);

	BOOL ProbeRecordedData(UCHAR* base);

	int ProbeFrame(UCHAR * base);

	sensor_entity* MapToSensorEntity(DWORD sensorId);

};