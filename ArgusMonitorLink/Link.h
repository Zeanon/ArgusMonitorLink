#pragma once
#include "argus_monitor_data_api.h"
#include "argus_monitor_data_accessor.h"
#include <string>
#include <map>

class ArgusMonitorLink {
	argus_monitor::data_api::ArgusMonitorDataAccessor data_accessor_{};

	argus_monitor::data_api::ArgusMonitorData current_sensor_data;
	int parsed_data_length = 0;
	const char *parsed_sensor_data = "";

	std::map<std::string, int> enabled_sensors = {
		{"CPU", 1},
		{"GPU", 1},
		{"RAM", 1},
		{"Mainboard", 1},
		{"Drive", 1},
		{"Network", 1},
		{"Battery", 1},
		{"ArgusMonitor", 1}
	};
public:
	ArgusMonitorLink();

	int start();
	int check_connection();
	void close();

	void parse_sensor_data();
	int get_data_length();
	void get_sensor_data(char *data, int maxlen);

	void set_sensor_enabled(char* name, int enabled);
};

// Helper methods for constructor and other method
extern "C" _declspec(dllexport) void* Instantiate() {
	return (void*) new ArgusMonitorLink();
}

extern "C" _declspec(dllexport) int Start(ArgusMonitorLink* t) {
	return t->start();
}

extern "C" _declspec(dllexport) int CheckConnection(ArgusMonitorLink* t) {
	return t->check_connection();
}

extern "C" _declspec(dllexport) void Close(ArgusMonitorLink* t) {
	t->close();
}

extern "C" _declspec(dllexport) void ParseSensorData(ArgusMonitorLink* t) {
	t->parse_sensor_data();
}

extern "C" _declspec(dllexport) int GetDataLength(ArgusMonitorLink* t) {
	return t->get_data_length();
}

extern "C" _declspec(dllexport) void GetSensorData(ArgusMonitorLink* t, char *data, int maxlen) {
	t->get_sensor_data(data, maxlen);
}

extern "C" _declspec(dllexport) void SetSensorEnabled(ArgusMonitorLink* t, char* name, int enabled) {
	t->set_sensor_enabled(name, enabled);
}
