/**
Argus Monitor Data API, based on https://github.com/argotronic/argus_data_api/blob/master/argus_monitor_data_accessor.cpp
Modified to not use multiple threads and "push" updates to a function but rather be completely poll based to save on memory and cpu

Copyright (C) 2025 Zeanon
Original License from https://github.com/argotronic/argus_data_api still applies
**/

#include "pch.h"
#include "argus_monitor_data_api.h"
#include "Link.h"
#include <list>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include <algorithm>
#include <numeric>


using namespace std;

// parse ARGUS_MONITOR_SENSOR_TYPE and name to usable values
// return: <HardwareType, SensorType, Group>
static vector<const char*> ParseTypes(const argus_monitor::data_api::ARGUS_MONITOR_SENSOR_TYPE& sensor_type, const string& name)
{
	switch (sensor_type)
	{
	case argus_monitor::data_api::SENSOR_TYPE_CPU_TEMPERATURE:
		return { "CPU", "Temperature", "Temperature" };
	case argus_monitor::data_api::SENSOR_TYPE_CPU_TEMPERATURE_ADDITIONAL:
		return { "CPU", "Temperature", "Additional Temperature" };
	case argus_monitor::data_api::SENSOR_TYPE_CPU_MULTIPLIER:
		return { "CPU", "Multiplier", "Multiplier" };
	case argus_monitor::data_api::SENSOR_TYPE_CPU_FREQUENCY_FSB:
		return { "CPU", "Frequency", "FSB" };
	case argus_monitor::data_api::SENSOR_TYPE_CPU_LOAD:
		return { "CPU", "Percentage", "Load" };

	case argus_monitor::data_api::SENSOR_TYPE_GPU_TEMPERATURE:
		if (name.find("Memory") != string::npos) {
			return { "GPU", "Temperature", "Memory" };
		}
		return { "GPU", "Temperature", "GPU" };
	case argus_monitor::data_api::SENSOR_TYPE_GPU_FAN_SPEED_RPM:
		return { "GPU", "RPM", "Fan" };
	case argus_monitor::data_api::SENSOR_TYPE_GPU_FAN_SPEED_PERCENT:
		return { "GPU", "Percentage", "Fan" };
	case argus_monitor::data_api::SENSOR_TYPE_GPU_NAME:
		return { "GPU", "Text", "Name" };
	case argus_monitor::data_api::SENSOR_TYPE_GPU_LOAD:
		return { "GPU", "Load", "GPU" };
	case argus_monitor::data_api::SENSOR_TYPE_GPU_CORECLK:
		return { "GPU", "Frequency", "GPU" };
	case argus_monitor::data_api::SENSOR_TYPE_GPU_MEMORYCLK:
		return { "GPU", "Frequency", "Memory" };
	case argus_monitor::data_api::SENSOR_TYPE_GPU_SHARERCLK:
		return { "GPU", "Frequency", "Share" };
	case argus_monitor::data_api::SENSOR_TYPE_GPU_MEMORY_USED_PERCENT:
		return { "GPU", "Percentage", "Memory" };
	case argus_monitor::data_api::SENSOR_TYPE_GPU_MEMORY_USED_MB:
		return { "GPU", "Usage", "Memory" };
	case argus_monitor::data_api::SENSOR_TYPE_GPU_POWER:
		return { "GPU", "Power", "GPU" };

	case argus_monitor::data_api::SENSOR_TYPE_FAN_SPEED_RPM:
		return { "Mainboard", "RPM", "Fan" };
	case argus_monitor::data_api::SENSOR_TYPE_FAN_CONTROL_VALUE:
		return { "Mainboard", "Percentage", "Fan" };
	case argus_monitor::data_api::SENSOR_TYPE_TEMPERATURE:
		return { "Mainboard", "Temperature", "Sensors" };

	case argus_monitor::data_api::SENSOR_TYPE_RAM_USAGE:
		if (name.find("Total") != string::npos) {
			return { "RAM", "Total", "RAM" };
		}
		if (name.find("Used") != string::npos) {
			return { "RAM", "Usage", "RAM" };
		}
		return { "RAM", "Percentage", "RAM" };

	case argus_monitor::data_api::SENSOR_TYPE_DISK_TEMPERATURE:
		return { "Drive", "Temperature", "Drive" };
	case argus_monitor::data_api::SENSOR_TYPE_DISK_TRANSFER_RATE:
		return { "Drive", "Transfer", "Drive" };

	case argus_monitor::data_api::SENSOR_TYPE_NETWORK_SPEED:
		return { "Network", "Transfer", "Network" };

	case argus_monitor::data_api::SENSOR_TYPE_BATTERY:
		return { "Battery", "Percentage", "Battery" };

	case argus_monitor::data_api::SENSOR_TYPE_SYNTHETIC_TEMPERATURE:
		return { "ArgusMonitor", "Temperature", "Synthetic Temperature" };
	case argus_monitor::data_api::SENSOR_TYPE_MAX_SENSORS:
		return { "ArgusMonitor", "Numeric", "Sensors" };

	case argus_monitor::data_api::SENSOR_TYPE_INVALID:
	default:
		return { "Invalid", "Invalid", "Invalid" };
	}
}

