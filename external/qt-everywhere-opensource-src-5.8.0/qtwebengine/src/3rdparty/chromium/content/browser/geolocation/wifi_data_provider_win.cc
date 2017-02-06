// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Windows Vista uses the Native Wifi (WLAN) API for accessing WiFi cards. See
// http://msdn.microsoft.com/en-us/library/ms705945(VS.85).aspx. Windows XP
// Service Pack 3 (and Windows XP Service Pack 2, if upgraded with a hot fix)
// also support a limited version of the WLAN API. See
// http://msdn.microsoft.com/en-us/library/bb204766.aspx. The WLAN API uses
// wlanapi.h, which is not part of the SDK used by Gears, so is replicated
// locally using data from the MSDN.
//
// Windows XP from Service Pack 2 onwards supports the Wireless Zero
// Configuration (WZC) programming interface. See
// http://msdn.microsoft.com/en-us/library/ms706587(VS.85).aspx.
//
// The MSDN recommends that one use the WLAN API where available, and WZC
// otherwise.
//
// However, it seems that WZC fails for some wireless cards. Also, WLAN seems
// not to work on XP SP3. So we use WLAN on Vista, and use NDIS directly
// otherwise.

#include "content/browser/geolocation/wifi_data_provider_win.h"

#include <windows.h>
#include <winioctl.h>
#include <wlanapi.h>

#include "base/memory/free_deleter.h"
#include "base/metrics/histogram.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/windows_version.h"
#include "content/browser/geolocation/wifi_data_provider_common.h"
#include "content/browser/geolocation/wifi_data_provider_common_win.h"
#include "content/browser/geolocation/wifi_data_provider_manager.h"

// Taken from ndis.h for WinCE.
#define NDIS_STATUS_INVALID_LENGTH   ((NDIS_STATUS)0xC0010014L)
#define NDIS_STATUS_BUFFER_TOO_SHORT ((NDIS_STATUS)0xC0010016L)

namespace content {
namespace {
// The limits on the size of the buffer used for the OID query.
const int kInitialBufferSize = 2 << 12;  // Good for about 50 APs.
const int kMaximumBufferSize = 2 << 20;  // 2MB

// Length for generic string buffers passed to Windows APIs.
const int kStringLength = 512;

// The time periods, in milliseconds, between successive polls of the wifi data.
const int kDefaultPollingInterval = 10000;  // 10s
const int kNoChangePollingInterval = 120000;  // 2 mins
const int kTwoNoChangePollingInterval = 600000;  // 10 mins
const int kNoWifiPollingIntervalMilliseconds = 20 * 1000; // 20s

// WlanOpenHandle
typedef DWORD (WINAPI* WlanOpenHandleFunction)(DWORD dwClientVersion,
                                               PVOID pReserved,
                                               PDWORD pdwNegotiatedVersion,
                                               PHANDLE phClientHandle);

// WlanEnumInterfaces
typedef DWORD (WINAPI* WlanEnumInterfacesFunction)(
    HANDLE hClientHandle,
    PVOID pReserved,
    PWLAN_INTERFACE_INFO_LIST* ppInterfaceList);

// WlanGetNetworkBssList
typedef DWORD (WINAPI* WlanGetNetworkBssListFunction)(
    HANDLE hClientHandle,
    const GUID* pInterfaceGuid,
    const  PDOT11_SSID pDot11Ssid,
    DOT11_BSS_TYPE dot11BssType,
    BOOL bSecurityEnabled,
    PVOID pReserved,
    PWLAN_BSS_LIST* ppWlanBssList
);

// WlanFreeMemory
typedef VOID (WINAPI* WlanFreeMemoryFunction)(PVOID pMemory);

// WlanCloseHandle
typedef DWORD (WINAPI* WlanCloseHandleFunction)(HANDLE hClientHandle,
                                                PVOID pReserved);


// Local classes and functions
class WindowsWlanApi : public WifiDataProviderCommon::WlanApiInterface {
 public:
  ~WindowsWlanApi() override;
  // Factory function. Will return NULL if this API is unavailable.
  static WindowsWlanApi* Create();

