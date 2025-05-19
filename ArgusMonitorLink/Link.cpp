#include "pch.h"
#include "argus_monitor_data_accessor.h"
#include "argus_monitor_data_api.h"
#include "Link.h"
#include <list>
#include <memory>
#include <string>
#include <vector>


using namespace std;

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
		return { "ArgusMonitor", "Temperature", "Synthetic" };

	case argus_monitor::data_api::SENSOR_TYPE_INVALID:
		return { "Invalid", "Invalid", "Invalid" };

	case argus_monitor::data_api::SENSOR_TYPE_MAX_SENSORS:
	default:
		return { "","","" };
	}
}

int ArgusMonitorLink::register_callback(const DWORD polling_interval)
{
	if (nullptr != data_accessor_)
	{
		return 10;
	}

	ArgusMonitorLink::data_accessor_ = new argus_monitor::data_api::ArgusMonitorDataAccessor(polling_interval);

	auto const new_sensor_data_available = [this](argus_monitor::data_api::ArgusMonitorData const& new_sensor_data) {
		current_sensor_data = &new_sensor_data;
		};

	data_accessor_->RegisterSensorCallbackOnDataChanged(new_sensor_data_available);
	return 0;
}

bool ArgusMonitorLink::open() const
{
	if (nullptr == data_accessor_)
	{
		return false;
	}
	return data_accessor_->Open();
}

int ArgusMonitorLink::close()
{
	auto _close = [this]() {
		data_accessor_->Close();
		return 0;
		};
	int result = data_accessor_ != nullptr ? _close() : 10;
	delete data_accessor_;
	//delete current_sensor_data;
	data_accessor_ = nullptr;
	current_sensor_data = nullptr;
	return result;
}

// Check whether ArgusMonitor is active
bool ArgusMonitorLink::check_argus_signature() const
{
	if (nullptr == current_sensor_data)
	{
		return false;
	}
	return current_sensor_data->Signature == 0x4D677241;
}

int ArgusMonitorLink::get_total_sensor_count() const
{
	if (nullptr == current_sensor_data)
	{
		return 0;
	}
	return current_sensor_data->TotalSensorCount;
}

void ArgusMonitorLink::get_sensor_data(void (add)(const char* sensor[])) const
{
	if (nullptr == current_sensor_data)
	{
		return;
	}

	for (size_t index{}; index < current_sensor_data->TotalSensorCount; ++index)
	{
		const wstring label(current_sensor_data->SensorData[index].Label);
		const string name(label.begin(), label.end());
		const auto types = parse_types(current_sensor_data->SensorData[index].SensorType, name);

		if (enabled_sensors.at(types[0]))
		{
			//Sensor: <Name, Value, SensorType, HarwareType, Group>
			const auto value = types[1] != "Text" ? to_string(current_sensor_data->SensorData[index].Value) : name;
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
}

void ArgusMonitorLink::set_sensor_enabled(const char* type, const bool enabled)
{
	enabled_sensors[type] = enabled;
}

bool ArgusMonitorLink::get_sensor_enabled(const char* type) const
{
	try
	{
		return enabled_sensors.at(type);
	}
	catch (out_of_range e)
	{
		return false;
	}
}