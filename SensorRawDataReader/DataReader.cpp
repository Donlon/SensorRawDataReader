#include "pch.h"
#include "DataReader.h"

#include <iostream>

using namespace std;

DataReader::DataReader(LPCTSTR path) : m_file_data(nullptr), m_file_handle(nullptr)
{
	_tcscpy_s(m_file_path, _MAX_PATH, path);
}

void DataReader::Load() {
	DWORD retCode = _LoadFile();
	if (retCode == 0) {
		m_status = LOADED;
	} else {
		m_status = LOAD_FAILED;
	}
}

#ifdef __WIN64
	#define DEBUGGER_BP __asm{int 3};
#else
	#define DEBUGGER_BP throw;
#endif

#define PARSE_ASSERT_MSG(condition, msg)       \
if (!(condition)) {                            \
	DEBUGGER_BP                                  \
	OutputDebugString(_T(msg));                  \
	return FALSE;                                \
}

#define PARSE_ASSERT(condition)                \
if (!(condition)) {                            \
	DEBUGGER_BP                                  \
	return;                                      \
}

#define PARSE_ASSERT_REVAL(condition, retval)  \
if (!(condition)) {                            \
	DEBUGGER_BP                                  \
	return retval;                               \
}

#define assert_condition(condition) ((void)sizeof(char[1 - 2*!(condition)]))
//#define LITTLE_ENDIAN
#ifdef LITTLE_ENDIAN
#define ENDIAN_CAST(type, pointer) *reinterpret_cast<type *>(pointer)
#else
template<typename _Tp> static inline _Tp endian_cast(void* pos) {
	//static_assert(sizeof(_Tp) == 2 ||sizeof(_Tp) == 4 || sizeof(_Tp) == 8, "Support only 16-, 32- or 64-bit-sized type");
	assert_condition(sizeof(_Tp) == 2 || sizeof(_Tp) == 4 || sizeof(_Tp) == 8);

	if (sizeof(_Tp) == 2) {
		SHORT v = _byteswap_ushort(*reinterpret_cast<SHORT*>(pos));
		return *reinterpret_cast<_Tp*>(&v);
	} else if (sizeof(_Tp) == 4) {
		DWORD v = _byteswap_ulong(*reinterpret_cast<DWORD*>(pos));
		return *reinterpret_cast<_Tp*>(&v);
	}	else if (sizeof(_Tp) == 8) {
		INT64 v = _byteswap_uint64(*reinterpret_cast<INT64*>(pos));
		return *reinterpret_cast<_Tp*>(&v);
	} else {
		//static_assert(false);
	}
}
#endif

void print_sensor_info(sensor_entity &sensor) {
#ifdef _DEBUG
	cout << "Sensor Name: " << sensor.name << endl;
	cout << "Sensor Vendor: " << sensor.vendor_name << endl;
	
	cout << "Version: " << sensor.version << endl;
	cout << "Type: " << sensor.type << endl;
	cout << "MaximumRange: " << sensor.maximum_range << endl;
	cout << "Resolution: " << sensor.resolution << endl;
	cout << "Power: " << sensor.power << endl;
	cout << "MinDelay: " << sensor.min_delay << endl;
	cout << endl;
#endif
}

BOOL DataReader::Parse() {
	m_status = PARSE_FAILED;

	UCHAR* pointer = m_file_data;

	PARSE_ASSERT_REVAL(memcmp(pointer, "DonlonDataFile\0\0", 16) == 0, FALSE)
	pointer += 16;
	PARSE_ASSERT_REVAL(memcmp(pointer, "\x12\x34\x56\x78\0\0\0\0\0\0\0\0", 12) == 0, FALSE);
	pointer += 12;

	DWORD version = endian_cast<DWORD>(pointer);

	PARSE_ASSERT_REVAL(version == 1, FALSE);

	pointer += 4;
	
	int sensorsInfoSize = ParseSensorsInfo(pointer);
	PARSE_ASSERT_REVAL(sensorsInfoSize >= 0, FALSE)

	pointer += sensorsInfoSize;

	PARSE_ASSERT_REVAL(ProbeRecordedData(pointer), FALSE)

	vector<recode_frame> frames(m_frame_count);
	for (int i = 0; i < m_frame_count; i++) {
		recode_frame& frame = frames[i];
		pointer += 16;

		frame.index = i;
		frame.pos_offset = pointer - m_file_data;

		DWORD frameSize = ParseFrame(pointer, frame);
		pointer += frameSize;
	}

	frames.swap(m_frames);

#if 0 && defined _DEBUG
	puts("");

	for (int i = 0; i < m_sensor_count; i++, putchar('\n')) {
		sensor_entity& sensor = m_sensors[i];
		cout << sensor.name << endl;
		SensorData<float>& data = sensor.data;

		for (int i = 0; i < data.Size(); i++, putchar('\n')) {
			printf("%d: %.2f", i, data.data_accuracy[i]);
			for (int j = 0; j < sensor.data.Dimension() && (putchar(' '), 1); j++) {
				printf("%.2f", data.At(j, i));
			}
		}
	}
#endif
	//m_sensors.resize(2);
	//m_sensors.resize(10);
	//causes errors
	
	m_status = PARSED;
	return TRUE;
}

