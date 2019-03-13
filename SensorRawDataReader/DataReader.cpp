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


//#define LITTLE_ENDIAN
#ifdef LITTLE_ENDIAN
#define ENDIAN_CAST(type, pointer) *reinterpret_cast<type *>(pointer)
#else
//#define ENDIAN_CAST(type, pointer) reinterpret_cast<type>(endian_inv(pointer))
//#define ENDIAN_CAST_DWORD endian_cast_dword
//#define ENDIAN_CAST_QWORD endian_cast_qword
//#define ENDIAN_CAST(type, pointer) (DWORD __endian_cast_tval = endian_inv(pointer), *reinterpret_cast<type*>(&__endian_cast_tval))
//#define ENDIAN_CAST_DWORD(pos, dst) *reinterpret_cast<DWORD*>(dst) = _byteswap_ulong(*reinterpret_cast<LONG*>(pos));
//#define ENDIAN_CAST_QWORD(pos, dst) *reinterpret_cast<UINT64*>(dst) = _byteswap_uint64(*reinterpret_cast<UINT64*>(pos));
DWORD inline CAST_DWORD(void* pos){
	return _byteswap_ulong(*reinterpret_cast<LONG*>(pos));
}
#define CAST_QWORD(pos) _byteswap_uint64(*reinterpret_cast<UINT64*>(pos))
#endif

//inline void endian_cast_dword(UCHAR* pos, void* dst) {
//	// from big endian
//	//DWORD val = endian_inv(pos);
//	//*reinterpret_cast<DWORD*>(dst) = *reinterpret_cast<DWORD*>(&val);
//	*reinterpret_cast<DWORD*>(dst) = _byteswap_ulong(*reinterpret_cast<LONG*>(pos));
//}

//inline void endian_cast_qword(UCHAR* pos, void* dst) {
//	*reinterpret_cast<UINT64*>(dst) = _byteswap_uint64(*reinterpret_cast<UINT64*>(pos));
//}

DWORD endian_inv(void* pos) {
	DWORD conv = 0;
	register UCHAR* pVal = reinterpret_cast<UCHAR*>(pos);
	conv |= *pVal++ << 24;
	conv |= *pVal++ << 16;
	conv |= *pVal++ << 8;
	conv |= *pVal;
	return conv;
}

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

	DWORD version = CAST_DWORD(pointer);

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

	DWORD info_size = CAST_DWORD(pointer);
	pointer += 4;
	PARSE_ASSERT_REVAL(BufferSufficient(pointer, info_size), -1)

	DWORD sensorCount = CAST_DWORD(pointer);
	pointer += 4;

	vector<sensor_entity> sensors(sensorCount);

	DWORD read_sensors_count = 0;

	while (read_sensors_count < sensorCount) {
		sensor_entity& sensor = sensors[read_sensors_count];

		PARSE_ASSERT_REVAL(BufferSufficient(pointer, 4), -1)

		DWORD entity_size = CAST_DWORD(pointer);
		pointer += 4;

		PARSE_ASSERT_REVAL(BufferSufficient(pointer, entity_size), -1)

		UCHAR* pointer_entity = pointer;

		sensor.id = CAST_DWORD(pointer_entity);
		pointer_entity += 4;

		DWORD data_dimension = CAST_DWORD(pointer_entity);
		sensor.data.SetDimension(data_dimension);
		pointer_entity += 4;

		ReadString(pointer_entity, sensor.name);
		pointer_entity += sensor.name.length();
		pointer_entity++; // TODO: skip zeros

		ReadString(pointer_entity, sensor.vendor_name);
		pointer_entity += sensor.vendor_name.length();
		pointer_entity++; // TODO: skip zeros

		sensor.version = CAST_DWORD(pointer_entity);
		pointer_entity += 4;

		sensor.type = CAST_DWORD(pointer_entity);
		pointer_entity += 4;

		sensor.maximum_range = *reinterpret_cast<float*>(CAST_DWORD(pointer_entity));
		pointer_entity += 4;

		sensor.resolution = CAST_DWORD(pointer_entity);
		pointer_entity += 4;

		sensor.power = CAST_DWORD(pointer_entity);
		pointer_entity += 4;

		sensor.min_delay = CAST_DWORD(pointer_entity);
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

	DWORD frameIndex = CAST_DWORD(pointer);
	pointer += 4;

	DWORD frameSize = CAST_DWORD(pointer);
	pointer += 4;

	PARSE_ASSERT_REVAL(BufferSufficient(pointer, frameSize), -1)

	DWORD groupCount = CAST_DWORD(pointer);
	pointer += 4;

	pointer += 8;//time


	int readGroup = 0;
	UCHAR* groupPointer = pointer;

	while (readGroup < (int) groupCount) { //TODO: use for.
		PARSE_ASSERT_REVAL(BufferSufficient(groupPointer, 8), -1)

		DWORD sensorId = CAST_DWORD(groupPointer);
		groupPointer += 4;

		DWORD data_count = CAST_DWORD(groupPointer);
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

	frame.size = CAST_DWORD(pointer);
	pointer += 4;// frameSize;

	DWORD groupCount = CAST_DWORD(pointer);
	frame.group_count = groupCount;
	pointer += 4;

	frame.time = CAST_QWORD(pointer);
	pointer += 8;// time

	int readGroup = 0;
	UCHAR* groupPointer = pointer;

	while (readGroup < (int)groupCount) {
		DWORD sensorId = CAST_DWORD(groupPointer);
		groupPointer += 4;

		DWORD data_count = CAST_DWORD(groupPointer);
		groupPointer += 4;

		if (data_count == 0) {
			continue;
		}
		SensorData<float>& data = MapToSensorEntity(sensorId).data;

		for (int i = 0; i < data_count; i++) {
			//ENDIAN_CAST_QWORD(groupPointer,
			//	reinterpret_cast<FLOAT *>(&currSensor->data_accuracy[currSensor->currDataIndex++]));
			UINT64 timestamp = CAST_QWORD(groupPointer);
			data.data_timestamp.push_back(timestamp);
			groupPointer += 8;// time

			float accuracy = CAST_DWORD(groupPointer);
			data.data_accuracy.push_back(accuracy);
			groupPointer += 4;// accuracy

			for (int j = 0; j < data.Dimension(); j++) {
				float dt = CAST_DWORD(groupPointer);
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
		GENERIC_READ,                          // 读文件
		FILE_SHARE_READ | FILE_SHARE_WRITE,    // 共享读写
		NULL,                                  // 缺省安全属性。
		OPEN_EXISTING,                         //
		FILE_ATTRIBUTE_NORMAL,                 //      
		NULL);                                 // 模板文件为空

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
