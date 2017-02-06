// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu/gpu_internals_ui.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/environment.h"
#include "base/i18n/time_formatting.h"
#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/sys_info.h"
#include "base/values.h"
#include "build/build_config.h"
#include "content/browser/gpu/browser_gpu_memory_buffer_manager.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/grit/content_resources.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/gpu_data_manager_observer.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "gpu/config/gpu_feature_type.h"
#include "gpu/config/gpu_info.h"
#include "third_party/angle/src/common/version.h"
#include "ui/gl/gpu_switching_manager.h"

#if defined(OS_LINUX) && defined(USE_X11)
#include <X11/Xlib.h>
#endif
#if defined(OS_WIN)
#include "ui/base/win/shell.h"
#include "ui/gfx/win/physical_size.h"
#endif

#if defined(OS_LINUX) && defined(USE_X11)
#include "ui/base/x/x11_util.h"       // nogncheck
#include "ui/gfx/x/x11_atom_cache.h"  // nogncheck
#endif

namespace content {
namespace {

WebUIDataSource* CreateGpuHTMLSource() {
  WebUIDataSource* source = WebUIDataSource::Create(kChromeUIGpuHost);

  source->SetJsonPath("strings.js");
  source->AddResourcePath("gpu_internals.js", IDR_GPU_INTERNALS_JS);
  source->SetDefaultResource(IDR_GPU_INTERNALS_HTML);
  return source;
}

base::DictionaryValue* NewDescriptionValuePair(const std::string& desc,
    const std::string& value) {
  base::DictionaryValue* dict = new base::DictionaryValue();
  dict->SetString("description", desc);
  dict->SetString("value", value);
  return dict;
}

base::DictionaryValue* NewDescriptionValuePair(const std::string& desc,
    base::Value* value) {
  base::DictionaryValue* dict = new base::DictionaryValue();
  dict->SetString("description", desc);
  dict->Set("value", value);
  return dict;
}

#if defined(OS_WIN)
// Output DxDiagNode tree as nested array of {description,value} pairs
base::ListValue* DxDiagNodeToList(const gpu::DxDiagNode& node) {
  base::ListValue* list = new base::ListValue();
  for (std::map<std::string, std::string>::const_iterator it =
      node.values.begin();
      it != node.values.end();
      ++it) {
    list->Append(NewDescriptionValuePair(it->first, it->second));
  }

  for (std::map<std::string, gpu::DxDiagNode>::const_iterator it =
      node.children.begin();
      it != node.children.end();
      ++it) {
    base::ListValue* sublist = DxDiagNodeToList(it->second);
    list->Append(NewDescriptionValuePair(it->first, sublist));
  }
  return list;
}
#endif

std::string GPUDeviceToString(const gpu::GPUInfo::GPUDevice& gpu) {
  std::string vendor = base::StringPrintf("0x%04x", gpu.vendor_id);
  if (!gpu.vendor_string.empty())
    vendor += " [" + gpu.vendor_string + "]";
  std::string device = base::StringPrintf("0x%04x", gpu.device_id);
  if (!gpu.device_string.empty())
    device += " [" + gpu.device_string + "]";
  return base::StringPrintf("VENDOR = %s, DEVICE= %s%s",
      vendor.c_str(), device.c_str(), gpu.active ? " *ACTIVE*" : "");
}

base::DictionaryValue* GpuInfoAsDictionaryValue() {
  gpu::GPUInfo gpu_info = GpuDataManagerImpl::GetInstance()->GetGPUInfo();
  base::ListValue* basic_info = new base::ListValue();
  basic_info->Append(NewDescriptionValuePair(
      "Initialization time",
      base::Int64ToString(gpu_info.initialization_time.InMilliseconds())));
  basic_info->Append(NewDescriptionValuePair(
      "In-process GPU", new base::FundamentalValue(gpu_info.in_process_gpu)));
  basic_info->Append(NewDescriptionValuePair(
      "Sandboxed", new base::FundamentalValue(gpu_info.sandboxed)));
  basic_info->Append(NewDescriptionValuePair(
      "GPU0", GPUDeviceToString(gpu_info.gpu)));
  for (size_t i = 0; i < gpu_info.secondary_gpus.size(); ++i) {
    basic_info->Append(NewDescriptionValuePair(
        base::StringPrintf("GPU%d", static_cast<int>(i + 1)),
        GPUDeviceToString(gpu_info.secondary_gpus[i])));
  }
  basic_info->Append(NewDescriptionValuePair(
      "Optimus", new base::FundamentalValue(gpu_info.optimus)));
  basic_info->Append(NewDescriptionValuePair(
      "AMD switchable", new base::FundamentalValue(gpu_info.amd_switchable)));
  if (gpu_info.lenovo_dcute) {
    basic_info->Append(NewDescriptionValuePair(
        "Lenovo dCute", new base::FundamentalValue(true)));
  }
  if (gpu_info.display_link_version.IsValid()) {
    basic_info->Append(NewDescriptionValuePair(
        "DisplayLink Version", gpu_info.display_link_version.GetString()));
  }
#if defined(OS_WIN)
  std::string compositor =
      ui::win::IsAeroGlassEnabled() ? "Aero Glass" : "none";
  basic_info->Append(
      NewDescriptionValuePair("Desktop compositing", compositor));

  std::vector<gfx::PhysicalDisplaySize> display_sizes =
      gfx::GetPhysicalSizeForDisplays();
  for (const auto& display_size : display_sizes) {
    const int w = display_size.width_mm;
    const int h = display_size.height_mm;
    const double size_mm = sqrt(w * w + h * h);
    const double size_inches = 0.0393701 * size_mm;
    const double rounded_size_inches = floor(10.0 * size_inches) / 10.0;
    std::string size_string = base::StringPrintf("%.1f\"", rounded_size_inches);
    std::string description_string = base::StringPrintf(
        "Diagonal Monitor Size of %s", display_size.display_name.c_str());
    basic_info->Append(
        NewDescriptionValuePair(description_string, size_string));
  }
#endif

  std::string disabled_extensions;
  GpuDataManagerImpl::GetInstance()->GetDisabledExtensions(
      &disabled_extensions);

  basic_info->Append(
      NewDescriptionValuePair("Driver vendor", gpu_info.driver_vendor));
  basic_info->Append(NewDescriptionValuePair("Driver version",
                                             gpu_info.driver_version));
  basic_info->Append(NewDescriptionValuePair("Driver date",
                                             gpu_info.driver_date));
  basic_info->Append(NewDescriptionValuePair("Pixel shader version",
                                             gpu_info.pixel_shader_version));
  basic_info->Append(NewDescriptionValuePair("Vertex shader version",
                                             gpu_info.vertex_shader_version));
  basic_info->Append(NewDescriptionValuePair("Max. MSAA samples",
                                             gpu_info.max_msaa_samples));
  basic_info->Append(NewDescriptionValuePair("Machine model name",
                                             gpu_info.machine_model_name));
  basic_info->Append(NewDescriptionValuePair("Machine model version",
                                             gpu_info.machine_model_version));
  basic_info->Append(NewDescriptionValuePair("GL_VENDOR",
                                             gpu_info.gl_vendor));
  basic_info->Append(NewDescriptionValuePair("GL_RENDERER",
                                             gpu_info.gl_renderer));
  basic_info->Append(NewDescriptionValuePair("GL_VERSION",
                                             gpu_info.gl_version));
  basic_info->Append(NewDescriptionValuePair("GL_EXTENSIONS",
                                             gpu_info.gl_extensions));
  basic_info->Append(NewDescriptionValuePair("Disabled Extensions",
                                             disabled_extensions));
  basic_info->Append(NewDescriptionValuePair("Window system binding vendor",
                                             gpu_info.gl_ws_vendor));
  basic_info->Append(NewDescriptionValuePair("Window system binding version",
                                             gpu_info.gl_ws_version));
  basic_info->Append(NewDescriptionValuePair("Window system binding extensions",
                                             gpu_info.gl_ws_extensions));
#if defined(OS_LINUX) && defined(USE_X11)
  basic_info->Append(NewDescriptionValuePair("Window manager",
                                             ui::GuessWindowManagerName()));
  {
    std::unique_ptr<base::Environment> env(base::Environment::Create());
    std::string value;
    const char kXDGCurrentDesktop[] = "XDG_CURRENT_DESKTOP";
    if (env->GetVar(kXDGCurrentDesktop, &value))
      basic_info->Append(NewDescriptionValuePair(kXDGCurrentDesktop, value));
    const char kGDMSession[] = "GDMSESSION";
    if (env->GetVar(kGDMSession, &value))
      basic_info->Append(NewDescriptionValuePair(kGDMSession, value));
    const char* const kAtomsToCache[] = {
        "_NET_WM_CM_S0",
        NULL
    };
    ui::X11AtomCache atom_cache(gfx::GetXDisplay(), kAtomsToCache);
    std::string compositing_manager = XGetSelectionOwner(
        gfx::GetXDisplay(),
        atom_cache.GetAtom("_NET_WM_CM_S0")) != None ? "Yes" : "No";
    basic_info->Append(
        NewDescriptionValuePair("Compositing manager", compositing_manager));
  }
#endif
  std::string direct_rendering = gpu_info.direct_rendering ? "Yes" : "No";
  basic_info->Append(
      NewDescriptionValuePair("Direct rendering", direct_rendering));

  std::string reset_strategy =
      base::StringPrintf("0x%04x", gpu_info.gl_reset_notification_strategy);
  basic_info->Append(NewDescriptionValuePair(
      "Reset notification strategy", reset_strategy));

  basic_info->Append(NewDescriptionValuePair(
      "GPU process crash count",
      new base::FundamentalValue(gpu_info.process_crash_count)));

  base::DictionaryValue* info = new base::DictionaryValue();
  info->Set("basic_info", basic_info);

#if defined(OS_WIN)
  std::unique_ptr<base::Value> dx_info = base::Value::CreateNullValue();
  if (gpu_info.dx_diagnostics.children.size())
    dx_info.reset(DxDiagNodeToList(gpu_info.dx_diagnostics));
  info->Set("diagnostics", std::move(dx_info));
#endif

  return info;
}

const char* BufferFormatToString(gfx::BufferFormat format) {
  switch (format) {
    case gfx::BufferFormat::ATC:
      return "ATC";
    case gfx::BufferFormat::ATCIA:
      return "ATCIA";
    case gfx::BufferFormat::DXT1:
      return "DXT1";
    case gfx::BufferFormat::DXT5:
      return "DXT5";
    case gfx::BufferFormat::ETC1:
      return "ETC1";
    case gfx::BufferFormat::R_8:
      return "R_8";
    case gfx::BufferFormat::BGR_565:
      return "BGR_565";
    case gfx::BufferFormat::RGBA_4444:
      return "RGBA_4444";
    case gfx::BufferFormat::RGBX_8888:
      return "RGBX_8888";
    case gfx::BufferFormat::RGBA_8888:
      return "RGBA_8888";
    case gfx::BufferFormat::BGRX_8888:
      return "BGRX_8888";
    case gfx::BufferFormat::BGRA_8888:
      return "BGRA_8888";
    case gfx::BufferFormat::YVU_420:
      return "YVU_420";
    case gfx::BufferFormat::YUV_420_BIPLANAR:
      return "YUV_420_BIPLANAR";
    case gfx::BufferFormat::UYVY_422:
      return "UYVY_422";
  }
  NOTREACHED();
  return nullptr;
}

const char* BufferUsageToString(gfx::BufferUsage usage) {
  switch (usage) {
    case gfx::BufferUsage::GPU_READ:
      return "GPU_READ";
    case gfx::BufferUsage::SCANOUT:
      return "SCANOUT";
    case gfx::BufferUsage::GPU_READ_CPU_READ_WRITE:
      return "GPU_READ_CPU_READ_WRITE";
    case gfx::BufferUsage::GPU_READ_CPU_READ_WRITE_PERSISTENT:
      return "GPU_READ_CPU_READ_WRITE_PERSISTENT";
  }
  NOTREACHED();
  return nullptr;
}

base::ListValue* CompositorInfo() {
  base::ListValue* compositor_info = new base::ListValue();

  compositor_info->Append(NewDescriptionValuePair(
      "Tile Update Mode",
      IsZeroCopyUploadEnabled() ? "Zero-copy" : "One-copy"));

  compositor_info->Append(NewDescriptionValuePair(
      "Partial Raster", IsPartialRasterEnabled() ? "Enabled" : "Disabled"));
  return compositor_info;
}

base::ListValue* GpuMemoryBufferInfo() {
  base::ListValue* gpu_memory_buffer_info = new base::ListValue();

  BrowserGpuMemoryBufferManager* gpu_memory_buffer_manager =
      BrowserGpuMemoryBufferManager::current();

  for (size_t format = 0;
       format < static_cast<size_t>(gfx::BufferFormat::LAST) + 1; format++) {
    std::string native_usage_support;
    for (size_t usage = 0;
         usage < static_cast<size_t>(gfx::BufferUsage::LAST) + 1; usage++) {
      if (gpu_memory_buffer_manager->IsNativeGpuMemoryBufferConfiguration(
              static_cast<gfx::BufferFormat>(format),
              static_cast<gfx::BufferUsage>(usage)))
        native_usage_support = base::StringPrintf(
            "%s%s %s", native_usage_support.c_str(),
            native_usage_support.empty() ? "" : ",",
            BufferUsageToString(static_cast<gfx::BufferUsage>(usage)));
    }
    if (native_usage_support.empty())
      native_usage_support = base::StringPrintf("Software only");

    gpu_memory_buffer_info->Append(NewDescriptionValuePair(
        BufferFormatToString(static_cast<gfx::BufferFormat>(format)),
        native_usage_support));
  }
  return gpu_memory_buffer_info;
}

// This class receives javascript messages from the renderer.
// Note that the WebUI infrastructure runs on the UI thread, therefore all of
// this class's methods are expected to run on the UI thread.
class GpuMessageHandler
    : public WebUIMessageHandler,
      public base::SupportsWeakPtr<GpuMessageHandler>,
      public GpuDataManagerObserver,
      public ui::GpuSwitchingObserver {
 public:
  GpuMessageHandler();
  ~GpuMessageHandler() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

  // GpuDataManagerObserver implementation.
  void OnGpuInfoUpdate() override;

  // ui::GpuSwitchingObserver implementation.
  void OnGpuSwitched() override;

  // Messages
  void OnBrowserBridgeInitialized(const base::ListValue* list);
  void OnCallAsync(const base::ListValue* list);

  // Submessages dispatched from OnCallAsync
  base::Value* OnRequestClientInfo(const base::ListValue* list);
  base::Value* OnRequestLogMessages(const base::ListValue* list);

 private:
  // True if observing the GpuDataManager (re-attaching as observer would
  // DCHECK).
  bool observing_;

  DISALLOW_COPY_AND_ASSIGN(GpuMessageHandler);
};

////////////////////////////////////////////////////////////////////////////////
//
// GpuMessageHandler
//
////////////////////////////////////////////////////////////////////////////////

GpuMessageHandler::GpuMessageHandler()
    : observing_(false) {
}

GpuMessageHandler::~GpuMessageHandler() {
  ui::GpuSwitchingManager::GetInstance()->RemoveObserver(this);
  GpuDataManagerImpl::GetInstance()->RemoveObserver(this);
}

/* BrowserBridge.callAsync prepends a requestID to these messages. */
void GpuMessageHandler::RegisterMessages() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  web_ui()->RegisterMessageCallback("browserBridgeInitialized",
      base::Bind(&GpuMessageHandler::OnBrowserBridgeInitialized,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("callAsync",
      base::Bind(&GpuMessageHandler::OnCallAsync,
                 base::Unretained(this)));
}

void GpuMessageHandler::OnCallAsync(const base::ListValue* args) {
  DCHECK_GE(args->GetSize(), static_cast<size_t>(2));
  // unpack args into requestId, submessage and submessageArgs
  bool ok;
  const base::Value* requestId;
  ok = args->Get(0, &requestId);
  DCHECK(ok);

  std::string submessage;
  ok = args->GetString(1, &submessage);
  DCHECK(ok);

  base::ListValue* submessageArgs = new base::ListValue();
  for (size_t i = 2; i < args->GetSize(); ++i) {
    const base::Value* arg;
    ok = args->Get(i, &arg);
    DCHECK(ok);

    base::Value* argCopy = arg->DeepCopy();
    submessageArgs->Append(argCopy);
  }

  // call the submessage handler
  base::Value* ret = NULL;
  if (submessage == "requestClientInfo") {
    ret = OnRequestClientInfo(submessageArgs);
  } else if (submessage == "requestLogMessages") {
    ret = OnRequestLogMessages(submessageArgs);
  } else {  // unrecognized submessage
    NOTREACHED();
    delete submessageArgs;
    return;
  }
  delete submessageArgs;

  // call BrowserBridge.onCallAsyncReply with result
  if (ret) {
    web_ui()->CallJavascriptFunctionUnsafe("browserBridge.onCallAsyncReply",
                                           *requestId, *ret);
    delete ret;
  } else {
    web_ui()->CallJavascriptFunctionUnsafe("browserBridge.onCallAsyncReply",
                                           *requestId);
  }
}

void GpuMessageHandler::OnBrowserBridgeInitialized(
    const base::ListValue* args) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Watch for changes in GPUInfo
  if (!observing_) {
    GpuDataManagerImpl::GetInstance()->AddObserver(this);
    ui::GpuSwitchingManager::GetInstance()->AddObserver(this);
  }
  observing_ = true;

  // Tell GpuDataManager it should have full GpuInfo. If the
  // Gpu process has not run yet, this will trigger its launch.
  GpuDataManagerImpl::GetInstance()->RequestCompleteGpuInfoIfNeeded();

  // Run callback immediately in case the info is ready and no update in the
  // future.
  OnGpuInfoUpdate();
}

base::Value* GpuMessageHandler::OnRequestClientInfo(
    const base::ListValue* list) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  base::DictionaryValue* dict = new base::DictionaryValue();

