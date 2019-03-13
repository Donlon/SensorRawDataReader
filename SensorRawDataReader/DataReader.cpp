#include "pch.h"
#include "DataReader.h"

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
	#define DEBUGGER_BP 
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
#define ENDIAN_CAST_DWORD(pos, dst) *reinterpret_cast<DWORD*>(dst) = _byteswap_ulong(*reinterpret_cast<LONG*>(pos));
#define ENDIAN_CAST_QWORD(pos, dst) *reinterpret_cast<UINT64*>(dst) = _byteswap_uint64(*reinterpret_cast<UINT64*>(pos));
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

void print_sensor_info(sensor_entity *sensor) {
#ifdef _DEBUG
	if (sensor->name) {
		cout << "Sensor Name: " << sensor->name << endl;
	}
	else {
		cout << "Sensor Name: (null)" << endl;
	}
	if (sensor->vendor_name) {
		cout << "Sensor Vendor: " << sensor->vendor_name << endl;
	}
	else {
		cout << "Sensor Vendor: (null)" << endl;
	}
	cout << "Version: " << sensor->version << endl;
	cout << "Type: " << sensor->type << endl;
	cout << "MaximumRange: " << sensor->maximum_range << endl;
	cout << "Resolution: " << sensor->resolution << endl;
	cout << "Power: " << sensor->power << endl;
	cout << "MinDelay: " << sensor->min_delay << endl;
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

	DWORD version;
	ENDIAN_CAST_DWORD(pointer, &version);

	PARSE_ASSERT_REVAL(version == 1, FALSE);

	pointer += 4;
	
	int sensorsInfoSize = ParseSensorsInfo(pointer);
	PARSE_ASSERT_REVAL(sensorsInfoSize >= 0, FALSE)

	pointer += sensorsInfoSize;

	PARSE_ASSERT_REVAL(ProbeRecordedData(pointer), FALSE)
	
	sensor_entity* s1 = m_sensors; 
	for (int s_i = 0; s_i < m_sensor_count; s_i++) {
		s1->data_timestamp = new time_t[s1->data_count]();
		s1->data_accuracy = new FLOAT[s1->data_count]();
		s1->data = new FLOAT*[s1->data_dimension]();
		for (int i = 0; i < s1->data_dimension; i++) {
			s1->data[i] = new FLOAT[s1->data_count]();
		}
		s1++;
	}

	recode_frame* frame_recodes = new recode_frame[m_frame_count];
	recode_frame* frame_recodes_pt = frame_recodes;

	for (int frame_i = 0; frame_i < m_frame_count; frame_i++, frame_recodes_pt++) {
		pointer += 16;

		frame_recodes_pt->index = frame_i;
		frame_recodes_pt->pos_offset = pointer - m_file_data;

		DWORD frameSize = ParseFrame(pointer, frame_recodes_pt);
		pointer += frameSize;
	}

	m_frames = frame_recodes;

#ifdef _DEBUG
	puts("");

	sensor_entity* s2 = m_sensors;
	for (int s_i = 0; s_i < m_sensor_count; s_i++, putchar('\n')) {
		puts(s2->name);
		for (int i = 0; i < s2->data_count; i++, putchar('\n')) {
			printf("%d: %.2f", i, s2->data_accuracy[i]);
			for (int j = 0; j < s2->data_dimension && (putchar(' '), 1); j++) {
				printf("%.2f", s2->data[j][i]);
			}
		}
		s2++;
	}
#endif

	m_status = PARSED;
	return TRUE;
}

