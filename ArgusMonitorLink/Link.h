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
				explicit Lock(HANDLE mutex_handle)
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
			const argus_monitor::data_api::ArgusMonitorData* sensor_data{ nullptr };

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

			static HANDLE OpenArgusApiMutex();
		public:
			ArgusMonitorLink() = default;

			ArgusMonitorLink(ArgusMonitorLink const&) = delete;
			ArgusMonitorLink(ArgusMonitorLink&&) = delete;
			ArgusMonitorLink& operator=(ArgusMonitorLink const&) = delete;
			ArgusMonitorLink& operator=(ArgusMonitorLink&&) = delete;

			~ArgusMonitorLink() { Close(); }

			bool Open();
			bool IsOpen() const noexcept { return is_open_; }
			int  Close();

			bool CheckArgusSignature() const;
			int  GetTotalSensorCount() const;
			void GetSensorData(void (add)(const char* sensor[]));

			void set_sensor_enabled(const char* type, bool enabled);
			bool get_sensor_enabled(const char* type) const;
		};
	}
}

using namespace argus_monitor::data_api;

// Helper methods for constructor and other method
extern "C" _declspec(dllexport) void* Instantiate() {
	return (void*) new ArgusMonitorLink();
}

extern "C" _declspec(dllexport) bool Open(ArgusMonitorLink* t) {
	return t->Open();
}

extern "C" _declspec(dllexport) bool IsOpen(ArgusMonitorLink* t) {
	return t->IsOpen();
}

extern "C" _declspec(dllexport) int Close(ArgusMonitorLink* t) {
	return t->Close();
}

extern "C" _declspec(dllexport) int GetTotalSensorCount(ArgusMonitorLink* t) {
	return t->GetTotalSensorCount();
}

extern "C" _declspec(dllexport) void GetSensorData(ArgusMonitorLink* t, void (add)(const char* sensor[])) {
	t->GetSensorData(add);
}

extern "C" _declspec(dllexport) bool CheckArgusSignature(ArgusMonitorLink* t) {
	return t->CheckArgusSignature();
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