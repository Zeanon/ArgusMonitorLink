#pragma once
#include "pch.h"
#include "argus_monitor_data_api.h"
#include <vector>
#include <string>
#include <algorithm>
#include <numeric>

using namespace std;

vector<const char*> ParseTypes(const argus_monitor::data_api::ARGUS_MONITOR_SENSOR_TYPE& sensor_type, const string& name);

const double get_double_value(const double& value, const string& sensor_type);

const string core_clock_id(const string& hardware_type, const string& sensor_name);

const string sensor_id(const string& hardware_type, const string& sensor_type, const string& sensor_group, const string& sensor_name);

const vector<double> min_max_average(const vector<double>& values);