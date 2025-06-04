/**
Argus Monitor Data API Utility class.

Copyright (C) 2025 Zeanon
Original License from https://github.com/argotronic/argus_data_api still applies.
**/

#include "utility.h"

using namespace std;

// parse ARGUS_MONITOR_SENSOR_TYPE and name to usable values
// return: <HardwareType, SensorType, Group>
vector<const char*> ParseTypes(const argus_monitor::data_api::ARGUS_MONITOR_SENSOR_TYPE& sensor_type, const string& name)
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

        case argus_monitor::data_api::SENSOR_TYPE_GPU_NAME:
            return { "GPU", "Text", "Name" };
        case argus_monitor::data_api::SENSOR_TYPE_GPU_TEMPERATURE:
            if (name.contains("Memory")) return { "GPU", "Temperature", "Memory" };
            return { "GPU", "Temperature", "GPU" };
        case argus_monitor::data_api::SENSOR_TYPE_GPU_FAN_SPEED_PERCENT:
            return { "GPU", "Percentage", "Fan" };
        case argus_monitor::data_api::SENSOR_TYPE_GPU_FAN_SPEED_RPM:
            return { "GPU", "RPM", "Fan" };
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
        case argus_monitor::data_api::SENSOR_TYPE_GPU_LOAD:
            return { "GPU", "Load", "GPU" };
        case argus_monitor::data_api::SENSOR_TYPE_GPU_POWER:
            return { "GPU", "Power", "GPU" };

        case argus_monitor::data_api::SENSOR_TYPE_FAN_CONTROL_VALUE:
            return { "Fan", "Percentage", "Power" };
        case argus_monitor::data_api::SENSOR_TYPE_FAN_SPEED_RPM:
            return { "Fan", "RPM", "RPM" };

        case argus_monitor::data_api::SENSOR_TYPE_RAM_USAGE:
            if (name.contains("Total")) return { "RAM", "Total", "RAM" };
            if (name.contains("Used"))  return { "RAM", "Usage", "RAM" };
            return { "RAM", "Percentage", "RAM" };

        case argus_monitor::data_api::SENSOR_TYPE_DISK_TEMPERATURE:
            return { "Drive", "Temperature", "Drive" };
        case argus_monitor::data_api::SENSOR_TYPE_DISK_TRANSFER_RATE:
            return { "Drive", "Transfer", "Drive" };

        case argus_monitor::data_api::SENSOR_TYPE_NETWORK_SPEED:
            return { "Network", "Transfer", "Network" };

        case argus_monitor::data_api::SENSOR_TYPE_BATTERY:
            return { "Battery", "Percentage", "Battery" };

        case argus_monitor::data_api::SENSOR_TYPE_TEMPERATURE:
            return { "Temperature", "Temperature", "Temperature Sensor" };
        case argus_monitor::data_api::SENSOR_TYPE_SYNTHETIC_TEMPERATURE:
            return { "Temperature", "Temperature", "Synthetic Temperature" };

        case argus_monitor::data_api::SENSOR_TYPE_MAX_SENSORS:
            return { "ArgusMonitor", "Numeric", "Sensor" };

        case argus_monitor::data_api::SENSOR_TYPE_INVALID:
        default:
            return { "Invalid", "Invalid", "Invalid" };
    }
}

const float GetFloatValue(const float& value, const string& sensor_type)
{
    if (sensor_type == "Transfer") return (value * 1000000) / 131072; // (1000/1024) * (1000/1024) * 8
    if (sensor_type == "Frequency" || sensor_type == "Clock") return value * 1000000;
    if (sensor_type == "Usage" || sensor_type == "Total") return (value * 1000000000) / 1024; // (1000/1024) * 1_000_000
    return value;
}

const string CoreClockId(const string& hardware_type, const int& sensor_index, const int& data_index)
{
    return hardware_type + "_Frequency_Core_Clock_" + to_string(sensor_index) + "_" + to_string(data_index);
}

const string SensorId(const string& hardware_type, const string& sensor_type, const string& sensor_group, const int& sensor_index, const int& data_index)
{
    return hardware_type + "_" + sensor_type + "_" + sensor_group + "_" + to_string(sensor_index) + "_" + to_string(data_index);
}

const vector<float> MinMaxAverage(const vector<float>& values)
{
    float min_value = FLT_MAX;
    float max_value = -FLT_MAX;
    float sum = 0;
    for (const auto value : values)
    {
        if (value < min_value) min_value = value;
        if (value > max_value) max_value = value;
        sum += value;
    }
    return { min_value, max_value, sum / values.size() };
}