  dict->SetString("version", GetContentClient()->GetProduct());
  dict->SetString("command_line",
      base::CommandLine::ForCurrentProcess()->GetCommandLineString());
  dict->SetString("operating_system",
                  base::SysInfo::OperatingSystemName() + " " +
                  base::SysInfo::OperatingSystemVersion());
  dict->SetString("angle_commit_id", ANGLE_COMMIT_HASH);
  dict->SetString("graphics_backend", "Skia");
  dict->SetString("blacklist_version",
      GpuDataManagerImpl::GetInstance()->GetBlacklistVersion());
  dict->SetString("driver_bug_list_version",
      GpuDataManagerImpl::GetInstance()->GetDriverBugListVersion());

  return dict;
}

base::Value* GpuMessageHandler::OnRequestLogMessages(const base::ListValue*) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  return GpuDataManagerImpl::GetInstance()->GetLogMessages();
}

void GpuMessageHandler::OnGpuInfoUpdate() {
  // Get GPU Info.
  std::unique_ptr<base::DictionaryValue> gpu_info_val(
      GpuInfoAsDictionaryValue());

  // Add in blacklisting features
  base::DictionaryValue* feature_status = new base::DictionaryValue;
  feature_status->Set("featureStatus", GetFeatureStatus());
  feature_status->Set("problems", GetProblems());
  base::ListValue* workarounds = new base::ListValue();
  for (const std::string& workaround : GetDriverBugWorkarounds())
    workarounds->AppendString(workaround);
  feature_status->Set("workarounds", workarounds);
  gpu_info_val->Set("featureStatus", feature_status);
  gpu_info_val->Set("compositorInfo", CompositorInfo());
  gpu_info_val->Set("gpuMemoryBufferInfo", GpuMemoryBufferInfo());

  // Send GPU Info to javascript.
  web_ui()->CallJavascriptFunctionUnsafe("browserBridge.onGpuInfoUpdate",
                                         *(gpu_info_val.get()));
}

void GpuMessageHandler::OnGpuSwitched() {
  GpuDataManagerImpl::GetInstance()->RequestCompleteGpuInfoIfNeeded();
}

}  // namespace


////////////////////////////////////////////////////////////////////////////////
//
// GpuInternalsUI
//
////////////////////////////////////////////////////////////////////////////////

GpuInternalsUI::GpuInternalsUI(WebUI* web_ui)
    : WebUIController(web_ui) {
  web_ui->AddMessageHandler(new GpuMessageHandler());

  // Set up the chrome://gpu/ source.
  BrowserContext* browser_context =
      web_ui->GetWebContents()->GetBrowserContext();
  WebUIDataSource::Add(browser_context, CreateGpuHTMLSource());
}

}  // namespace content
