#pragma once
#include "argus_monitor_data_api.h"
#include "argus_monitor_data_accessor.h"
#include <string>
#include <map>

class ArgusMonitorLink {
	argus_monitor::data_api::ArgusMonitorDataAccessor data_accessor_{};

	argus_monitor::data_api::ArgusMonitorData current_sensor_data;
	int parsed_data_length = 0;
	const char* parsed_sensor_data = "";

	std::map<std::string, bool> enabled_sensors = {
		{"CPU", true},
		{"GPU", true},
		{"RAM", true},
		{"Mainboard", true},
		{"Drive", true},
		{"Network", true},
		{"Battery", true},
		{"ArgusMonitor", true}
	};
public:
	ArgusMonitorLink();

	void start();
	bool check_connection();
	void close();

	void parse_sensor_data();
	int get_data_length();
	void get_sensor_data(char* data, int maxlen);
	bool check_data();

	void set_sensor_enabled(char* name, bool enabled);
};

// Helper methods for constructor and other method
extern "C" _declspec(dllexport) void* Instantiate() {
	return (void*) new ArgusMonitorLink();
}

extern "C" _declspec(dllexport) void Start(ArgusMonitorLink* t) {
	t->start();
}

extern "C" _declspec(dllexport) bool CheckConnection(ArgusMonitorLink* t) {
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

extern "C" _declspec(dllexport) void GetSensorData(ArgusMonitorLink* t, char* data, int maxlen) {
	t->get_sensor_data(data, maxlen);
}

extern "C" _declspec(dllexport) bool CheckData(ArgusMonitorLink* t) {
	return t->check_data();
}

extern "C" _declspec(dllexport) void SetSensorEnabled(ArgusMonitorLink* t, char* name, bool enabled) {
	t->set_sensor_enabled(name, enabled);
}