  // WlanApiInterface
  bool GetAccessPointData(WifiData::AccessPointDataSet* data) override;

 private:
  // Takes ownership of the library handle.
  explicit WindowsWlanApi(HINSTANCE library);

  // Loads the required functions from the DLL.
  void GetWLANFunctions(HINSTANCE wlan_library);
  int GetInterfaceDataWLAN(HANDLE wlan_handle,
                           const GUID& interface_id,
                           WifiData::AccessPointDataSet* data);

  // Logs number of detected wlan interfaces.
  static void LogWlanInterfaceCount(int count);

  // Handle to the wlanapi.dll library.
  HINSTANCE library_;

  // Function pointers for WLAN
  WlanOpenHandleFunction WlanOpenHandle_function_;
  WlanEnumInterfacesFunction WlanEnumInterfaces_function_;
  WlanGetNetworkBssListFunction WlanGetNetworkBssList_function_;
  WlanFreeMemoryFunction WlanFreeMemory_function_;
  WlanCloseHandleFunction WlanCloseHandle_function_;
};

class WindowsNdisApi : public WifiDataProviderCommon::WlanApiInterface {
 public:
  ~WindowsNdisApi() override;
  static WindowsNdisApi* Create();

  // WlanApiInterface
  bool GetAccessPointData(WifiData::AccessPointDataSet* data) override;

 private:
  static bool GetInterfacesNDIS(
      std::vector<base::string16>* interface_service_names_out);

  // Swaps in content of the vector passed
  explicit WindowsNdisApi(std::vector<base::string16>* interface_service_names);

  bool GetInterfaceDataNDIS(HANDLE adapter_handle,
                            WifiData::AccessPointDataSet* data);
  // NDIS variables.
  std::vector<base::string16> interface_service_names_;

