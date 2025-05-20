#include "pch.h"
#include "argus_monitor_data_api.h"
#include "Link.h"
#include <list>
#include <memory>
#include <stdexcept>
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

namespace argus_monitor {
	namespace data_api {
		// Open the connection to Argus Monitor
		bool ArgusMonitorLink::Open()
		{
			if (is_open_) {
				return true;
			}
			handle_file_mapping = OpenFileMappingW(FILE_MAP_READ | FILE_MAP_WRITE,             // read/write access
				FALSE,                                       // do not inherit the name
				argus_monitor::data_api::kMappingName());    // name of mapping object

			if (handle_file_mapping == nullptr) {
				return false;
			}

			pointer_to_mapped_data = MapViewOfFile(handle_file_mapping,               // handle to map object
				FILE_MAP_READ | FILE_MAP_WRITE,    // read/write permission
				0, 0, argus_monitor::data_api::kMappingSize());

			if (pointer_to_mapped_data == nullptr) {
				CloseHandle(handle_file_mapping);
				return false;
			}

			sensor_data = reinterpret_cast<argus_monitor::data_api::ArgusMonitorData const*>(pointer_to_mapped_data);
			last_cycle_counter = sensor_data->CycleCounter;

			is_open_ = true;

			return true;
		}

		// Close the connection with Argus Monitor and clean up the stored data
		int ArgusMonitorLink::Close()
		{
			is_open_ = false;

			int success{ 0 };
			sensor_data = nullptr;
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
			if (nullptr == ArgusMonitorLink::OpenArgusApiMutex() || nullptr == sensor_data) {
				return false;
			}
			return sensor_data->Signature == 0x4D677241;
		}

		int ArgusMonitorLink::GetTotalSensorCount() const
		{
			if (nullptr == ArgusMonitorLink::OpenArgusApiMutex() || nullptr == sensor_data) {
				return 0;
			}
			return sensor_data->TotalSensorCount;
		}

		void ArgusMonitorLink::GetSensorData(void (add)(const char* sensor[]))
		{
			if (nullptr == sensor_data) {
				return;
			}

			HANDLE mutex_handle = ArgusMonitorLink::OpenArgusApiMutex();
			if (nullptr == mutex_handle) {
				return;
			}

			{
				Lock scoped_lock(mutex_handle);
				if (last_cycle_counter != sensor_data->CycleCounter)
				{
					last_cycle_counter = sensor_data->CycleCounter;
					for (size_t index{}; index < sensor_data->TotalSensorCount; ++index)
					{
						const wstring label(sensor_data->SensorData[index].Label);
						const string name(label.begin(), label.end());
						const auto types = parse_types(sensor_data->SensorData[index].SensorType, name);

						if (enabled_sensors.at(types[0]))
						{
							//Sensor: <Name, Value, SensorType, HarwareType, Group>
							const auto value = types[1] != "Text" ? to_string(sensor_data->SensorData[index].Value) : name;
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