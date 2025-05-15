#include "pch.h"
#include "Link.h"
#include "argus_monitor_data_accessor.h"
#include "argus_monitor_data_api.h"
#include <string>

std::tuple<std::string, std::string, std::string> parse_types(argus_monitor::data_api::ARGUS_MONITOR_SENSOR_TYPE sensor_type, std::string name)
{
	switch (sensor_type)
	{
	case argus_monitor::data_api::SENSOR_TYPE_INVALID:
		return { "Invalid", "Invalid", "Invalid" };
	case argus_monitor::data_api::SENSOR_TYPE_CPU_TEMPERATURE:
		return { "CPU", "Temperature", "Temperature" };
	case argus_monitor::data_api::SENSOR_TYPE_CPU_TEMPERATURE_ADDITIONAL:
		return { "CPU", "Temperature", "Additional Temperature" };
	case argus_monitor::data_api::SENSOR_TYPE_GPU_TEMPERATURE:
		if (name.find("Memory") != std::string::npos) {
			return { "GPU", "Temperature", "Memory" };
		}
		return { "GPU", "Temperature", "GPU" };
	case argus_monitor::data_api::SENSOR_TYPE_DISK_TEMPERATURE:
		return { "Drive", "Temperature", "Drive" };
	case argus_monitor::data_api::SENSOR_TYPE_TEMPERATURE:
		return { "Mainboard", "Temperature", "Sensors" };
	case argus_monitor::data_api::SENSOR_TYPE_SYNTHETIC_TEMPERATURE:
		return { "ArgusMonitor", "Temperature", "Synthetic" };
	case argus_monitor::data_api::SENSOR_TYPE_GPU_FAN_SPEED_RPM:
		return { "GPU", "RPM", "Fan" };
	case argus_monitor::data_api::SENSOR_TYPE_FAN_SPEED_RPM:
		return { "Mainboard", "RPM", "Fan" };
	case argus_monitor::data_api::SENSOR_TYPE_GPU_FAN_SPEED_PERCENT:
		return { "GPU", "Percentage", "Fan" };
	case argus_monitor::data_api::SENSOR_TYPE_FAN_CONTROL_VALUE:
		return { "Mainboard", "Percentage", "Fan" };
	case argus_monitor::data_api::SENSOR_TYPE_NETWORK_SPEED:
		return { "Network", "Transfer", "Network" };
	case argus_monitor::data_api::SENSOR_TYPE_CPU_MULTIPLIER:
		return { "CPU", "Multiplier", "Multiplier" };
	case argus_monitor::data_api::SENSOR_TYPE_CPU_FREQUENCY_FSB:
		return { "CPU", "Frequency", "FSB" };
	case argus_monitor::data_api::SENSOR_TYPE_GPU_NAME:
		return { "GPU", "Text", "Name" };
	case argus_monitor::data_api::SENSOR_TYPE_GPU_LOAD:
		return { "GPU", "Load", "GPU" };
	case argus_monitor::data_api::SENSOR_TYPE_GPU_CORECLK:
		return { "GPU", "Clock", "GPU" };
	case argus_monitor::data_api::SENSOR_TYPE_GPU_MEMORYCLK:
		return { "GPU", "Clock", "Memory" };
	case argus_monitor::data_api::SENSOR_TYPE_GPU_SHARERCLK:
		return { "GPU", "Clock", "Share" };
	case argus_monitor::data_api::SENSOR_TYPE_GPU_MEMORY_USED_PERCENT:
		return { "GPU", "Percentage", "Memory" };
	case argus_monitor::data_api::SENSOR_TYPE_GPU_MEMORY_USED_MB:
		return { "GPU", "Usage", "Memory" };
	case argus_monitor::data_api::SENSOR_TYPE_GPU_POWER:
		return { "GPU", "Power", "GPU" };
	case argus_monitor::data_api::SENSOR_TYPE_DISK_TRANSFER_RATE:
		return { "Drive", "Transfer", "Drive" };
	case argus_monitor::data_api::SENSOR_TYPE_CPU_LOAD:
		return { "CPU", "Percentage", "Load" };
	case argus_monitor::data_api::SENSOR_TYPE_RAM_USAGE:
		if (name.find("Total") != std::string::npos) {
			return { "RAM", "Total", "RAM" };
		}
		if (name.find("Used") != std::string::npos) {
			return { "RAM", "Usage", "RAM" };
		}
		return { "RAM", "Percentage", "RAM" };
	case argus_monitor::data_api::SENSOR_TYPE_BATTERY:
		return { "Battery", "Percentage", "Battery" };
	case argus_monitor::data_api::SENSOR_TYPE_MAX_SENSORS:
	default:
		return { "","","" };
	}
}

