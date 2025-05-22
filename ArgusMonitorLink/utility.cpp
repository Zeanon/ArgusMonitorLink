#include "pch.h"
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
		return { "Mainboard", "Temperature", "Sensor" };

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
		return { "ArgusMonitor", "Numeric", "Sensor" };

	case argus_monitor::data_api::SENSOR_TYPE_INVALID:
	default:
		return { "Invalid", "Invalid", "Invalid" };
	}
}

const double get_double_value(const double& value, const string& sensor_type)
{
	if (sensor_type == "Transfer") return value * 8;
	if (sensor_type == "Usage" || sensor_type == "Total" || sensor_type == "Frequency" || sensor_type == "Clock") return value * 1000000;
	return value;
}

const string core_clock_id(const string& hardware_type, const string& sensor_name, const int& sensor_index, const int& data_index)
{
	return hardware_type + "_Frequency_Core_Clock_" + sensor_name + "_" + to_string(sensor_index) + "_" + to_string(data_index);
}

const string sensor_id(const string& hardware_type, const string& sensor_type, const string& sensor_group, const string& sensor_name, const int& sensor_index, const int& data_index)
{
	return hardware_type + "_" + sensor_type + "_" + sensor_group + "_" + sensor_name + "_" + to_string(sensor_index) + "_" + to_string(data_index);
}

const vector<double> min_max_average(const vector<double>& values)
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
	return { min_value, max_value, sum / values.size() };
}