  // Remembers scan result buffer size across calls.
  int oid_buffer_size_;
};

// Extracts data for an access point and converts to Gears format.
bool GetNetworkData(const WLAN_BSS_ENTRY& bss_entry,
                    AccessPointData* access_point_data);
bool UndefineDosDevice(const base::string16& device_name);
bool DefineDosDeviceIfNotExists(const base::string16& device_name);
HANDLE GetFileHandle(const base::string16& device_name);
// Makes the OID query and returns a Windows API error code.
int PerformQuery(HANDLE adapter_handle,
                 BYTE* buffer,
                 DWORD buffer_size,
                 DWORD* bytes_out);
bool ResizeBuffer(int requested_size,
                  std::unique_ptr<BYTE, base::FreeDeleter>* buffer);
// Gets the system directory and appends a trailing slash if not already
// present.
bool GetSystemDirectory(base::string16* path);
}  // namespace

WifiDataProvider* WifiDataProviderManager::DefaultFactoryFunction() {
  return new WifiDataProviderWin();
}

WifiDataProviderWin::WifiDataProviderWin() {
}

WifiDataProviderWin::~WifiDataProviderWin() {
}

WifiDataProviderCommon::WlanApiInterface* WifiDataProviderWin::NewWlanApi() {
  // Use the WLAN interface if we're on Vista and if it's available. Otherwise,
  // use NDIS.
  WlanApiInterface* api = WindowsWlanApi::Create();
  if (api) {
    return api;
  }
  return WindowsNdisApi::Create();
}

WifiPollingPolicy* WifiDataProviderWin::NewPollingPolicy() {
  return new GenericWifiPollingPolicy<kDefaultPollingInterval,
                                      kNoChangePollingInterval,
                                      kTwoNoChangePollingInterval,
                                      kNoWifiPollingIntervalMilliseconds>;
}

// Local classes and functions
namespace {

// WindowsWlanApi
WindowsWlanApi::WindowsWlanApi(HINSTANCE library)
    : library_(library) {
  GetWLANFunctions(library_);
}

WindowsWlanApi::~WindowsWlanApi() {
  FreeLibrary(library_);
}

WindowsWlanApi* WindowsWlanApi::Create() {
  if (base::win::GetVersion() < base::win::VERSION_VISTA)
    return NULL;
  // We use an absolute path to load the DLL to avoid DLL preloading attacks.
  base::string16 system_directory;
  if (!GetSystemDirectory(&system_directory)) {
    return NULL;
  }
  DCHECK(!system_directory.empty());
  base::string16 dll_path = system_directory + L"wlanapi.dll";
  HINSTANCE library = LoadLibraryEx(dll_path.c_str(),
                                    NULL,
                                    LOAD_WITH_ALTERED_SEARCH_PATH);
  if (!library) {
    return NULL;
  }
  return new WindowsWlanApi(library);
}

void WindowsWlanApi::GetWLANFunctions(HINSTANCE wlan_library) {
  DCHECK(wlan_library);
  WlanOpenHandle_function_ = reinterpret_cast<WlanOpenHandleFunction>(
      GetProcAddress(wlan_library, "WlanOpenHandle"));
  WlanEnumInterfaces_function_ = reinterpret_cast<WlanEnumInterfacesFunction>(
      GetProcAddress(wlan_library, "WlanEnumInterfaces"));
  WlanGetNetworkBssList_function_ =
      reinterpret_cast<WlanGetNetworkBssListFunction>(
      GetProcAddress(wlan_library, "WlanGetNetworkBssList"));
  WlanFreeMemory_function_ = reinterpret_cast<WlanFreeMemoryFunction>(
      GetProcAddress(wlan_library, "WlanFreeMemory"));
  WlanCloseHandle_function_ = reinterpret_cast<WlanCloseHandleFunction>(
      GetProcAddress(wlan_library, "WlanCloseHandle"));
  DCHECK(WlanOpenHandle_function_ &&
         WlanEnumInterfaces_function_ &&
         WlanGetNetworkBssList_function_ &&
         WlanFreeMemory_function_ &&
         WlanCloseHandle_function_);
}

void WindowsWlanApi::LogWlanInterfaceCount(int count) {
  UMA_HISTOGRAM_CUSTOM_COUNTS(
      "Net.Wifi.InterfaceCount",
      count,
      1,
      5,
      6);
}

bool WindowsWlanApi::GetAccessPointData(
    WifiData::AccessPointDataSet* data) {
  DCHECK(data);

  // Get the handle to the WLAN API.
  DWORD negotiated_version;
  HANDLE wlan_handle = NULL;
  // We could be executing on either Windows XP or Windows Vista, so use the
  // lower version of the client WLAN API. It seems that the negotiated version
  // is the Vista version irrespective of what we pass!
  static const int kXpWlanClientVersion = 1;
  if ((*WlanOpenHandle_function_)(kXpWlanClientVersion,
                                  NULL,
                                  &negotiated_version,
                                  &wlan_handle) != ERROR_SUCCESS) {
    LogWlanInterfaceCount(0);
    return false;
  }
  DCHECK(wlan_handle);

  // Get the list of interfaces. WlanEnumInterfaces allocates interface_list.
  WLAN_INTERFACE_INFO_LIST* interface_list = NULL;
  if ((*WlanEnumInterfaces_function_)(wlan_handle, NULL, &interface_list) !=
      ERROR_SUCCESS) {
    LogWlanInterfaceCount(0);
    return false;
  }
  DCHECK(interface_list);

  LogWlanInterfaceCount(interface_list->dwNumberOfItems);

  // Go through the list of interfaces and get the data for each.
  for (int i = 0; i < static_cast<int>(interface_list->dwNumberOfItems); ++i) {
    // Skip any interface that is midway through association; the
    // WlanGetNetworkBssList function call is known to hang indefinitely
    // when it's in this state. http://crbug.com/39300
    if (interface_list->InterfaceInfo[i].isState ==
        wlan_interface_state_associating) {
      LOG(WARNING) << "Skipping wifi scan on adapter " << i << " ("
                   << interface_list->InterfaceInfo[i].strInterfaceDescription
                   << ") in 'associating' state. Repeated occurrences "
                      "indicates a non-responding adapter.";
      continue;
    }
    GetInterfaceDataWLAN(wlan_handle,
                         interface_list->InterfaceInfo[i].InterfaceGuid,
                         data);
  }

  // Free interface_list.
  (*WlanFreeMemory_function_)(interface_list);

  // Close the handle.
  if ((*WlanCloseHandle_function_)(wlan_handle, NULL) != ERROR_SUCCESS) {
    return false;
  }

  return true;
}

// Appends the data for a single interface to the data vector. Returns the
// number of access points found, or -1 on error.
int WindowsWlanApi::GetInterfaceDataWLAN(
    const HANDLE wlan_handle,
    const GUID& interface_id,
    WifiData::AccessPointDataSet* data) {
  DCHECK(data);

  const base::TimeTicks start_time = base::TimeTicks::Now();

  // WlanGetNetworkBssList allocates bss_list.
  WLAN_BSS_LIST* bss_list = NULL;
  if ((*WlanGetNetworkBssList_function_)(wlan_handle,
                                         &interface_id,
                                         NULL,   // Use all SSIDs.
                                         dot11_BSS_type_any,
                                         false,  // bSecurityEnabled - unused
                                         NULL,   // reserved
                                         &bss_list) != ERROR_SUCCESS) {
    return -1;
  }
  // According to http://www.attnetclient.com/kb/questions.php?questionid=75
  // WlanGetNetworkBssList can sometimes return success, but leave the bss
  // list as NULL.
  if (!bss_list)
    return -1;

  const base::TimeDelta duration = base::TimeTicks::Now() - start_time;

  UMA_HISTOGRAM_CUSTOM_TIMES(
      "Net.Wifi.ScanLatency",
      duration,
      base::TimeDelta::FromMilliseconds(1),
      base::TimeDelta::FromMinutes(1),
      100);


  int found = 0;
  for (int i = 0; i < static_cast<int>(bss_list->dwNumberOfItems); ++i) {
    AccessPointData access_point_data;
    if (GetNetworkData(bss_list->wlanBssEntries[i], &access_point_data)) {
      ++found;
      data->insert(access_point_data);
    }
  }

  (*WlanFreeMemory_function_)(bss_list);

  return found;
}

// WindowsNdisApi
WindowsNdisApi::WindowsNdisApi(
    std::vector<base::string16>* interface_service_names)
    : oid_buffer_size_(kInitialBufferSize) {
  DCHECK(!interface_service_names->empty());
  interface_service_names_.swap(*interface_service_names);
}

WindowsNdisApi::~WindowsNdisApi() {
}

WindowsNdisApi* WindowsNdisApi::Create() {
  std::vector<base::string16> interface_service_names;
  if (GetInterfacesNDIS(&interface_service_names)) {
    return new WindowsNdisApi(&interface_service_names);
  }
  return NULL;
}

bool WindowsNdisApi::GetAccessPointData(WifiData::AccessPointDataSet* data) {
  DCHECK(data);
  int interfaces_failed = 0;
  int interfaces_succeeded = 0;

  for (int i = 0; i < static_cast<int>(interface_service_names_.size()); ++i) {
    // First, check that we have a DOS device for this adapter.
    if (!DefineDosDeviceIfNotExists(interface_service_names_[i])) {
      continue;
    }

    // Get the handle to the device. This will fail if the named device is not
    // valid.
    HANDLE adapter_handle = GetFileHandle(interface_service_names_[i]);
    if (adapter_handle == INVALID_HANDLE_VALUE) {
      continue;
    }

    // Get the data.
    if (GetInterfaceDataNDIS(adapter_handle, data)) {
      ++interfaces_succeeded;
    } else {
      ++interfaces_failed;
    }

    // Clean up.
    CloseHandle(adapter_handle);
    UndefineDosDevice(interface_service_names_[i]);
  }

  // Return true if at least one interface succeeded, or at the very least none
  // failed.
  return interfaces_succeeded > 0 || interfaces_failed == 0;
}

bool WindowsNdisApi::GetInterfacesNDIS(
    std::vector<base::string16>* interface_service_names_out) {
  HKEY network_cards_key = NULL;
  if (RegOpenKeyEx(
      HKEY_LOCAL_MACHINE,
      L"Software\\Microsoft\\Windows NT\\CurrentVersion\\NetworkCards",
      0,
      KEY_READ,
      &network_cards_key) != ERROR_SUCCESS) {
    return false;
  }
  DCHECK(network_cards_key);

  for (int i = 0; ; ++i) {
    TCHAR name[kStringLength];
    DWORD name_size = kStringLength;
    FILETIME time;
    if (RegEnumKeyEx(network_cards_key,
                     i,
                     name,
                     &name_size,
                     NULL,
                     NULL,
                     NULL,
                     &time) != ERROR_SUCCESS) {
      break;
    }
    HKEY hardware_key = NULL;
    if (RegOpenKeyEx(network_cards_key, name, 0, KEY_READ, &hardware_key) !=
        ERROR_SUCCESS) {
      break;
    }
    DCHECK(hardware_key);

    TCHAR service_name[kStringLength];
    DWORD service_name_size = kStringLength;
    DWORD type = 0;
    if (RegQueryValueEx(hardware_key,
                        L"ServiceName",
                        NULL,
                        &type,
                        reinterpret_cast<LPBYTE>(service_name),
                        &service_name_size) == ERROR_SUCCESS) {
      interface_service_names_out->push_back(service_name);
    }
    RegCloseKey(hardware_key);
  }

  RegCloseKey(network_cards_key);
  return true;
}


bool WindowsNdisApi::GetInterfaceDataNDIS(HANDLE adapter_handle,
                                          WifiData::AccessPointDataSet* data) {
  DCHECK(data);

  std::unique_ptr<BYTE, base::FreeDeleter> buffer(
      static_cast<BYTE*>(malloc(oid_buffer_size_)));
  if (buffer == NULL) {
    return false;
  }

  DWORD bytes_out;
  int result;

  while (true) {
    bytes_out = 0;
    result = PerformQuery(adapter_handle, buffer.get(),
                          oid_buffer_size_, &bytes_out);
    if (result == ERROR_GEN_FAILURE ||  // Returned by some Intel cards.
        result == ERROR_INSUFFICIENT_BUFFER ||
        result == ERROR_MORE_DATA ||
        result == NDIS_STATUS_INVALID_LENGTH ||
        result == NDIS_STATUS_BUFFER_TOO_SHORT) {
      // The buffer we supplied is too small, so increase it. bytes_out should
      // provide the required buffer size, but this is not always the case.
      if (bytes_out > static_cast<DWORD>(oid_buffer_size_)) {
        oid_buffer_size_ = bytes_out;
      } else {
        oid_buffer_size_ *= 2;
      }
      if (!ResizeBuffer(oid_buffer_size_, &buffer)) {
        oid_buffer_size_ = kInitialBufferSize;  // Reset for next time.
        return false;
      }
    } else {
      // The buffer is not too small.
      break;
    }
  }
  DCHECK(buffer.get());

  if (result == ERROR_SUCCESS) {
    NDIS_802_11_BSSID_LIST* bssid_list =
        reinterpret_cast<NDIS_802_11_BSSID_LIST*>(buffer.get());
    GetDataFromBssIdList(*bssid_list, oid_buffer_size_, data);
  }

  return true;
}

bool GetNetworkData(const WLAN_BSS_ENTRY& bss_entry,
                    AccessPointData* access_point_data) {
  // Currently we get only MAC address, signal strength and SSID.
  DCHECK(access_point_data);
  access_point_data->mac_address = MacAddressAsString16(bss_entry.dot11Bssid);
  access_point_data->radio_signal_strength = bss_entry.lRssi;
  // bss_entry.dot11Ssid.ucSSID is not null-terminated.
  base::UTF8ToUTF16(reinterpret_cast<const char*>(bss_entry.dot11Ssid.ucSSID),
                    static_cast<ULONG>(bss_entry.dot11Ssid.uSSIDLength),
                    &access_point_data->ssid);
  // TODO(steveblock): Is it possible to get the following?
  // access_point_data->signal_to_noise
  // access_point_data->age
  // access_point_data->channel
  return true;
}

bool UndefineDosDevice(const base::string16& device_name) {
  // We remove only the mapping we use, that is \Device\<device_name>.
  base::string16 target_path = L"\\Device\\" + device_name;
  return DefineDosDevice(
      DDD_RAW_TARGET_PATH | DDD_REMOVE_DEFINITION | DDD_EXACT_MATCH_ON_REMOVE,
      device_name.c_str(),
      target_path.c_str()) == TRUE;
}

bool DefineDosDeviceIfNotExists(const base::string16& device_name) {
  // We create a DOS device name for the device at \Device\<device_name>.
  base::string16 target_path = L"\\Device\\" + device_name;

  TCHAR target[kStringLength];
  if (QueryDosDevice(device_name.c_str(), target, kStringLength) > 0 &&
      target_path.compare(target) == 0) {
    // Device already exists.
    return true;
  }

  if (GetLastError() != ERROR_FILE_NOT_FOUND) {
    return false;
  }

  if (!DefineDosDevice(DDD_RAW_TARGET_PATH,
                       device_name.c_str(),
                       target_path.c_str())) {
    return false;
  }

  // Check that the device is really there.
  return QueryDosDevice(device_name.c_str(), target, kStringLength) > 0 &&
      target_path.compare(target) == 0;
}

HANDLE GetFileHandle(const base::string16& device_name) {
  // We access a device with DOS path \Device\<device_name> at
  // \\.\<device_name>.
  base::string16 formatted_device_name = L"\\\\.\\" + device_name;

  return CreateFile(formatted_device_name.c_str(),
                    GENERIC_READ,
                    FILE_SHARE_READ | FILE_SHARE_WRITE,  // share mode
                    0,  // security attributes
                    OPEN_EXISTING,
                    0,  // flags and attributes
                    INVALID_HANDLE_VALUE);
}

int PerformQuery(HANDLE adapter_handle,
                 BYTE* buffer,
                 DWORD buffer_size,
                 DWORD* bytes_out) {
  DWORD oid = OID_802_11_BSSID_LIST;
  if (!DeviceIoControl(adapter_handle,
                       IOCTL_NDIS_QUERY_GLOBAL_STATS,
                       &oid,
                       sizeof(oid),
                       buffer,
                       buffer_size,
                       bytes_out,
                       NULL)) {
    return GetLastError();
  }
  return ERROR_SUCCESS;
}

bool ResizeBuffer(int requested_size,
                  std::unique_ptr<BYTE, base::FreeDeleter>* buffer) {
  DCHECK_GT(requested_size, 0);
  DCHECK(buffer);
  if (requested_size > kMaximumBufferSize) {
    buffer->reset();
    return false;
  }

  buffer->reset(reinterpret_cast<BYTE*>(
      realloc(buffer->release(), requested_size)));
  return buffer != NULL;
}

bool GetSystemDirectory(base::string16* path) {
  DCHECK(path);
  // Return value includes terminating NULL.
  int buffer_size = ::GetSystemDirectory(NULL, 0);
  if (buffer_size == 0) {
    return false;
  }
  std::unique_ptr<base::char16[]> buffer(new base::char16[buffer_size]);

  // Return value excludes terminating NULL.
  int characters_written = ::GetSystemDirectory(buffer.get(), buffer_size);
  if (characters_written == 0) {
    return false;
  }
  DCHECK_EQ(buffer_size - 1, characters_written);

  path->assign(buffer.get(), characters_written);

  if (*path->rbegin() != L'\\') {
    path->append(L"\\");
  }
  DCHECK_EQ(L'\\', *path->rbegin());
  return true;
}
}  // namespace

}  // namespace content