int DataReader::ParseSensorsInfo(UCHAR* base)
{
	UCHAR* pointer = base;

	DWORD info_size = endian_cast<DWORD>(pointer);
	pointer += 4;
	PARSE_ASSERT_REVAL(BufferSufficient(pointer, info_size), -1)

	DWORD sensorCount = endian_cast<DWORD>(pointer);
	pointer += 4;

	vector<sensor_entity> sensors(sensorCount);

	DWORD read_sensors_count = 0;

	while (read_sensors_count < sensorCount) {
		sensor_entity& sensor = sensors[read_sensors_count];

		PARSE_ASSERT_REVAL(BufferSufficient(pointer, 4), -1)

		DWORD entity_size = endian_cast<DWORD>(pointer);
		pointer += 4;

		PARSE_ASSERT_REVAL(BufferSufficient(pointer, entity_size), -1)

		UCHAR* pointer_entity = pointer;

		sensor.id = endian_cast<DWORD>(pointer_entity);
		pointer_entity += 4;

		DWORD data_dimension = endian_cast<DWORD>(pointer_entity);
		sensor.data.SetDimension(data_dimension);
		pointer_entity += 4;

		ReadString(pointer_entity, sensor.name);
		pointer_entity += sensor.name.length();
		pointer_entity++; // TODO: skip zeros

		ReadString(pointer_entity, sensor.vendor_name);
		pointer_entity += sensor.vendor_name.length();
		pointer_entity++; // TODO: skip zeros

		sensor.version = endian_cast<DWORD>(pointer_entity);
		pointer_entity += 4;

		sensor.type = endian_cast<DWORD>(pointer_entity);
		pointer_entity += 4;

		sensor.maximum_range = endian_cast<FLOAT>(pointer_entity);
		pointer_entity += 4;

		sensor.resolution = endian_cast<FLOAT>(pointer_entity);
		pointer_entity += 4;

		sensor.power = endian_cast<FLOAT>(pointer_entity);
		pointer_entity += 4;

		sensor.min_delay = endian_cast<DWORD>(pointer_entity);
		pointer_entity += 4;

		print_sensor_info(sensor);

		PARSE_ASSERT_REVAL((DWORD)(pointer_entity - pointer) <= entity_size, -1)//TODO free buf

		sensor.index = read_sensors_count;

		pointer = pointer_entity;

		read_sensors_count++;
	}

	if ((DWORD)(pointer - base) <= info_size) {
		m_sensor_count = read_sensors_count;
		sensors.swap(m_sensors);
		return info_size + 4;
	} else {
		throw;
	}
}

template<class T> T* deconst(const T* obj) {
	return const_cast<T*>(obj);
}

void DataReader::ReadString(BYTE* base, string& str)
{
	BYTE *pos = base;
	while (*pos != 0) {
		pos++;
	}
	DWORD l = (DWORD) (pos - base);
	if (l == 0) {
		str.clear();
		return;
	}
	str.resize(l);
	//LPSTR str = new CHAR[l + 1];
	memcpy(const_cast<char*>(str.data()), base, l + 1);
}

BOOL inline DataReader::BufferSufficient(UCHAR * base, DWORD bytesToRead) {
	return (DWORD)(base + bytesToRead - m_file_data) <= m_file_size;
}

BOOL DataReader::ProbeRecordedData(UCHAR * base)
{
	UCHAR* pointer = base;
	int frameCount = 0;
	while (BufferSufficient(pointer, 16)) { //separator
		pointer += 16;
		int readlen = ProbeFrame(pointer);
		
		PARSE_ASSERT_REVAL(readlen >= 0, FALSE)
		
		frameCount++;
		pointer += readlen;
	}// Read each frame

	m_frame_count = frameCount;
	
#ifdef _DEBUG
	puts("Probe result:");
	for (int i = 0; i < m_sensor_count; i++) {
		printf("  count:%6zd  name:%s\n", m_sensors[i].data.Size(), m_sensors[i].name.c_str());
	}
#endif

	return TRUE;
}

