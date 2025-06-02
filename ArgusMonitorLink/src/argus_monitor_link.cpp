/**
Argus Monitor Data API, loosely based on https://github.com/argotronic/argus_data_api/blob/master/argus_monitor_data_accessor.cpp
Modified to not use multiple threads and "push" updates to a function but rather be completely poll based to save on memory and cpu

Copyright (C) 2025 Zeanon
Original License from https://github.com/argotronic/argus_data_api still applies.
**/

#include "argus_monitor_link.h"


using namespace std;

namespace argus_monitor
{
    namespace data_api
    {
        // Open the connection to Argus Monitor
        // return:
        //   0: connection is open
        //   1: could not open file mapping
        //  10: could not optain fileview
        // 100: could not open ArgusApiMutex
        int ArgusMonitorLink::Open()
        {
            if (is_open)
            {
                return 0;
            }

            file_mapping_handle = OpenFileMappingW(FILE_MAP_READ | FILE_MAP_WRITE,             // read/write access
                                                   FALSE,                                      // do not inherit the name
                                                   argus_monitor::data_api::kMappingName());   // name of mapping object

            if (nullptr == file_mapping_handle)
            {
                return 1;
            }

            argus_monitor_data = reinterpret_cast<argus_monitor::data_api::ArgusMonitorData const*>(
                MapViewOfFile(file_mapping_handle,               // handle to map object
                              FILE_MAP_READ | FILE_MAP_WRITE,    // read/write permission
                              0, 0, argus_monitor::data_api::kMappingSize())
                );

            if (nullptr == argus_monitor_data)
            {
                CloseHandle(file_mapping_handle);
                return 10;
            }

            mutex_handle = ArgusMonitorLink::OpenArgusApiMutex();

            if (nullptr == mutex_handle)
            {
                return 100;
            }

            last_cycle_counter = 0;

            is_open = true;

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
            is_open = false;

            int success{ 0 };
            if (argus_monitor_data)
            {
                success += UnmapViewOfFile(argus_monitor_data) == 0 ? 1 : 0;
                argus_monitor_data = nullptr;
            }

            if (file_mapping_handle)
            {
                success += CloseHandle(file_mapping_handle) == 0 ? 10 : 0;
                file_mapping_handle = nullptr;
            }

            if (mutex_handle)
            {
                mutex_handle = nullptr;
            }
            return success;
        }

        // Get the data from argus monitor and if its new, call the passed delegate method
        // returns true if new data was available and false if no new data was available
        void ArgusMonitorLink::GetSensorData(void (process_sensor_data)(const char* sensor_name,
                                                                        const char* sensor_value,
                                                                        const char* sensor_type,
                                                                        const char* hardware_type,
                                                                        const char* sensor_group,
                                                                        const char* sensor_index,
                                                                        const char* data_index))
        {
            Lock scoped_lock(mutex_handle);

            if (IsHardwareEnabled("ArgusMonitor"))
            {
                process_sensor_data("Argus Monitor Version", (to_string(argus_monitor_data->ArgusMajor) + "." + to_string(argus_monitor_data->ArgusMinorA) + "." + to_string(argus_monitor_data->ArgusMinorB)).c_str(), "Text", "ArgusMonitor", "Argus Monitor", "0", "0");
                process_sensor_data("Argus Monitor Build", to_string(argus_monitor_data->ArgusBuild).c_str(), "Text", "ArgusMonitor", "Argus Monitor", "0", "1");
                process_sensor_data("Argus Data API Version", to_string(argus_monitor_data->Version).c_str(), "Text", "ArgusMonitor", "Argus Monitor", "0", "2");
                process_sensor_data("ArgusMonitorLink Version", VER_FILE_VERSION_STR, "Text", "ArgusMonitor", "Argus Monitor", "0", "3");
                process_sensor_data("Available Sensors", to_string(argus_monitor_data->TotalSensorCount).c_str(), "Text", "ArgusMonitor", "Argus Monitor", "0", "4");
            }

            for (size_t index{}; index < argus_monitor_data->TotalSensorCount; ++index)
            {
                const auto& sensor_data = argus_monitor_data->SensorData[index];
                const wstring label(sensor_data.Label);
                const string name(label.begin(), label.end());
                const auto& types = ParseTypes(sensor_data.SensorType, name);

                if (IsHardwareEnabled(types[0]))
                {
                    //Sensor: <Name, Value, SensorType, HarwareType, Group>
                    process_sensor_data("Text" == types[1] ? types[2] : name.c_str(),
                                        ("Text" == types[1] ? name : to_string(sensor_data.Value)).c_str(),
                                        types[1],
                                        types[0],
                                        types[2],
                                        to_string(sensor_data.SensorIndex).c_str(),
                                        to_string(sensor_data.DataIndex).c_str());
                }
            }
        }