int DataReader::ParseSensorsInfo(UCHAR* base)
{
	UCHAR* pointer = base;

	DWORD info_size;
	ENDIAN_CAST_DWORD(pointer, &info_size);
	pointer += 4;
	PARSE_ASSERT_REVAL(BufferSufficient(pointer, info_size), -1)

	DWORD sensorCount;
	ENDIAN_CAST_DWORD(pointer, &sensorCount);
	pointer += 4;

	m_sensors = new sensor_entity[sensorCount];

	sensor_entity *curr_sensor = m_sensors;

	DWORD read_sensors_count = 0;
	while (read_sensors_count < sensorCount) {

		PARSE_ASSERT_REVAL(BufferSufficient(pointer, 4), -1)

		DWORD entity_size;
		ENDIAN_CAST_DWORD(pointer, &entity_size);
		pointer += 4;

		PARSE_ASSERT_REVAL(BufferSufficient(pointer, entity_size), -1)

		UCHAR* pointer_entity = pointer;

		ENDIAN_CAST_DWORD(pointer_entity, &curr_sensor->id);
		pointer_entity += 4;

		ENDIAN_CAST_DWORD(pointer_entity, &curr_sensor->data_dimension);
		pointer_entity += 4;

		curr_sensor->name = ReadString(pointer_entity, &curr_sensor->name_len);
		pointer_entity += curr_sensor->name_len;
		pointer_entity++; // TODO: skip zeros

		curr_sensor->vendor_name = ReadString(pointer_entity, &curr_sensor->vendor_name_len);
		pointer_entity += curr_sensor->vendor_name_len;
		pointer_entity++; // TODO: skip zeros

		ENDIAN_CAST_DWORD(pointer_entity, &curr_sensor->version);
		pointer_entity += 4;

		ENDIAN_CAST_DWORD(pointer_entity, &curr_sensor->type);
		pointer_entity += 4;

		ENDIAN_CAST_DWORD(pointer_entity, &curr_sensor->maximum_range);
		pointer_entity += 4;

		ENDIAN_CAST_DWORD(pointer_entity, &curr_sensor->resolution);
		pointer_entity += 4;

		ENDIAN_CAST_DWORD(pointer_entity, &curr_sensor->power);
		pointer_entity += 4;

		ENDIAN_CAST_DWORD(pointer_entity, &curr_sensor->min_delay);
		pointer_entity += 4;

		print_sensor_info(curr_sensor);

		PARSE_ASSERT_REVAL((DWORD)(pointer_entity - pointer) <= entity_size, -1)//TODO free buf

		curr_sensor->index = read_sensors_count;

		pointer = pointer_entity;

		read_sensors_count++;
		curr_sensor++;
	}
	m_sensor_count = read_sensors_count;

	if ((DWORD)(pointer - base) <= info_size) {
		return info_size + 4;
	} else {
		//TODO free buf
		return -1;
	}
}

char* DataReader::ReadString(UCHAR* base, DWORD* read_length)
{
	UCHAR *pos = base;
	DWORD l = 0;
	while (*pos != 0) {
		l++;
		pos++;
	}
	if (l == 0) {
		*read_length = 0;
		return nullptr;
	}

	LPSTR str = new CHAR[l + 1];

	memcpy(str, base, l + 1);
	*read_length = l;
	return str;
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
		printf("  count:%6d  name:%s\n", m_sensors[i].data_count, m_sensors[i].name);
	}
#endif

	return TRUE;
}

// base should refer to validated frame pos.
int DataReader::ProbeFrame(UCHAR* base)
{
	PARSE_ASSERT_REVAL(BufferSufficient(base, 20), -1)//TODO: merge 20 with 16
	UCHAR* pointer = base;

	int frameIndex = 0;
	ENDIAN_CAST_DWORD(pointer, reinterpret_cast<DWORD *>(&frameIndex));
	pointer += 4;

	DWORD frameSize = 0;
	ENDIAN_CAST_DWORD(pointer, reinterpret_cast<DWORD *>(&frameSize));
	pointer += 4;

	PARSE_ASSERT_REVAL(BufferSufficient(pointer, frameSize), -1)

	DWORD groupCount = 0;
	ENDIAN_CAST_DWORD(pointer, reinterpret_cast<DWORD *>(&groupCount));
	pointer += 4;

	pointer += 8;//time


	int readGroup = 0;
	UCHAR* groupPointer = pointer;

	while (readGroup < (int) groupCount) { //TODO: use for.
		DWORD sensorId = 0;
		ENDIAN_CAST_DWORD(groupPointer, reinterpret_cast<DWORD *>(&sensorId));
		groupPointer += 4;

		DWORD data_count = 0;
		ENDIAN_CAST_DWORD(groupPointer, reinterpret_cast<DWORD *>(&data_count));
		groupPointer += 4;

		if (data_count == 0) {
			continue;
		}
		sensor_entity* currSensor = MapToSensorEntity(sensorId);
		PARSE_ASSERT_REVAL(currSensor, -1);
		currSensor->data_count += data_count;

		DWORD groupSize = data_count * (8 + 4 + 4 * currSensor->data_dimension);
		groupPointer += groupSize;
		readGroup++;
	}

	PARSE_ASSERT_REVAL((DWORD)(groupPointer - pointer) + 8 + 4 == frameSize, -1);

	return (DWORD)(groupPointer - base);
}

