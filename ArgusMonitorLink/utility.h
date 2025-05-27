#pragma once
#include "argus_monitor_data_api.h"
#include "pch.h"
#include <algorithm>
#include <numeric>
#include <string>
#include <vector>

using namespace std;

vector<const char*> ParseTypes(const argus_monitor::data_api::ARGUS_MONITOR_SENSOR_TYPE& sensor_type, const string& name);

const float get_float_value(const float& value, const string& sensor_type);

const string core_clock_id(const string& hardware_type, const string& sensor_name, const int& data_index, const int& sensor_index);

const string sensor_id(const string& hardware_type, const string& sensor_type, const string& sensor_group, const string& sensor_name, const int& data_index, const int& sensor_index);

const vector<float> min_max_average(const vector<float>& values);