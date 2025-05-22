/**
Argus Monitor Data API, based on https://github.com/argotronic/argus_data_api/blob/master/argus_monitor_data_accessor.cpp
Modified to not use multiple threads and "push" updates to a function but rather be completely poll based to save on memory and cpu

Copyright (C) 2025 Zeanon
Original License from https://github.com/argotronic/argus_data_api still applies
**/

#include "argus_monitor_link.h"
#include "pch.h"


using namespace std;

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
		                                                                const char* sensor_group,
		                                                                const char* sensor_index,
		                                                                const char* data_index))
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

				if (IsHardwareEnabled("ArgusMonitor"))
				{
					process_sensor_data("Argus Monitor Version", (to_string(argus_monitor_data->ArgusMajor) + "." + to_string(argus_monitor_data->ArgusMinorA) + "." + to_string(argus_monitor_data->ArgusMinorB)).c_str(), "Text", "ArgusMonitor", "Argus Monitor", 0, 0);
					process_sensor_data("Argus Monitor Build", to_string(argus_monitor_data->ArgusBuild).c_str(), "Text", "ArgusMonitor", "Argus Monitor", 0, 0);
					process_sensor_data("Argus Data API Version", to_string(argus_monitor_data->Version).c_str(), "Text", "ArgusMonitor", "Argus Monitor", 0, 0);
					process_sensor_data("ArgusMonitorLink Version", "1.2.0.0", "Text", "ArgusMonitor", "Argus Monitor", 0, 0);
					process_sensor_data("Available Sensors", to_string(argus_monitor_data->TotalSensorCount).c_str(), "Text", "ArgusMonitor", "Argus Monitor", 0, 0);
				}

				for (size_t index{}; index < argus_monitor_data->TotalSensorCount; ++index)
				{
					const wstring label(argus_monitor_data->SensorData[index].Label);
					string name(label.begin(), label.end());
					const auto types = ParseTypes(argus_monitor_data->SensorData[index].SensorType, name);

					if (IsHardwareEnabled(types[0]))
					{
						//Sensor: <Name, Value, SensorType, HarwareType, Group>
						process_sensor_data("Text" == types[1] ? types[2] : name.c_str(),
						                    ("Text" == types[1] ? name : to_string(argus_monitor_data->SensorData[index].Value)).c_str(),
						                    types[1],
						                    types[0],
						                    types[2],
						                    to_string(argus_monitor_data->SensorData[index].SensorIndex).c_str(),
						                    to_string(argus_monitor_data->SensorData[index].DataIndex).c_str());
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

				map<uint32_t, double> fsb;
				map<uint32_t, vector<double>> cpu_temps;
				map<uint32_t, map<string, double>> multipliers;

				last_cycle_counter = argus_monitor_data->CycleCounter;

				for (size_t index{}; index < argus_monitor_data->TotalSensorCount; ++index)
				{
					const wstring label(argus_monitor_data->SensorData[index].Label);
					const string name(label.begin(), label.end());
					const vector<const char*> types = ParseTypes(argus_monitor_data->SensorData[index].SensorType, name);

					if (IsHardwareEnabled(types[0]) && "Text" != types[1])
					{
						const auto value = get_double_value(argus_monitor_data->SensorData[index].Value, types[1]);
						if ("CPU" == types[0])
						{
							const auto id = argus_monitor_data->SensorData[index].SensorIndex;
							if ("Temperature" == types[1] && "Temperature" == types[2])
							{
								cpu_temps[argus_monitor_data->SensorData[index].SensorIndex].push_back(value);
							}

							if ("Multiplier" == types[1] && "Multiplier" == types[2])
							{
								multipliers[argus_monitor_data->SensorData[index].SensorIndex]
								           [core_clock_id(types[0],
								                          name,
								                          argus_monitor_data->SensorData[index].SensorIndex,
								                          argus_monitor_data->SensorData[index].DataIndex)] = value;
							}

							if ("Frequency" == types[1] && "FSB" == types[2])
							{
								fsb[argus_monitor_data->SensorData[index].SensorIndex] = value;
							}
						}

						update(sensor_id(types[0],
						                 types[1],
						                 types[2],
						                 name,
						                 argus_monitor_data->SensorData[index].SensorIndex,
						                 argus_monitor_data->SensorData[index].DataIndex).c_str(),
						       to_string(value).c_str());
					}
				}

				if (!fsb.empty() && !multipliers.empty())
				{
					for (const auto &fsb_pair : fsb)
					{
						const auto id = to_string(fsb_pair.first);
						double max_multiplier = 0;
						double min_multiplier = 9999;
						double sum_multiplier = 0;
						for (const auto& multiplier : multipliers[fsb_pair.first])
						{
							max_multiplier = max(max_multiplier, multiplier.second);
							min_multiplier = min(min_multiplier, multiplier.second);
							sum_multiplier += multiplier.second;
							update(multiplier.first.c_str(), to_string(multiplier.second * fsb_pair.second).c_str());
						}
						const double average_multiplier = sum_multiplier / multipliers[fsb_pair.first].size();

						update(("CPU_Multiplier_Multiplier_Max_" + id).c_str(), to_string(max_multiplier).c_str());
						update(("CPU_Frequency_Core_Clock_Max_" + id).c_str(), to_string(max_multiplier * fsb_pair.second).c_str());
						update(("CPU_Multiplier_Multiplier_Average_" + id).c_str(), to_string(average_multiplier).c_str());
						update(("CPU_Frequency_Core_Clock_Average_" + id).c_str(), to_string(average_multiplier * fsb_pair.second).c_str());
						update(("CPU_Multiplier_Multiplier_Min_" + id).c_str(), to_string(min_multiplier).c_str());
						update(("CPU_Frequency_Core_Clock_Min_" + id).c_str(), to_string(min_multiplier * fsb_pair.second).c_str());
					}
				}

				if (!cpu_temps.empty())
				{
					for (const auto& cpu_temp : cpu_temps)
					{
						const auto values = min_max_average(cpu_temp.second);
						const auto id = to_string(cpu_temp.first);
						update(("CPU_Temperature_Temperature_Max_" + id).c_str(), to_string(values[1]).c_str());
						update(("CPU_Temperature_Temperature_Average_" + id).c_str(), to_string(values[2]).c_str());
						update(("CPU_Temperature_Temperature_Min_" + id).c_str(), to_string(values[0]).c_str());
					}
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