static const double get_double_value(const double& value, const string& sensor_type)
{
	if (sensor_type == "Transfer") return value * 8;
	if (sensor_type == "Usage" || sensor_type == "Total" || sensor_type == "Frequency" || sensor_type == "Clock") return value * 1000000;
	return value;
}

static const string core_clock_id(const string &hardware_type, const string &sensor_name)
{
	return hardware_type + "_Frequency_Core_Clock_" + sensor_name;
}

static const string sensor_id(const string& hardware_type, const string& sensor_type, const string& sensor_group, const string& sensor_name)
{
	return hardware_type + "_" + sensor_type + "_" + sensor_group + "_" + sensor_name;
}

static const vector<double> min_max_average(const vector<double> values)
{
	double min_value = 99999;
	double max_value = -99999;
	double sum = 0;
	for (const auto value : values)
	{
		min_value = min(min_value, value);
		max_value = max(max_value, value);
		sum += value;
	}
	return { min_value, max_value, sum/values.size() };
}

namespace argus_monitor {
	namespace data_api {
		// Open the connection to Argus Monitor
		// return:
		//  0: connection is open
		//  1: could not open file mapping
		// 10: could not optain fileview
		int ArgusMonitorLink::Open()
		{
			if (is_open_) {
				return 0;
			}

			int success{ 0 };
			handle_file_mapping = OpenFileMappingW(FILE_MAP_READ | FILE_MAP_WRITE,             // read/write access
				                                   FALSE,                                      // do not inherit the name
				                                   argus_monitor::data_api::kMappingName());   // name of mapping object

			if (nullptr == handle_file_mapping) {
				return 1;
			}

			pointer_to_mapped_data = MapViewOfFile(handle_file_mapping,               // handle to map object
                                                   FILE_MAP_READ | FILE_MAP_WRITE,    // read/write permission
                                                   0, 0, argus_monitor::data_api::kMappingSize());

			if (nullptr == pointer_to_mapped_data) {
				CloseHandle(handle_file_mapping);
				return 10;
			}

			argus_monitor_data = reinterpret_cast<argus_monitor::data_api::ArgusMonitorData const*>(pointer_to_mapped_data);
			last_cycle_counter = 0;

			is_open_ = true;

			return 0;
		}

		// Clean up the file handle and unmap the fileview
		// return:
		//  0: successfully unmaped the fileview and closed the handle
		//  1: Could not unmap the fileview
		// 10: Could not close the handle
		// 11: Neither was possible
		int ArgusMonitorLink::Close()
		{
			is_open_ = false;

			int success{ 0 };
			argus_monitor_data = nullptr;
			if (pointer_to_mapped_data) {
				success += UnmapViewOfFile(pointer_to_mapped_data) == 0 ? 1 : 0;
				pointer_to_mapped_data = nullptr;
			}
			if (handle_file_mapping) {
				success += CloseHandle(handle_file_mapping) == 0 ? 10 : 0;
				handle_file_mapping = nullptr;
			}
			return success;
		}

		// Check whether ArgusMonitor is active
		bool ArgusMonitorLink::CheckArgusSignature() const
		{
			if (nullptr == pointer_to_mapped_data || nullptr == argus_monitor_data || nullptr == ArgusMonitorLink::OpenArgusApiMutex()) {
				return false;
			}
			return argus_monitor_data->Signature == 0x4D677241;
		}

		// Get the total amount of sensors provided by Argus Monitor
		int ArgusMonitorLink::GetTotalSensorCount() const
		{
			if (nullptr == pointer_to_mapped_data || nullptr == argus_monitor_data || nullptr == ArgusMonitorLink::OpenArgusApiMutex()) {
				return 0;
			}
			return argus_monitor_data->TotalSensorCount;
		}

		// Get the data from argus monitor and if its new, create arrays that hold the sensor data and then use the passed add method to
		// add it to an external collection
		// returns true if new data was available and false if no new data was available
		bool ArgusMonitorLink::GetSensorData(void (process_sensor_data)(const char* sensor_name,
		                                                                const char* sensor_value,
		                                                                const char* sensor_type,
		                                                                const char* hardware_type,
		                                                                const char* sensor_group))
		{
			if (nullptr == pointer_to_mapped_data || nullptr == argus_monitor_data) {
				return false;
			}

			HANDLE mutex_handle = ArgusMonitorLink::OpenArgusApiMutex();
			if (nullptr == mutex_handle) {
				return false;
			}


			{
				Lock scoped_lock(mutex_handle);
				// Check if new data is available
				if (last_cycle_counter == argus_monitor_data->CycleCounter)
				{
					return false;
				}
				last_cycle_counter = argus_monitor_data->CycleCounter;
				for (size_t index{}; index < argus_monitor_data->TotalSensorCount; ++index)
				{
					const wstring label(argus_monitor_data->SensorData[index].Label);
					const string name(label.begin(), label.end());
					const auto types = ParseTypes(argus_monitor_data->SensorData[index].SensorType, name);

					if (IsHardwareEnabled(types[0]))
					{
						//Sensor: <Name, Value, SensorType, HarwareType, Group>
						const auto value = types[1] != "Text" ? to_string(argus_monitor_data->SensorData[index].Value) : name;
						process_sensor_data(name.c_str(), value.c_str(), types[1], types[0], types[2]);
					}
				}
				return true;
			}
			return false;
		}