sensor_entity* DataReader::MapToSensorEntity(DWORD sensorId) {
	sensor_entity* s = m_sensors;
	for (int i = 0; i < m_sensor_count; i++, s++) {
		if (s->id == sensorId) {
			return s;
		}
	}
	return nullptr;
}

int DataReader::ParseFrame(UCHAR* base, recode_frame* frame)
{
	UCHAR* pointer = base;

	pointer += 4;// frameIndex;

	ENDIAN_CAST_DWORD(pointer, reinterpret_cast<DWORD *>(&frame->size));
	pointer += 4;// frameSize;

	DWORD groupCount = 0;
	ENDIAN_CAST_DWORD(pointer, reinterpret_cast<DWORD *>(&groupCount));
	frame->group_count = groupCount;
	pointer += 4;

	ENDIAN_CAST_QWORD(pointer, reinterpret_cast<UINT64 *>(&frame->time));
	pointer += 8;// time

	int readGroup = 0;
	UCHAR* groupPointer = pointer;

	while (readGroup < (int)groupCount) {
		DWORD sensorId = 0;
		ENDIAN_CAST_DWORD(groupPointer, reinterpret_cast<DWORD *>(&sensorId));
		groupPointer += 4;

		DWORD data_count = 0;
		ENDIAN_CAST_DWORD(groupPointer, reinterpret_cast<DWORD *>(&data_count));
		groupPointer += 4;

		if (data_count == 0) {
			continue;
		}
		sensor_entity* currSensor = MapToSensorEntity(sensorId);

		for (int i = 0; i < data_count; i++, currSensor->curr_data_index++) {
			//ENDIAN_CAST_QWORD(groupPointer,
			//	reinterpret_cast<FLOAT *>(&currSensor->data_accuracy[currSensor->currDataIndex++]));
			ENDIAN_CAST_QWORD(groupPointer, reinterpret_cast<UINT64 *>(&currSensor->data_timestamp[currSensor->curr_data_index]));
			groupPointer += 8;// time

			ENDIAN_CAST_DWORD(groupPointer,
					reinterpret_cast<FLOAT *>(&currSensor->data_accuracy[currSensor->curr_data_index]));
			groupPointer += 4;// accuracy

			for (int j = 0; j < currSensor->data_dimension; j++) {
				ENDIAN_CAST_DWORD(groupPointer,
						reinterpret_cast<FLOAT *>(&currSensor->data[j][currSensor->curr_data_index]));
				groupPointer += 4;
			}
		}
		readGroup++;
	}
	return (groupPointer - base);
}

DataReader::~DataReader() {
	::CloseHandle(m_file_handle);
	ptr_free(m_sensors)
	ptr_free(m_file_data)
}

#define LOAD_ASSERT_MSG(condition, msg) \
if (!(condition)) {                     \
	OutputDebugString(_T(msg));           \
	return -1;                            \
}  //	return GetLastError();               \\ 

#define LOAD_ASSERT(condition)          \
if (!(condition)) {                     \
	return -1;                            \
}

int DataReader::_LoadFile() {

	m_file_handle = ::CreateFile(m_file_path,    // 名称
		GENERIC_READ,                          // 读文件
		FILE_SHARE_READ | FILE_SHARE_WRITE,    // 共享读写
		NULL,                                  // 缺省安全属性。
		OPEN_EXISTING,                         //
		FILE_ATTRIBUTE_NORMAL,                 //      
		NULL);                                 // 模板文件为空

	LOAD_ASSERT_MSG(m_file_handle != INVALID_HANDLE_VALUE, "CreateFile failed!/r/n")

	//从文件里读取数据。
	LONG lDistance = 0;
	DWORD dwPtr = ::SetFilePointer(m_file_handle, lDistance, NULL, FILE_BEGIN);

	LOAD_ASSERT(dwPtr != INVALID_SET_FILE_POINTER)

	DWORD size = GetFileSize(m_file_handle, NULL);

	LOAD_ASSERT(size != 0)

	UCHAR* buffer = new UCHAR[size];

	DWORD dwReadSize = 0;
	BOOL bRet = ::ReadFile(m_file_handle, buffer, size, &dwReadSize, NULL);

	LOAD_ASSERT(bRet)
	LOAD_ASSERT(size == dwReadSize)

	m_file_size = dwReadSize;

	m_file_data = buffer;

	return 0;
}
