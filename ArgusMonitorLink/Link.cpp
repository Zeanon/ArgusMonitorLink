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


using namespace std;

// parse ARGUS_MONITOR_SENSOR_TYPE and name to usable values
// return: <HardwareType, SensorType, Group>
static vector<const char*> parse_types(argus_monitor::data_api::ARGUS_MONITOR_SENSOR_TYPE sensor_type, string name)
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
		return { "ArgusMonitor", "Temperature", "Synthetic Temperatures" };
	case argus_monitor::data_api::SENSOR_TYPE_MAX_SENSORS:
		return { "ArgusMonitor", "Numeric", "Sensors" };

	case argus_monitor::data_api::SENSOR_TYPE_INVALID:
	default:
		return { "Invalid", "Invalid", "Invalid" };
	}
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
		bool ArgusMonitorLink::GetSensorData(void (add)(const char* sensor[]))
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
					const auto types = parse_types(argus_monitor_data->SensorData[index].SensorType, name);

					if (enabled_sensors.at(types[0]))
					{
						//Sensor: <Name, Value, SensorType, HarwareType, Group>
						const auto value = types[1] != "Text" ? to_string(argus_monitor_data->SensorData[index].Value) : name;
						const char* sensor[] = {
							name.c_str(),
							value.c_str(),
							types[1],
							types[0],
							types[2]
						};
						add(sensor);
					}
				}
				return true;
			}
			return false;
		}

		// Set the given hardware type to enabled/disabled
		void ArgusMonitorLink::set_hardware_enabled(const char* type, const bool enabled)
		{
			enabled_sensors[type] = enabled;
		}

		// Check whether the given hardware type is enabled
		bool ArgusMonitorLink::get_hardware_enabled(const char* type) const
		{
			try
			{
				return enabled_sensors.at(type);
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