// base should refer to validated frame pos.
int DataReader::ProbeFrame(UCHAR* base)
{
	PARSE_ASSERT_REVAL(BufferSufficient(base, 20), -1)//TODO: merge 20 with 16
	UCHAR* pointer = base;

	DWORD frameIndex = endian_cast<DWORD>(pointer);
	pointer += 4;

	DWORD frameSize = endian_cast<DWORD>(pointer);
	pointer += 4;

	PARSE_ASSERT_REVAL(BufferSufficient(pointer, frameSize), -1)

	DWORD groupCount = endian_cast<DWORD>(pointer);
	pointer += 4;

	pointer += 8;//time


	int readGroup = 0;
	UCHAR* groupPointer = pointer;

	while (readGroup < (int) groupCount) { //TODO: use for.
		PARSE_ASSERT_REVAL(BufferSufficient(groupPointer, 8), -1)

		DWORD sensorId = endian_cast<DWORD>(groupPointer);
		groupPointer += 4;

		DWORD data_count = endian_cast<DWORD>(groupPointer);
		groupPointer += 4;


		if (data_count != 0) {
			SensorData<float>& data = MapToSensorEntity(sensorId).data;
			data.SetSize(data_count);

			DWORD groupSize = data_count * (8 + 4 + 4 * data.Dimension());

			PARSE_ASSERT_REVAL(BufferSufficient(groupPointer, groupSize), -1)

			groupPointer += groupSize;
			readGroup++;
		} else {
			printf("");
		}
	}

	PARSE_ASSERT_REVAL((DWORD)(groupPointer - pointer) + 8 + 4 == frameSize, -1);

	return (DWORD)(groupPointer - base);
}

sensor_entity& DataReader::MapToSensorEntity(DWORD sensorId) {
	for (int i = 0; i < m_sensors.size(); i++) {
		if (m_sensors[i].id == sensorId) {
			return m_sensors[i];
		}
	}
	throw;
}

int DataReader::ParseFrame(UCHAR* base, recode_frame& frame)
{
	UCHAR* pointer = base;

	pointer += 4;// frameIndex;

	frame.size = endian_cast<DWORD>(pointer);
	pointer += 4;// frameSize;

	DWORD groupCount = endian_cast<DWORD>(pointer);
	frame.group_count = groupCount;
	pointer += 4;

	frame.time = endian_cast<DWORD>(pointer);
	pointer += 8;// time

	int readGroup = 0;
	UCHAR* groupPointer = pointer;

	while (readGroup < (int)groupCount) {
		DWORD sensorId = endian_cast<DWORD>(groupPointer);
		groupPointer += 4;

		DWORD data_count = endian_cast<DWORD>(groupPointer);
		groupPointer += 4;

		if (data_count == 0) {
			continue;
		}
		SensorData<float>& data = MapToSensorEntity(sensorId).data;

		for (int i = 0; i < data_count; i++) {
			//ENDIAN_CAST_QWORD(groupPointer,
			//	reinterpret_cast<FLOAT *>(&currSensor->data_accuracy[currSensor->currDataIndex++]));
			time_t timestamp = endian_cast<time_t>(groupPointer);
			data.data_timestamp.push_back(timestamp);
			groupPointer += 8;// time

			float accuracy = endian_cast<FLOAT>(groupPointer);
			data.data_accuracy.push_back(accuracy);
			groupPointer += 4;// accuracy

			for (int j = 0; j < data.Dimension(); j++) {
				float dt = endian_cast<FLOAT>(groupPointer);
				data.data[j].push_back(dt);
				groupPointer += 4;
			}
		}
		readGroup++;
	}
	return (groupPointer - base);
}

DataReader::~DataReader() {
	::CloseHandle(m_file_handle);
	ptr_free(m_file_data)
}

int DataReader::_LoadFile() {
	m_file_handle = ::CreateFile(m_file_path,    // 名称
		GENERIC_READ,                              // 读文件
		FILE_SHARE_READ | FILE_SHARE_WRITE,        // 共享读写
		NULL,                                      // 缺省安全属性。
		OPEN_EXISTING,                             //
		FILE_ATTRIBUTE_NORMAL,                     //      
		NULL);                                     // 模板文件为空

	if (m_file_handle == INVALID_HANDLE_VALUE) {
		throw "CreateFile failed!";
	}

	//从文件里读取数据。
	LONG lDistance = 0;
	DWORD dwPtr = ::SetFilePointer(m_file_handle, lDistance, NULL, FILE_BEGIN);

	if (dwPtr == INVALID_SET_FILE_POINTER) throw;

	DWORD size = ::GetFileSize(m_file_handle, NULL);

	if (size == 0) throw;

	BYTE* buffer = new BYTE[size];

	DWORD dwReadSize = 0;
	BOOL bRet = ::ReadFile(m_file_handle, buffer, size, &dwReadSize, NULL);

	if (!bRet) throw;
	if (size != dwReadSize) throw;

	m_file_size = dwReadSize;

	m_file_data = buffer;

	return 0;
}
