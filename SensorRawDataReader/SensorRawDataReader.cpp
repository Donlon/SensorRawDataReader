// SensorRawDataReader.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include "pch.h"
#include "ReadingTest.h"

#include <iostream>

using namespace std;

void test() {
	int n=1000000;
	while (n-- > 0) {
		if (n % 1000 == 0) {
			cout << 10000 - n << endl;
		}
		read_raw_file(_T("D:\\Works\\Sensors\\rawdata\\sensor_data_0_1533823541213.data"));
	}
}
int main()
{
	//test();
	std::cout << "Hello World!\n";
	//read_raw_file(_T("D:\\Works\\Sensors\\rawdata\\sensor_data_21_20180831_083222.data"));
	read_raw_file(_T("D:\\Works\\Sensors\\rawdata\\sensor_data_0_1533823541213.data"));
}