        bool ArgusMonitorLink::UpdateSensorData(void (update)(const char* id, const char* value))
        {
            Lock scoped_lock(mutex_handle);

            // Check if new data is available
            if (last_cycle_counter == argus_monitor_data->CycleCounter) return false;

            if (IsHardwareEnabled("ArgusMonitor"))
            {
                update(sensor_id("ArgusMonitor", "Text", "Argus Monitor", 0, 0).c_str(),
                       (to_string(argus_monitor_data->ArgusMajor)
                        + "."
                        + to_string(argus_monitor_data->ArgusMinorA)
                        + "."
                        + to_string(argus_monitor_data->ArgusMinorB)).c_str());
                update(sensor_id("ArgusMonitor", "Text", "Argus Monitor", 0, 1).c_str(),
                       to_string(argus_monitor_data->ArgusBuild).c_str());
                update(sensor_id("ArgusMonitor", "Text", "Argus Monitor", 0, 2).c_str(),
                       to_string(argus_monitor_data->Version).c_str());
                update(sensor_id("ArgusMonitor", "Text", "Argus Monitor", 0, 3).c_str(),
                       VER_FILE_VERSION_STR);
                update(sensor_id("ArgusMonitor", "Text", "Argus Monitor", 0, 4).c_str(),
                       to_string(argus_monitor_data->TotalSensorCount).c_str());
            }

            map<uint32_t, float> fsb_clocks;
            map<uint32_t, vector<float>> cpu_temps;
            map<uint32_t, map<string, float>> multipliers;

            last_cycle_counter = argus_monitor_data->CycleCounter;

            for (size_t index{}; index < argus_monitor_data->TotalSensorCount; ++index)
            {
                const auto& sensor_data = argus_monitor_data->SensorData[index];
                const wstring label(sensor_data.Label);
                const string name(label.begin(), label.end());
                const auto& types = ParseTypes(sensor_data.SensorType, name);

                if (IsHardwareEnabled(types[0]) && "Text" != types[1])
                {
                    const auto& value = get_float_value(sensor_data.Value, types[1]);
                    const auto& sensor_index = sensor_data.SensorIndex;
                    const auto& data_index = sensor_data.DataIndex;
                    if ("CPU" == types[0])
                    {
                        if ("Temperature" == types[1] && "Temperature" == types[2])
                        {
                            cpu_temps[sensor_index].push_back(value);
                        }

                        if ("Multiplier" == types[1] && "Multiplier" == types[2])
                        {
                            multipliers[sensor_index]
                                [core_clock_id(types[0],
                                               sensor_index,
                                               data_index)] = value;
                        }

                        if ("Frequency" == types[1] && "FSB" == types[2])
                        {
                            fsb_clocks[sensor_index] = value;
                        }
                    }

                    if ("Temperature" != types[1] || value > 0.0)
                    {
                        update(sensor_id(types[0],
                                         types[1],
                                         types[2],
                                         sensor_index,
                                         data_index).c_str(),
                               to_string(value).c_str());
                    }
                }
            }

            if (fsb_clocks.size() > 0 && multipliers.size() > 0)
            {
                for (const auto& fsb_clock : fsb_clocks)
                {
                    const auto& multiplier = multipliers[fsb_clock.first];
                    const auto& multiplier_size = multiplier.size();
                    if (multiplier_size > 0)
                    {
                        float min_multiplier = FLT_MAX;
                        float max_multiplier = -FLT_MAX;
                        float sum_multiplier = 0;
                        for (const auto& value : multiplier)
                        {
                            if (value.second < min_multiplier) min_multiplier = value.second;
                            if (value.second > max_multiplier) max_multiplier = value.second;
                            sum_multiplier += value.second;

                            update(value.first.c_str(), to_string(value.second * fsb_clock.second).c_str());
                        }
                        const float& average_multiplier = sum_multiplier / multiplier_size;

                        const auto& id = to_string(fsb_clock.first);
                        update(("CPU_Multiplier_Multiplier_Max_" + id).c_str(), to_string(max_multiplier).c_str());
                        update(("CPU_Frequency_Core_Clock_Max_" + id).c_str(), to_string(max_multiplier * fsb_clock.second).c_str());
                        update(("CPU_Multiplier_Multiplier_Average_" + id).c_str(), to_string(average_multiplier).c_str());
                        update(("CPU_Frequency_Core_Clock_Average_" + id).c_str(), to_string(average_multiplier * fsb_clock.second).c_str());
                        update(("CPU_Multiplier_Multiplier_Min_" + id).c_str(), to_string(min_multiplier).c_str());
                        update(("CPU_Frequency_Core_Clock_Min_" + id).c_str(), to_string(min_multiplier * fsb_clock.second).c_str());
                    }
                }
            }

            if (cpu_temps.size() > 0)
            {
                for (const auto& cpu_temp : cpu_temps)
                {
                    if (!cpu_temp.second.empty())
                    {
                        float min_temp = FLT_MAX;
                        float max_temp = -FLT_MAX;
                        float sum_temp = 0;
                        for (const auto& temp : cpu_temp.second)
                        {
                            if (temp < min_temp) min_temp = temp;
                            if (temp > max_temp) max_temp = temp;
                            sum_temp += temp;
                        }

                        const auto& id = to_string(cpu_temp.first);
                        update(("CPU_Temperature_Temperature_Max_" + id).c_str(), to_string(max_temp).c_str());
                        update(("CPU_Temperature_Temperature_Average_" + id).c_str(), to_string(sum_temp / cpu_temp.second.size()).c_str());
                        update(("CPU_Temperature_Temperature_Min_" + id).c_str(), to_string(min_temp).c_str());
                    }
                }
            }
            return true;
        }

        // Set the given hardware type to enabled/disabled
        void ArgusMonitorLink::SetHardwareEnabled(const string& type, const bool& enabled)
        {
            enabled_hardware[type] = enabled;
        }

        // Check whether the given hardware type is enabled
        bool ArgusMonitorLink::IsHardwareEnabled(const string& type) const
        {
            try
            {
                return enabled_hardware.at(type);
            }
            catch (const out_of_range& _)
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
