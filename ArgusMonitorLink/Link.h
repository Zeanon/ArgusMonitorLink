#pragma once
#include "argus_monitor_data_accessor.h"
#include "argus_monitor_data_api.h"
#include <list>
#include <map>
#include <string>


using namespace std;

class ArgusMonitorLink {
private:
	argus_monitor::data_api::ArgusMonitorDataAccessor* data_accessor_ = nullptr;
	const argus_monitor::data_api::ArgusMonitorData* current_sensor_data = nullptr;

	map<const string, bool> enabled_sensors = {
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
	~ArgusMonitorLink() { close(); }

	int register_callback(const DWORD polling_interval);
	bool open() const;
	int close();

	bool check_argus_signature() const;
	int get_total_sensor_count() const;
	void get_sensor_data(void (add)(const char* sensor[])) const;

	void set_sensor_enabled(const char* type, bool enabled);
	bool get_sensor_enabled(const char* type) const;
};

// Helper methods for constructor and other method
extern "C" _declspec(dllexport) void* Instantiate() {
	return (void*) new ArgusMonitorLink();
}

extern "C" _declspec(dllexport) int Register(ArgusMonitorLink* t, const DWORD polling_interval) {
	return t->register_callback(polling_interval);
}

extern "C" _declspec(dllexport) bool Open(ArgusMonitorLink* t) {
	return t->open();
}

extern "C" _declspec(dllexport) int Close(ArgusMonitorLink* t) {
	return t->close();
}

extern "C" _declspec(dllexport) int GetTotalSensorCount(ArgusMonitorLink* t) {
	return t->get_total_sensor_count();
}

extern "C" _declspec(dllexport) void GetSensorData(ArgusMonitorLink* t, void (add)(const char* sensor[])) {
	t->get_sensor_data(add);
}

extern "C" _declspec(dllexport) bool CheckArgusSignature(ArgusMonitorLink* t) {
	return t->check_argus_signature();
}

extern "C" _declspec(dllexport) void SetSensorEnabled(ArgusMonitorLink* t, const char* type, const bool enabled) {
	t->set_sensor_enabled(type, enabled);
}

extern "C" _declspec(dllexport) bool GetSensorEnabled(ArgusMonitorLink* t, const char* type) {
	return t->get_sensor_enabled(type);
}

extern "C" _declspec(dllexport) void Destroy(ArgusMonitorLink* t) {
	t->~ArgusMonitorLink();
}