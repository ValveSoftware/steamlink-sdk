// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/config/gpu_info_collector_linux.h"

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/trace_event/trace_event.h"
#include "gpu/config/gpu_info_collector.h"
#include "gpu/config/gpu_switches.h"
#include "third_party/re2/src/re2/re2.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_switches.h"

#if defined(USE_LIBPCI)
#include "library_loaders/libpci.h"  // nogncheck
#endif

namespace gpu {

namespace {

#if defined(USE_LIBPCI)
// This checks if a system supports PCI bus.
// We check the existence of /sys/bus/pci or /sys/bug/pci_express.
bool IsPciSupported() {
  const base::FilePath pci_path("/sys/bus/pci/");
  const base::FilePath pcie_path("/sys/bus/pci_express/");
  return (base::PathExists(pci_path) ||
          base::PathExists(pcie_path));
}
#endif  // defined(USE_LIBPCI)

// Scan /sys/module/amdgpu/version.
// Return empty string on failing.
std::string CollectDriverVersionAMDBrahma() {
  const base::FilePath ati_file_path("/sys/module/amdgpu/version");
  if (!base::PathExists(ati_file_path))
    return std::string();
  std::string contents;
  if (!base::ReadFileToString(ati_file_path, &contents))
    return std::string();
  size_t begin = contents.find_first_of("0123456789");
  if (begin != std::string::npos) {
    size_t end = contents.find_first_not_of("0123456789.", begin);
    if (end == std::string::npos)
      return contents.substr(begin);
    else
      return contents.substr(begin, end - begin);
  }
  return std::string();
}

// Scan /etc/ati/amdpcsdb.default for "ReleaseVersion".
// Return empty string on failing.
std::string CollectDriverVersionAMDCatalyst() {
  const base::FilePath ati_file_path("/etc/ati/amdpcsdb.default");
  if (!base::PathExists(ati_file_path))
    return std::string();
  std::string contents;
  if (!base::ReadFileToString(ati_file_path, &contents))
    return std::string();
  base::StringTokenizer t(contents, "\r\n");
  while (t.GetNext()) {
    std::string line = t.token();
    if (base::StartsWith(line, "ReleaseVersion=",
                         base::CompareCase::SENSITIVE)) {
      size_t begin = line.find_first_of("0123456789");
      if (begin != std::string::npos) {
        size_t end = line.find_first_not_of("0123456789.", begin);
        if (end == std::string::npos)
          return line.substr(begin);
        else
          return line.substr(begin, end - begin);
      }
    }
  }
  return std::string();
}

const uint32_t kVendorIDIntel = 0x8086;
const uint32_t kVendorIDNVidia = 0x10de;
const uint32_t kVendorIDAMD = 0x1002;

CollectInfoResult CollectPCIVideoCardInfo(GPUInfo* gpu_info) {
  DCHECK(gpu_info);

#if !defined(USE_LIBPCI)
  return kCollectInfoNonFatalFailure;
#else

  if (IsPciSupported() == false) {
    VLOG(1) << "PCI bus scanning is not supported";
    return kCollectInfoNonFatalFailure;
  }

  // TODO(zmo): be more flexible about library name.
  LibPciLoader libpci_loader;
  if (!libpci_loader.Load("libpci.so.3") &&
      !libpci_loader.Load("libpci.so")) {
    VLOG(1) << "Failed to locate libpci";
    return kCollectInfoNonFatalFailure;
  }

  pci_access* access = (libpci_loader.pci_alloc)();
  DCHECK(access != NULL);
  (libpci_loader.pci_init)(access);
  (libpci_loader.pci_scan_bus)(access);
  bool primary_gpu_identified = false;
  for (pci_dev* device = access->devices;
       device != NULL; device = device->next) {
    // Fill the IDs and class fields.
    (libpci_loader.pci_fill_info)(device, 33);
    bool is_gpu = false;
    switch (device->device_class) {
      case PCI_CLASS_DISPLAY_VGA:
      case PCI_CLASS_DISPLAY_XGA:
      case PCI_CLASS_DISPLAY_3D:
        is_gpu = true;
        break;
      case PCI_CLASS_DISPLAY_OTHER:
      default:
        break;
    }
    if (!is_gpu)
      continue;
    if (device->vendor_id == 0 || device->device_id == 0)
      continue;

    GPUInfo::GPUDevice gpu;
    gpu.vendor_id = device->vendor_id;
    gpu.device_id = device->device_id;

    if (!primary_gpu_identified) {
      primary_gpu_identified = true;
      gpu_info->gpu = gpu;
    } else {
      // TODO(zmo): if there are multiple GPUs, we assume the non Intel
      // one is primary. Revisit this logic because we actually don't know
      // which GPU we are using at this point.
      if (gpu_info->gpu.vendor_id == kVendorIDIntel &&
          gpu.vendor_id != kVendorIDIntel) {
        gpu_info->secondary_gpus.push_back(gpu_info->gpu);
        gpu_info->gpu = gpu;
      } else {
        gpu_info->secondary_gpus.push_back(gpu);
      }
    }
  }

  // Detect Optimus or AMD Switchable GPU.
  if (gpu_info->secondary_gpus.size() == 1 &&
      gpu_info->secondary_gpus[0].vendor_id == kVendorIDIntel) {
    if (gpu_info->gpu.vendor_id == kVendorIDNVidia)
      gpu_info->optimus = true;
    if (gpu_info->gpu.vendor_id == kVendorIDAMD)
      gpu_info->amd_switchable = true;
  }

  (libpci_loader.pci_cleanup)(access);
  if (!primary_gpu_identified)
    return kCollectInfoNonFatalFailure;
  return kCollectInfoSuccess;
#endif
}

}  // namespace anonymous

CollectInfoResult CollectContextGraphicsInfo(GPUInfo* gpu_info) {
  DCHECK(gpu_info);

  TRACE_EVENT0("gpu", "gpu_info_collector::CollectGraphicsInfo");

  CollectInfoResult result = CollectGraphicsInfoGL(gpu_info);
  gpu_info->context_info_state = result;
  return result;
}

CollectInfoResult CollectGpuID(uint32_t* vendor_id, uint32_t* device_id) {
  DCHECK(vendor_id && device_id);
  *vendor_id = 0;
  *device_id = 0;

  GPUInfo gpu_info;
  CollectInfoResult result = CollectPCIVideoCardInfo(&gpu_info);
  if (result == kCollectInfoSuccess) {
    *vendor_id = gpu_info.gpu.vendor_id;
    *device_id = gpu_info.gpu.device_id;
  }
  return result;
}

CollectInfoResult CollectBasicGraphicsInfo(GPUInfo* gpu_info) {
  DCHECK(gpu_info);

  CollectInfoResult result = CollectPCIVideoCardInfo(gpu_info);

  std::string driver_version;
  switch (gpu_info->gpu.vendor_id) {
    case kVendorIDAMD:
      driver_version = CollectDriverVersionAMDBrahma();
      if (!driver_version.empty()) {
        gpu_info->driver_vendor = "ATI / AMD (Brahma)";
        gpu_info->driver_version = driver_version;
      } else {
        driver_version = CollectDriverVersionAMDCatalyst();
        if (!driver_version.empty()) {
          gpu_info->driver_vendor = "ATI / AMD (Catalyst)";
          gpu_info->driver_version = driver_version;
        }
      }
      break;
    case kVendorIDNVidia:
      driver_version = CollectDriverVersionNVidia();
      if (!driver_version.empty()) {
        gpu_info->driver_vendor = "NVIDIA";
        gpu_info->driver_version = driver_version;
      }
      break;
    case kVendorIDIntel:
      // In dual-GPU cases, sometimes PCI scan only gives us the
      // integrated GPU (i.e., the Intel one).
      if (gpu_info->secondary_gpus.size() == 0) {
        driver_version = CollectDriverVersionNVidia();
        if (!driver_version.empty()) {
          gpu_info->driver_vendor = "NVIDIA";
          gpu_info->driver_version = driver_version;
          gpu_info->optimus = true;
          // Put Intel to the secondary GPU list.
          gpu_info->secondary_gpus.push_back(gpu_info->gpu);
          // Put NVIDIA as the primary GPU.
          gpu_info->gpu.vendor_id = kVendorIDNVidia;
          gpu_info->gpu.device_id = 0;  // Unknown Device.
        }
      }
      break;
  }

  gpu_info->basic_info_state = result;
  return result;
}

CollectInfoResult CollectDriverInfoGL(GPUInfo* gpu_info) {
  DCHECK(gpu_info);

  // Driver vendor and version are always expected to be extracted from the
  // testing gl version.
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kGpuTestingGLVersion) &&
      !gpu_info->driver_vendor.empty() && !gpu_info->driver_version.empty()) {
    return kCollectInfoSuccess;
  }

  std::string gl_version = gpu_info->gl_version;
  std::vector<std::string> pieces = base::SplitString(
      gl_version, base::kWhitespaceASCII, base::KEEP_WHITESPACE,
      base::SPLIT_WANT_NONEMPTY);
  // In linux, the gl version string might be in the format of
  //   GLVersion DriverVendor DriverVersion
  if (pieces.size() < 3)
    return kCollectInfoNonFatalFailure;

  // Search from the end for the first piece that starts with major.minor or
  // major.minor.micro but assume the driver version cannot be in the first two
  // pieces.
  re2::RE2 pattern("([\\d]+\\.[\\d]+(\\.[\\d]+)?).*");
  std::string driver_version;
  auto it = pieces.rbegin();
  while (pieces.rend() - it > 2) {
    bool parsed = re2::RE2::FullMatch(*it, pattern, &driver_version);
    if (parsed)
      break;
    ++it;
  }

  if (driver_version.empty())
    return kCollectInfoNonFatalFailure;

  gpu_info->driver_vendor = *(++it);
  gpu_info->driver_version = driver_version;
  return kCollectInfoSuccess;
}

void MergeGPUInfo(GPUInfo* basic_gpu_info,
                  const GPUInfo& context_gpu_info) {
  MergeGPUInfoGL(basic_gpu_info, context_gpu_info);
}

}  // namespace gpu
