
#include "pch.h"
#include "ReadingTest.h"
#include "MatlabDataExporter.h"

void read_raw_file(LPCTSTR file) {
	DataReader reader(file);
	reader.Load();
	if (reader.m_status != DataReader::LOADED) {
		_tprintf(_T("Load %s failed, reason=%d\n"), file, 0);
		return;
	} else {
		//_tprintf(_T("Load %s Successfully.\n"), file);
	}

	reader.Parse();

	if (reader.m_status != DataReader::PARSED) {
		_tprintf(_T("Parse %s failed, reason=%d\n"), file, 0);
		return;
	} else {
		_tprintf(_T("Parse %s Successfully.\n"), file);
		MatlabDataExporter::Export(reader);
	}
}