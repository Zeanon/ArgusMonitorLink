/**
Argus Monitor Data API, loosely based on https://github.com/argotronic/argus_data_api/blob/master/argus_monitor_data_accessor.h
Modified to not use multiple threads and "push" updates to a function but rather be completely poll based to save on memory and cpu

Copyright (C) 2025 Zeanon
Original License from https://github.com/argotronic/argus_data_api still applies.
**/

#pragma once

#include "ArgusMonitor/argus_monitor_data_api.h"
#include "dll/pch.h"
#include "utility/utility.h"
#include "Version/version.h"
#include <algorithm>
#include <list>
#include <map>
#include <memory>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>

using namespace std;

namespace argus_monitor
{
    namespace data_api
    {
        namespace
        {
            class Lock
            {
            private:
                const HANDLE mutex_handle_;

            public:
                explicit Lock(const HANDLE& mutex_handle)
                    : mutex_handle_{ mutex_handle }
                {
                    WaitForSingleObject(mutex_handle_, INFINITE);
                }

                ~Lock() { ReleaseMutex(mutex_handle_); }
            };
        }

        class ArgusMonitorLink
        {
        private:
            bool                                             is_open             { false };
            HANDLE                                           file_mapping_handle { nullptr };
            HANDLE                                           mutex_handle        { nullptr };
            const ArgusMonitorData*                          argus_monitor_data  { nullptr };
            uint32_t                                         last_cycle_counter  { 0 };

            map<const string, bool> enabled_hardware = {
                {"CPU", true},
                {"GPU", true},
                {"RAM", true},
                {"Fan", true},
                {"Drive", true},
                {"Network", true},
                {"Battery", true},
                {"Temperature", true},
                {"ArgusMonitor", true}
            };

            static inline HANDLE OpenArgusApiMutex() { return OpenMutexW(READ_CONTROL | MUTANT_QUERY_STATE | SYNCHRONIZE, FALSE, kMutexName()); }
        public:
            ArgusMonitorLink() = default;

            ArgusMonitorLink(ArgusMonitorLink const&)            = delete;
            ArgusMonitorLink(ArgusMonitorLink&&)                 = delete;
            ArgusMonitorLink& operator=(ArgusMonitorLink const&) = delete;
            ArgusMonitorLink& operator=(ArgusMonitorLink&&)      = delete;

            ~ArgusMonitorLink() { Close(); }

            int  Open();
            inline bool IsOpen() const noexcept { return is_open; }
            int  Close();

            inline bool CheckArgusSignature() const { return 0x4D677241 == argus_monitor_data->Signature; }
            inline int  GetTotalSensorCount() const { return argus_monitor_data->TotalSensorCount; }
            void GetSensorData(void (process_sensor_data)(const char* sensor_name,
                                                          const char* sensor_value,
                                                          const char* sensor_type,
                                                          const char* hardware_type,
                                                          const char* sensor_group,
                                                          const char* sensor_index,
                                                          const char* data_index));
            bool UpdateSensorData(void (update)(const char* sensor_id, const float sensor_value));

            inline void SetHardwareEnabled(const string& type, const bool& enabled) { enabled_hardware[type] = enabled; }
            inline bool IsHardwareEnabled(const string& type) const {
                try { return enabled_hardware.at(type); }
                catch (const out_of_range& _) { return false; }
            }
        };
    }
}

using namespace argus_monitor::data_api;

// Create an instance of ArgusMonitorLink
extern "C" _declspec(dllexport) void* Create()
{
    return (void*) new ArgusMonitorLink();
}

// Open the connection to Argus Monitor
// return:
//   0: connection is open
//   1: could not open file mapping
//  10: could not optain fileview
// 100: could not open ArgusApiMutex
extern "C" _declspec(dllexport) int Open(ArgusMonitorLink* argus_monitor_link_ptr)
{
    return argus_monitor_link_ptr->Open();
}

// Check whether the connection is already open
extern "C" _declspec(dllexport) bool IsOpen(ArgusMonitorLink* argus_monitor_link_ptr)
{
    return argus_monitor_link_ptr->IsOpen();
}

// Clean up the file handle and unmap the fileview
// return:
//  0: successfully unmaped the fileview and closed the handle
//  1: Could not unmap the fileview
// 10: Could not close the handle
// 11: Neither was possible
extern "C" _declspec(dllexport) int Close(ArgusMonitorLink* argus_monitor_link_ptr)
{
    return argus_monitor_link_ptr->Close();
}

// Check whether ArgusMonitor is active
extern "C" _declspec(dllexport) bool CheckArgusSignature(ArgusMonitorLink* argus_monitor_link_ptr)
{
    return argus_monitor_link_ptr->CheckArgusSignature();
}

// Get the total amount of sensors provided by Argus Monitor
extern "C" _declspec(dllexport) int GetTotalSensorCount(ArgusMonitorLink* argus_monitor_link_ptr)
{
    return argus_monitor_link_ptr->GetTotalSensorCount();
}

// Get the data from argus monitor and if its new, create arrays that hold the sensor data and then use the passed add method to
// add it to an external collection
extern "C" _declspec(dllexport) void GetSensorData(ArgusMonitorLink* argus_monitor_link_ptr,
                                                   void (process_sensor_data)(const char* sensor_name,
                                                                              const char* sensor_value,
                                                                              const char* sensor_type,
                                                                              const char* hardware_type,
                                                                              const char* sensor_group,
                                                                              const char* sensor_index,
                                                                              const char* data_index))
{
    argus_monitor_link_ptr->GetSensorData(process_sensor_data);
}

// Update the non static sensors
// returns true if new data was available and false if no new data was available
extern "C" _declspec(dllexport) bool UpdateSensorData(ArgusMonitorLink* argus_monitor_link_ptr,
                                                      void (update)(const char* sensor_id, const float sensor_value))
{
    return argus_monitor_link_ptr->UpdateSensorData(update);
}

// Set the given hardware type to enabled/disabled
extern "C" _declspec(dllexport) void SetHardwareEnabled(ArgusMonitorLink* argus_monitor_link_ptr, const char* type, const bool enabled)
{
    argus_monitor_link_ptr->SetHardwareEnabled(type, enabled);
}

// Check whether the given hardware type is enabled
extern "C" _declspec(dllexport) bool IsHardwareEnabled(ArgusMonitorLink* argus_monitor_link_ptr, const char* type)
{
    return argus_monitor_link_ptr->IsHardwareEnabled(type);
}

// Delete the given instance
// This needs to be called to ensure proper memory cleanup
extern "C" _declspec(dllexport) void Destroy(ArgusMonitorLink* argus_monitor_link_ptr)
{
    delete argus_monitor_link_ptr;
}
