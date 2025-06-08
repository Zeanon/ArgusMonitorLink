/**
Argus Monitor Data API Utility class.

Copyright (C) 2025 Zeanon
Original License from https://github.com/argotronic/argus_data_api still applies.
**/

#include "utility.h"

// parse ARGUS_MONITOR_SENSOR_TYPE and name to usable values
inline void ParseTypes(const ARGUS_MONITOR_SENSOR_TYPE& sensor_type,
                       const string& name,
                       const char*& hardware_type_buf,
                       const char*& sensor_type_buf,
                       const char*& sensor_group_buf) {
    switch (sensor_type)
    {
        case SENSOR_TYPE_CPU_TEMPERATURE:
            hardware_type_buf = "CPU";
            sensor_type_buf = "Temperature";
            sensor_group_buf = "Temperature";
            break;
        case SENSOR_TYPE_CPU_TEMPERATURE_ADDITIONAL:
            hardware_type_buf = "CPU";
            sensor_type_buf = "Temperature";
            sensor_group_buf = "Additional Temperature";
            break;
        case SENSOR_TYPE_CPU_MULTIPLIER:
            hardware_type_buf = "CPU";
            sensor_type_buf = "Multiplier";
            sensor_group_buf = "Multiplier";
            break;
        case SENSOR_TYPE_CPU_FREQUENCY_FSB:
            hardware_type_buf = "CPU";
            sensor_type_buf = "Frequency";
            sensor_group_buf = "FSB";
            break;
        case SENSOR_TYPE_CPU_LOAD:
            hardware_type_buf = "CPU";
            sensor_type_buf = "Percentage";
            sensor_group_buf = "Load";
            break;

        case SENSOR_TYPE_GPU_NAME:
            hardware_type_buf = "GPU";
            sensor_type_buf = "Text";
            sensor_group_buf = "Name";
            break;
        case SENSOR_TYPE_GPU_TEMPERATURE:
            hardware_type_buf = "GPU";
            sensor_type_buf = "Temperature";
            if (name.contains("Memory"))
            {
                sensor_group_buf = "Memory";
            }
            else
            {
                sensor_group_buf = "GPU";
            }
            break;
        case SENSOR_TYPE_GPU_FAN_SPEED_PERCENT:
            hardware_type_buf = "GPU";
            sensor_type_buf = "Percentage";
            sensor_group_buf = "Fan";
            break;
        case SENSOR_TYPE_GPU_FAN_SPEED_RPM:
            hardware_type_buf = "GPU";
            sensor_type_buf = "RPM";
            sensor_group_buf = "Fan";
            break;
        case SENSOR_TYPE_GPU_CORECLK:
            hardware_type_buf = "GPU";
            sensor_type_buf = "Frequency";
            sensor_group_buf = "GPU";
            break;
        case SENSOR_TYPE_GPU_MEMORYCLK:
            hardware_type_buf = "GPU";
            sensor_type_buf = "Frequency";
            sensor_group_buf = "Memory";
            break;
        case SENSOR_TYPE_GPU_SHARERCLK:
            hardware_type_buf = "GPU";
            sensor_type_buf = "Frequency";
            sensor_group_buf = "Share";
            break;
        case SENSOR_TYPE_GPU_MEMORY_USED_PERCENT:
            hardware_type_buf = "GPU";
            sensor_type_buf = "Percentage";
            sensor_group_buf = "Memory";
            break;
        case SENSOR_TYPE_GPU_MEMORY_USED_MB:
            hardware_type_buf = "GPU";
            sensor_type_buf = "Usage";
            sensor_group_buf = "Memory";
            break;
        case SENSOR_TYPE_GPU_LOAD:
            hardware_type_buf = "GPU";
            sensor_type_buf = "Load";
            sensor_group_buf = "GPU";
            break;
        case SENSOR_TYPE_GPU_POWER:
            hardware_type_buf = "GPU";
            sensor_type_buf = "Power";
            sensor_group_buf = "GPU";
            break;

        case SENSOR_TYPE_FAN_CONTROL_VALUE:
            hardware_type_buf = "Fan";
            sensor_type_buf = "Percentage";
            sensor_group_buf = "Power";
            break;
        case SENSOR_TYPE_FAN_SPEED_RPM:
            hardware_type_buf = "Fan";
            sensor_type_buf = "RPM";
            sensor_group_buf = "RPM";
            break;

        case SENSOR_TYPE_RAM_USAGE:
            hardware_type_buf = "RAM";
            if (name.contains("Total"))
            {
                sensor_type_buf = "Total";
            }
            else if (name.contains("Used"))
            {
                sensor_type_buf = "Usage";
            }
            else
            {
                sensor_type_buf = "Percentage";
            }
            sensor_group_buf = "RAM";
            break;

        case SENSOR_TYPE_DISK_TEMPERATURE:
            hardware_type_buf = "Drive";
            sensor_type_buf = "Temperature";
            sensor_group_buf = "Drive";
            break;
        case SENSOR_TYPE_DISK_TRANSFER_RATE:
            hardware_type_buf = "Drive";
            sensor_type_buf = "Transfer";
            sensor_group_buf = "Drive";
            break;

        case SENSOR_TYPE_NETWORK_SPEED:
            hardware_type_buf = "Network";
            sensor_type_buf = "Transfer";
            sensor_group_buf = "Network";
            break;

        case SENSOR_TYPE_BATTERY:
            hardware_type_buf = "Battery";
            sensor_type_buf = "Percentage";
            sensor_group_buf = "Battery";
            break;

        case SENSOR_TYPE_TEMPERATURE:
            hardware_type_buf = "Temperature";
            sensor_type_buf = "Temperature";
            sensor_group_buf = "Temperature Sensor";
            break;
        case SENSOR_TYPE_SYNTHETIC_TEMPERATURE:
            hardware_type_buf = "Temperature";
            sensor_type_buf = "Temperature";
            sensor_group_buf = "Synthetic Temperature";
            break;

        case SENSOR_TYPE_MAX_SENSORS:
            hardware_type_buf = "ArgusMonitor";
            sensor_type_buf = "Numeric";
            sensor_group_buf = "Sensor";
            break;

        case SENSOR_TYPE_INVALID:
        default:
            hardware_type_buf = "Invalid";
            sensor_type_buf = "Invalid";
            sensor_group_buf = "Invalid";
            break;
    }
}

// get the properly parsed float value for the specified sensor type
inline const float GetFloatValue(const float& value, const string& sensor_type) {
    if ("Transfer" == sensor_type) return (value * 1000000) / 131072; // MB => MiB, bytes => bits (1000/1024) * (1000/1024) * 8
    if ("Frequency" == sensor_type || "Clock" == sensor_type) return value * 1000000;
    if ("Usage" == sensor_type || "Total" == sensor_type) return (value * 1000000000) / 1024; // KB => KiB, B => MB (1000/1024) * 1_000_000
    return value;
}

// create a unique ID for the given sensor
inline const string SensorId(const string& hardware_type, const string& sensor_type, const string& sensor_group, const int& sensor_index, const int& data_index) {
    return hardware_type + "_" + sensor_type + "_" + sensor_group + "_" + to_string(sensor_index) + "_" + to_string(data_index);
}