ArgusMonitorLink::ArgusMonitorLink() : current_sensor_data() {}

int ArgusMonitorLink::start()
{
	if (running)
	{
		return 10;
	}

	running = true;
	auto const new_sensor_data_available = [this](argus_monitor::data_api::ArgusMonitorData const& new_sensor_data) {
		ArgusMonitorLink::current_sensor_data = new_sensor_data;
		};

	ArgusMonitorLink::data_accessor_.RegisterSensorCallbackOnDataChanged(new_sensor_data_available);
	return 0;
}

bool ArgusMonitorLink::check_connection() {
	return ArgusMonitorLink::data_accessor_.Open();
}

int ArgusMonitorLink::stop()
{
	if (!running)
	{
		return 10;
	}
	ArgusMonitorLink::data_accessor_.Close();
	running = false;
	return 0;
}

void ArgusMonitorLink::parse_sensor_data()
{
	argus_monitor::data_api::ArgusMonitorData argus_monitor_data = ArgusMonitorLink::current_sensor_data;

	std::string sensors = "";

	for (std::size_t index{}; index < argus_monitor_data.TotalSensorCount; ++index)
	{
		argus_monitor::data_api::ArgusMonitorSensorData sensor_data = argus_monitor_data.SensorData[index];
		std::wstring name(sensor_data.Label);
		std::string name_string(name.begin(), name.end());
		std::tuple < std::string, std::string, std::string> types = parse_types(sensor_data.SensorType, name_string);

		if (ArgusMonitorLink::enabled_sensors[std::get<0>(types)])
		{
			sensors.append(name_string);
			sensors.append("[<|>]");

			sensors.append(std::get<1>(types) != "Text" ? std::to_string(sensor_data.Value) : name_string);
			sensors.append("[<|>]");

			sensors.append(std::get<1>(types));
			sensors.append("[<|>]");

			sensors.append(std::get<0>(types));
			sensors.append("[<|>]");

			sensors.append(std::get<2>(types));
			sensors.append("]<|>[");
		}
	}

	char* data = new char[sensors.size()];
	strcpy(data, sensors.c_str());
	ArgusMonitorLink::parsed_sensor_data = data;
	ArgusMonitorLink::parsed_data_length = std::strlen(data);
}

int ArgusMonitorLink::get_data_length()
{
	return ArgusMonitorLink::parsed_data_length;
}

void ArgusMonitorLink::get_sensor_data(char* data, int maxlen)
{
	int length = min(maxlen - 1, ArgusMonitorLink::parsed_data_length);

	std::copy(ArgusMonitorLink::parsed_sensor_data, ArgusMonitorLink::parsed_sensor_data + length, data);
	data[length] = 0;
}

bool ArgusMonitorLink::check_data()
{
	return ArgusMonitorLink::current_sensor_data.Signature == 0x4D677241 && ArgusMonitorLink::current_sensor_data.TotalSensorCount > 0;
}

void ArgusMonitorLink::set_sensor_enabled(char* name, bool enabled)
{
	ArgusMonitorLink::enabled_sensors.at(std::string(name)) = enabled;
}