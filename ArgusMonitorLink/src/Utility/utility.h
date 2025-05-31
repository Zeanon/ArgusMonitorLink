/**
Argus Monitor Data API Utility class.

Copyright (C) 2025 Zeanon
Original License from https://github.com/argotronic/argus_data_api still applies.
**/

#pragma once
#include "../ArgusMonitor/argus_monitor_data_api.h"
#include <algorithm>
#include <numeric>
#include <regex>
#include <string>
#include <vector>

using namespace std;

vector<const char*> ParseTypes(const argus_monitor::data_api::ARGUS_MONITOR_SENSOR_TYPE& sensor_type, const string& name);

const float get_float_value(const float& value, const string& sensor_type);

const string core_clock_id(const string& hardware_type, const int& data_index, const int& sensor_index);

const string sensor_id(const string& hardware_type, const string& sensor_type, const string& sensor_group, const int& data_index, const int& sensor_index);

const vector<float> min_max_average(const vector<float>& values);