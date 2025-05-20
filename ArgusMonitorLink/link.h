/**
Argus Monitor Data API, based on https://github.com/argotronic/argus_data_api/blob/master/argus_monitor_data_accessor.h
Modified to not use multiple threads and "push" updates to a function but rather be completely poll based to save on memory and cpu

Copyright (C) 2025 Zeanon
Original License from https://github.com/argotronic/argus_data_api still applies
**/

#pragma once
#include "argus_monitor_data_api.h"
#include <list>
#include <map>
#include <string>


using namespace std;

namespace argus_monitor {
	namespace data_api {
		namespace {
			class Lock {
			private:
				HANDLE mutex_handle_;

			public:
				explicit Lock(HANDLE &mutex_handle)
					: mutex_handle_{ mutex_handle }
				{
					WaitForSingleObject(mutex_handle_, INFINITE);
				}
				~Lock() { ReleaseMutex(mutex_handle_); }
			};
		}

		class ArgusMonitorLink {
		private:
			HANDLE                                           handle_file_mapping{ nullptr };
			void*                                            pointer_to_mapped_data{ nullptr };
			bool                                             is_open_{ false };
			uint32_t                                         last_cycle_counter{ 0 };
			const argus_monitor::data_api::ArgusMonitorData* argus_monitor_data{ nullptr };

			map<const string, bool> enabled_hardware = {
				{"CPU", true},
				{"GPU", true},
				{"RAM", true},
				{"Mainboard", true},
				{"Drive", true},
				{"Network", true},
				{"Battery", true},
				{"ArgusMonitor", true}
			};

			static HANDLE OpenArgusApiMutex();
		public:
			ArgusMonitorLink() = default;

			ArgusMonitorLink(ArgusMonitorLink const&)            = delete;
			ArgusMonitorLink(ArgusMonitorLink&&)                 = delete;
			ArgusMonitorLink& operator=(ArgusMonitorLink const&) = delete;
			ArgusMonitorLink& operator=(ArgusMonitorLink&&)      = delete;

			~ArgusMonitorLink() { Close(); }

			int  Open();
			bool IsOpen() const noexcept { return is_open_; }
			int  Close();

			bool CheckArgusSignature() const;
			int  GetTotalSensorCount() const;
			bool GetSensorData(void (process_sensor_data)(const char* sensor_name,
			                                              const char* sensor_value,
			                                              const char* sensor_type,
			                                              const char* hardware_type,
			                                              const char* sensor_group));

			void SetHardwareEnabled(const string &type, const bool &enabled);
			bool IsHardwareEnabled(const string &type) const;
		};
	}
}

using namespace argus_monitor::data_api;

// Create an instance of ArgusMonitorLink
extern "C" _declspec(dllexport) void* Instantiate() {
	return (void*) new ArgusMonitorLink();
}

// Open the connection to Argus Monitor
// return:
//  0: connection is open
//  1: could not open file mapping
// 10: could not optain fileview
extern "C" _declspec(dllexport) int Open(ArgusMonitorLink* t) {
	return t->Open();
}

// Check whether the connection is already open
extern "C" _declspec(dllexport) bool IsOpen(ArgusMonitorLink* t) {
	return t->IsOpen();
}

// Clean up the file handle and unmap the fileview
// return:
//  0: successfully unmaped the fileview and closed the handle
//  1: Could not unmap the fileview
// 10: Could not close the handle
// 11: Neither was possible
extern "C" _declspec(dllexport) int Close(ArgusMonitorLink* t) {
	return t->Close();
}

// Check whether ArgusMonitor is active
extern "C" _declspec(dllexport) bool CheckArgusSignature(ArgusMonitorLink* t) {
	return t->CheckArgusSignature();
}

// Get the total amount of sensors provided by Argus Monitor
extern "C" _declspec(dllexport) int GetTotalSensorCount(ArgusMonitorLink* t) {
	return t->GetTotalSensorCount();
}

// Get the data from argus monitor and if its new, create arrays that hold the sensor data and then use the passed add method to
// add it to an external collection
// returns true if new data was available and false if no new data was available
extern "C" _declspec(dllexport) bool GetSensorData(ArgusMonitorLink* t,
                                                   void (process_sensor_data)(const char* sensor_name,
                                                                              const char* sensor_value,
                                                                              const char* sensor_type,
                                                                              const char* hardware_type,
                                                                              const char* sensor_group)) {
	return t->GetSensorData(process_sensor_data);
}

// Set the given hardware type to enabled/disabled
extern "C" _declspec(dllexport) void SetHardwareEnabled(ArgusMonitorLink* t, const char* type, const bool enabled) {
	t->SetHardwareEnabled(type, enabled);
}

// Check whether the given hardware type is enabled
extern "C" _declspec(dllexport) bool IsHardwareEnabled(ArgusMonitorLink* t, const char* type) {
	return t->IsHardwareEnabled(type);
}

extern "C" _declspec(dllexport) void Destroy(ArgusMonitorLink* t) {
	t->~ArgusMonitorLink();
}
