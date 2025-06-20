/**
Argus Monitor Data API, loosely based on https://github.com/argotronic/argus_data_api/blob/master/argus_monitor_data_accessor.cpp
Modified to not use multiple threads and "push" updates to a function but rather be completely poll based to save on memory and cpu

Copyright (C) 2025 Zeanon
Original License from https://github.com/argotronic/argus_data_api still applies.
**/

#include "argus_monitor_link.h"

namespace argus_monitor
{
    namespace data_api
    {
        int ArgusMonitorLink::Open()
        {
            if (is_open)
            {
                return 0;
            }

            file_mapping_handle = OpenFileMappingW(FILE_MAP_READ | FILE_MAP_WRITE,             // read/write access
                                                   FALSE,                                      // do not inherit the name
                                                   kMappingName());                            // name of mapping object

            if (nullptr == file_mapping_handle)
            {
                return 1;
            }

            argus_monitor_data = reinterpret_cast<ArgusMonitorData const*>(
                MapViewOfFile(file_mapping_handle,               // handle to map object
                              FILE_MAP_READ | FILE_MAP_WRITE,    // read/write permission
                              0,
                              0,
                              kMappingSize())
                );

            if (nullptr == argus_monitor_data)
            {
                CloseHandle(file_mapping_handle);
                return 10;
            }

            mutex_handle = OpenArgusApiMutex();

            if (nullptr == mutex_handle)
            {
                return 100;
            }

            last_cycle_counter = 0;

            is_open = true;

            return 0;
        }

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
                const char* hardware_type;
                const char* sensor_type;
                const char* sensor_group;
                ParseTypes(sensor_data.SensorType, name, hardware_type, sensor_type, sensor_group);

                if (IsHardwareEnabled(hardware_type))
                {
                    const auto& value = GetFloatValue(sensor_data.Value, sensor_type);
                    //Sensor: <Name, Value, SensorType, HarwareType, Group>
                    process_sensor_data("Text" == sensor_type ? sensor_group : name.c_str(),
                                        ("Text" == sensor_type ? name : to_string(value)).c_str(),
                                        sensor_type,
                                        hardware_type,
                                        sensor_group,
                                        to_string(sensor_data.SensorIndex).c_str(),
                                        to_string(sensor_data.DataIndex).c_str());
                }
            }
        }

        bool ArgusMonitorLink::UpdateSensorData(void (update)(const char* sensor_id, const float sensor_value))
        {
            Lock scoped_lock(mutex_handle);

            // Check if new data is available
            if (last_cycle_counter == argus_monitor_data->CycleCounter) return false;

            map<uint32_t, float> fsb_clocks;
            map<uint32_t, vector<float>> cpu_temps;
            map<uint32_t, map<string, float>> multipliers;

            last_cycle_counter = argus_monitor_data->CycleCounter;

            for (size_t index{}; index < argus_monitor_data->TotalSensorCount; ++index)
            {
                const auto& sensor_data = argus_monitor_data->SensorData[index];
                const wstring label(sensor_data.Label);
                const string name(label.begin(), label.end());
                const char* hardware_type;
                const char* sensor_type;
                const char* sensor_group;
                ParseTypes(sensor_data.SensorType, name, hardware_type, sensor_type, sensor_group);

                if (IsHardwareEnabled(hardware_type) && "Text" != sensor_type)
                {
                    const auto& value = GetFloatValue(sensor_data.Value, sensor_type);
                    if (value >= 0 && ("Temperature" != sensor_type || value > 0))
                    {
                        const auto& sensor_index = sensor_data.SensorIndex;
                        const auto& data_index = sensor_data.DataIndex;
                        if ("CPU" == hardware_type)
                        {
                            if ("Temperature" == sensor_type && "Temperature" == sensor_group)
                            {
                                cpu_temps[sensor_index].push_back(value);
                            }

                            if ("Multiplier" == sensor_type && "Multiplier" == sensor_group)
                            {
                                multipliers[sensor_index]
                                           [SensorId(hardware_type,
                                                     "Frequency",
                                                     "Core_Clock",
                                                     sensor_index,
                                                     data_index)]
                                    = value;
                            }

                            if ("Frequency" == sensor_type && "FSB" == sensor_group)
                            {
                                fsb_clocks[sensor_index] = value;
                            }
                        }

                        update(SensorId(hardware_type,
                                        sensor_type,
                                        sensor_group,
                                        sensor_index,
                                        data_index).c_str(),
                               value);
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

                            update(value.first.c_str(), value.second * fsb_clock.second);
                        }
                        const float& average_multiplier = sum_multiplier / multiplier_size;

                        const auto& id = to_string(fsb_clock.first);
                        update(("CPU_Multiplier_Multiplier_Max_" + id).c_str(), max_multiplier);
                        update(("CPU_Frequency_Core_Clock_Max_" + id).c_str(), max_multiplier * fsb_clock.second);
                        update(("CPU_Multiplier_Multiplier_Average_" + id).c_str(), average_multiplier);
                        update(("CPU_Frequency_Core_Clock_Average_" + id).c_str(), average_multiplier * fsb_clock.second);
                        update(("CPU_Multiplier_Multiplier_Min_" + id).c_str(), min_multiplier);
                        update(("CPU_Frequency_Core_Clock_Min_" + id).c_str(), min_multiplier * fsb_clock.second);
                    }
                }
            }

            if (cpu_temps.size() > 0)
            {
                for (const auto& cpu_temp : cpu_temps)
                {
                    if (!cpu_temp.second.empty())
                    {
                        float min_temp{ FLT_MAX };
                        float max_temp{ -FLT_MAX };
                        float sum_temp{ 0 };
                        for (const auto& temp : cpu_temp.second)
                        {
                            if (temp < min_temp) min_temp = temp;
                            if (temp > max_temp) max_temp = temp;
                            sum_temp += temp;
                        }

                        const auto& id = to_string(cpu_temp.first);
                        update(("CPU_Temperature_Temperature_Max_" + id).c_str(), max_temp);
                        update(("CPU_Temperature_Temperature_Average_" + id).c_str(), sum_temp / cpu_temp.second.size());
                        update(("CPU_Temperature_Temperature_Min_" + id).c_str(), min_temp);
                    }
                }
            }
            return true;
        }
    }
}