		bool ArgusMonitorLink::UpdateSensorData(void (update)(const char* id, const char* value))
		{

			if (nullptr == pointer_to_mapped_data || nullptr == argus_monitor_data) {
				return false;
			}

			HANDLE mutex_handle = ArgusMonitorLink::OpenArgusApiMutex();
			if (nullptr == mutex_handle) {
				return false;
			}


			{
				Lock scoped_lock(mutex_handle);
				// Check if new data is available
				if (last_cycle_counter == argus_monitor_data->CycleCounter)
				{
					return false;
				}

				double fsb = 0;
				vector<double> cpu_temps;
				map<string, double> multipliers;

				last_cycle_counter = argus_monitor_data->CycleCounter;
				for (size_t index{}; index < argus_monitor_data->TotalSensorCount; ++index)
				{
					const wstring label(argus_monitor_data->SensorData[index].Label);
					const string name(label.begin(), label.end());
					const vector<const char*> types = ParseTypes(argus_monitor_data->SensorData[index].SensorType, name);

					if (IsHardwareEnabled(types[0]))
					{
						const auto value = get_double_value(argus_monitor_data->SensorData[index].Value, types[1]);
						if ("CPU" == types[0])
						{
							if ("Temperature" == types[1] && "Temperature" == types[2])
							{
								cpu_temps.push_back(value);
							}

							if ("Multiplier" == types[1] && "Multiplier" == types[2])
							{
								multipliers[core_clock_id(types[0], name)] = value;
							}

							if ("Frequency" == types[1] && "FSB" == types[2])
							{
								fsb = value;
							}
						}

						//Sensor: <Name, Value, SensorType, HarwareType, Group>
						update(sensor_id(types[0], types[1], types[2], name).c_str(), types[1] == "Text" ? name.c_str() : to_string(value).c_str());
					}
				}

				if (0 != fsb && multipliers.size() > 0)
				{
					double max_multiplier = 0;
					double min_multiplier = 9999;
					double sum_multiplier = 0;
					for (auto& multiplier : multipliers)
					{
						max_multiplier = max(max_multiplier, multiplier.second);
						min_multiplier = min(min_multiplier, multiplier.second);
						sum_multiplier += multiplier.second;
						update(multiplier.first.c_str(), to_string(multiplier.second * fsb).c_str());
					}
					const double average_multiplier = sum_multiplier / multipliers.size();

					update("CPU_Multiplier_Multiplier_Max", to_string(max_multiplier).c_str());
					update("CPU_Frequency_Core_Clock_Max", to_string(max_multiplier * fsb).c_str());
					update("CPU_Multiplier_Multiplier_Average", to_string(average_multiplier).c_str());
					update("CPU_Frequency_Core_Clock_Average", to_string(average_multiplier * fsb).c_str());
					update("CPU_Multiplier_Multiplier_Min", to_string(min_multiplier).c_str());
					update("CPU_Frequency_Core_Clock_Min", to_string(min_multiplier * fsb).c_str());
				}

				if (!cpu_temps.empty())
				{
					const auto values = min_max_average(cpu_temps);

					update("CPU_Temperature_Temperature_Max", to_string(values[1]).c_str());
					update("CPU_Temperature_Temperature_Average", to_string(values[2]).c_str());
					update("CPU_Temperature_Temperature_Min", to_string(values[0]).c_str());
				}
				return true;
			}
			return false;
		}

		// Set the given hardware type to enabled/disabled
		void ArgusMonitorLink::SetHardwareEnabled(const string &type, const bool &enabled)
		{
			enabled_hardware[type] = enabled;
		}

		// Check whether the given hardware type is enabled
		bool ArgusMonitorLink::IsHardwareEnabled(const string &type) const
		{
			try
			{
				return enabled_hardware.at(type);
			}
			catch (const out_of_range& e)
			{
				return false;
			}
		}

		HANDLE ArgusMonitorLink::OpenArgusApiMutex()
		{
			return OpenMutexW(READ_CONTROL | MUTANT_QUERY_STATE | SYNCHRONIZE, FALSE, argus_monitor::data_api::kMutexName());
		}
	}
}
