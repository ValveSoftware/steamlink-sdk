// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Represents the browser side of the browser <--> renderer communication
// channel. There will be one RenderProcessHost per renderer process.

#include "content/browser/renderer_host/render_process_host_impl.h"

#include <algorithm>
#include <limits>
#include <utility>
#include <vector>

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/files/file.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/shared_memory.h"
#include "base/memory/shared_memory_handle.h"
#include "base/metrics/histogram.h"
#include "base/metrics/persistent_histogram_allocator.h"
#include "base/metrics/persistent_memory_allocator.h"
#include "base/process/process_handle.h"
#include "base/rand_util.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/supports_user_data.h"
#include "base/sys_info.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "base/tracked_objects.h"
#include "build/build_config.h"
#include "cc/base/switches.h"
#include "components/scheduler/common/scheduler_switches.h"
#include "components/tracing/common/tracing_switches.h"
#include "components/webmessaging/broadcast_channel_provider.h"
#include "content/browser/appcache/appcache_dispatcher_host.h"
#include "content/browser/appcache/chrome_appcache_service.h"
#include "content/browser/background_sync/background_sync_service_impl.h"
#include "content/browser/bad_message.h"
#include "content/browser/blob_storage/blob_dispatcher_host.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/browser_child_process_host_impl.h"
#include "content/browser/browser_main.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/browser_plugin/browser_plugin_message_filter.h"
#include "content/browser/cache_storage/cache_storage_context_impl.h"
#include "content/browser/cache_storage/cache_storage_dispatcher_host.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/device_sensors/device_light_message_filter.h"
#include "content/browser/device_sensors/device_motion_message_filter.h"
#include "content/browser/device_sensors/device_orientation_absolute_message_filter.h"
#include "content/browser/device_sensors/device_orientation_message_filter.h"
#include "content/browser/dom_storage/dom_storage_context_wrapper.h"
#include "content/browser/dom_storage/dom_storage_message_filter.h"
#include "content/browser/fileapi/fileapi_message_filter.h"
#include "content/browser/frame_host/render_frame_message_filter.h"
#include "content/browser/gpu/browser_gpu_memory_buffer_manager.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/browser/gpu/shader_disk_cache.h"
#include "content/browser/histogram_message_filter.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/browser/indexed_db/indexed_db_dispatcher_host.h"
#include "content/browser/loader/resource_message_filter.h"
#include "content/browser/loader/resource_scheduler_filter.h"
#include "content/browser/media/capture/audio_mirroring_manager.h"
#include "content/browser/media/capture/image_capture_impl.h"
#include "content/browser/media/media_internals.h"
#include "content/browser/media/midi_host.h"
#include "content/browser/memory/memory_message_filter.h"
#include "content/browser/message_port_message_filter.h"
#include "content/browser/mime_registry_impl.h"
#include "content/browser/mojo/constants.h"
#include "content/browser/mojo/mojo_child_connection.h"
#include "content/browser/notifications/notification_message_filter.h"
#include "content/browser/notifications/platform_notification_context_impl.h"
#include "content/browser/permissions/permission_service_context.h"
#include "content/browser/permissions/permission_service_impl.h"
#include "content/browser/profiler_message_filter.h"
#include "content/browser/push_messaging/push_messaging_message_filter.h"
#include "content/browser/quota_dispatcher_host.h"
#include "content/browser/renderer_host/clipboard_message_filter.h"
#include "content/browser/renderer_host/database_message_filter.h"
#include "content/browser/renderer_host/file_utilities_message_filter.h"
#include "content/browser/renderer_host/gamepad_browser_message_filter.h"
#include "content/browser/renderer_host/media/audio_input_renderer_host.h"
#include "content/browser/renderer_host/media/audio_renderer_host.h"
#include "content/browser/renderer_host/media/media_stream_dispatcher_host.h"
#include "content/browser/renderer_host/media/peer_connection_tracker_host.h"
#include "content/browser/renderer_host/media/video_capture_host.h"
#include "content/browser/renderer_host/offscreen_canvas_surface_impl.h"
#include "content/browser/renderer_host/pepper/pepper_message_filter.h"
#include "content/browser/renderer_host/pepper/pepper_renderer_connection.h"
#include "content/browser/renderer_host/render_message_filter.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_helper.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/text_input_client_message_filter.h"
#include "content/browser/renderer_host/websocket_dispatcher_host.h"
#include "content/browser/resolve_proxy_msg_helper.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_dispatcher_host.h"
#include "content/browser/shared_worker/shared_worker_message_filter.h"
#include "content/browser/shared_worker/worker_storage_partition.h"
#include "content/browser/speech/speech_recognition_dispatcher_host.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/streams/stream_context.h"
#include "content/browser/tracing/trace_message_filter.h"
#include "content/browser/webui/web_ui_controller_factory_registry.h"
#include "content/common/child_process_host_impl.h"
#include "content/common/child_process_messages.h"
#include "content/common/content_switches_internal.h"
#include "content/common/frame_messages.h"
#include "content/common/gpu_host_messages.h"
#include "content/common/in_process_child_thread_params.h"
#include "content/common/mojo/mojo_shell_connection_impl.h"
#include "content/common/render_process_messages.h"
#include "content/common/resource_messages.h"
#include "content/common/site_isolation_policy.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host_factory.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/browser/render_widget_host_view_frame_subscriber.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/worker_service.h"
#include "content/public/common/child_process_host.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/mojo_channel_switches.h"
#include "content/public/common/process_type.h"
#include "content/public/common/resource_type.h"
#include "content/public/common/result_codes.h"
#include "content/public/common/sandboxed_process_launcher_delegate.h"
#include "content/public/common/url_constants.h"
#include "device/battery/battery_monitor_impl.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gpu_switches.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "ipc/attachment_broker.h"
#include "ipc/attachment_broker_privileged.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_channel_mojo.h"
#include "ipc/ipc_logging.h"
#include "ipc/ipc_switches.h"
#include "media/base/media_switches.h"
#include "mojo/edk/embedder/embedder.h"
#include "net/url_request/url_request_context_getter.h"
#include "ppapi/shared_impl/ppapi_switches.h"
#include "services/shell/runner/common/switches.h"
#include "storage/browser/fileapi/sandbox_file_system_backend.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/ui_base_switches.h"
#include "ui/display/display_switches.h"
#include "ui/events/event_switches.h"
#include "ui/gfx/switches.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/gpu_switching_manager.h"
#include "ui/native_theme/native_theme_switches.h"

#if defined(OS_ANDROID)
#include "content/browser/android/child_process_launcher_android.h"
#include "content/browser/media/android/browser_demuxer_android.h"
#include "content/browser/mojo/service_registrar_android.h"
#include "content/browser/screen_orientation/screen_orientation_message_filter_android.h"
#include "ipc/ipc_sync_channel.h"
#endif

#if defined(OS_WIN)
#include "base/win/scoped_com_initializer.h"
#include "base/win/windows_version.h"
#include "content/browser/renderer_host/dwrite_font_proxy_message_filter_win.h"
#include "content/common/font_cache_dispatcher_win.h"
#include "content/common/sandbox_win.h"
#include "sandbox/win/src/sandbox_policy.h"
#include "ui/display/win/dpi.h"
#endif

#if defined(OS_MACOSX)
#include "content/browser/bootstrap_sandbox_manager_mac.h"
#include "content/browser/mach_broker_mac.h"
#endif

#if defined(OS_POSIX)
#include "content/browser/zygote_host/zygote_communication_linux.h"
#include "content/browser/zygote_host/zygote_host_impl_linux.h"
#include "content/public/browser/zygote_handle_linux.h"
#endif  // defined(OS_POSIX)

#if defined(USE_OZONE)
#include "ui/ozone/public/ozone_switches.h"
#endif

#if defined(ENABLE_BROWSER_CDMS)
#include "content/browser/media/cdm/browser_cdm_manager.h"
#endif

#if defined(ENABLE_PLUGINS)
#include "content/browser/plugin_service_impl.h"
#endif

#if defined(ENABLE_WEBRTC)
#include "content/browser/media/webrtc/webrtc_internals.h"
#include "content/browser/renderer_host/media/media_stream_track_metrics_host.h"
#include "content/browser/renderer_host/media/webrtc_identity_service_host.h"
#include "content/browser/renderer_host/p2p/socket_dispatcher_host.h"
#include "content/common/media/aec_dump_messages.h"
#include "content/common/media/media_stream_messages.h"
#endif

#if defined(MOJO_SHELL_CLIENT) && defined(USE_AURA)
#include "components/mus/common/switches.h"  // nogncheck
#endif

#if defined(OS_WIN)
#define IntToStringType base::IntToString16
#else
#define IntToStringType base::IntToString
#endif

namespace content {
namespace {

const char kSiteProcessMapKeyName[] = "content_site_process_map";

#ifdef ENABLE_WEBRTC
const base::FilePath::CharType kAecDumpFileNameAddition[] =
    FILE_PATH_LITERAL("aec_dump");
const base::FilePath::CharType kEventLogFileNameAddition[] =
    FILE_PATH_LITERAL("event_log");
#endif

void CacheShaderInfo(int32_t id, base::FilePath path) {
  ShaderCacheFactory::GetInstance()->SetCacheInfo(id, path);
}

void RemoveShaderInfo(int32_t id) {
  ShaderCacheFactory::GetInstance()->RemoveCacheInfo(id);
}

net::URLRequestContext* GetRequestContext(
    scoped_refptr<net::URLRequestContextGetter> request_context,
    scoped_refptr<net::URLRequestContextGetter> media_request_context,
    ResourceType resource_type) {
  // If the request has resource type of RESOURCE_TYPE_MEDIA, we use a request
  // context specific to media for handling it because these resources have
  // specific needs for caching.
  if (resource_type == RESOURCE_TYPE_MEDIA)
    return media_request_context->GetURLRequestContext();
  return request_context->GetURLRequestContext();
}

void GetContexts(
    ResourceContext* resource_context,
    scoped_refptr<net::URLRequestContextGetter> request_context,
    scoped_refptr<net::URLRequestContextGetter> media_request_context,
    ResourceType resource_type,
    int origin_pid,
    ResourceContext** resource_context_out,
    net::URLRequestContext** request_context_out) {
  *resource_context_out = resource_context;
  *request_context_out =
      GetRequestContext(request_context, media_request_context, resource_type);
}

#if defined(ENABLE_WEBRTC)

// Creates a file used for handing over to the renderer.
IPC::PlatformFileForTransit CreateFileForProcess(base::FilePath file_path) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  base::File dump_file(file_path,
                       base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_APPEND);
  if (!dump_file.IsValid()) {
    VLOG(1) << "Could not open AEC dump file, error="
            << dump_file.error_details();
    return IPC::InvalidPlatformFileForTransit();
  }
  return IPC::TakePlatformFileForTransit(std::move(dump_file));
}

// Allow us to only run the trial in the first renderer.
bool has_done_stun_trials = false;

#endif

// the global list of all renderer processes
base::LazyInstance<IDMap<RenderProcessHost>>::Leaky g_all_hosts =
    LAZY_INSTANCE_INITIALIZER;

// Map of site to process, to ensure we only have one RenderProcessHost per
// site in process-per-site mode.  Each map is specific to a BrowserContext.
class SiteProcessMap : public base::SupportsUserData::Data {
 public:
  typedef base::hash_map<std::string, RenderProcessHost*> SiteToProcessMap;
  SiteProcessMap() {}

  void RegisterProcess(const std::string& site, RenderProcessHost* process) {
    map_[site] = process;
  }

  RenderProcessHost* FindProcess(const std::string& site) {
    SiteToProcessMap::iterator i = map_.find(site);
    if (i != map_.end())
      return i->second;
    return NULL;
  }

  void RemoveProcess(RenderProcessHost* host) {
    // Find all instances of this process in the map, then separately remove
    // them.
    std::set<std::string> sites;
    for (SiteToProcessMap::const_iterator i = map_.begin(); i != map_.end();
         i++) {
      if (i->second == host)
        sites.insert(i->first);
    }
    for (std::set<std::string>::iterator i = sites.begin(); i != sites.end();
         i++) {
      SiteToProcessMap::iterator iter = map_.find(*i);
      if (iter != map_.end()) {
        DCHECK_EQ(iter->second, host);
        map_.erase(iter);
      }
    }
  }

 private:
  SiteToProcessMap map_;
};

// Find the SiteProcessMap specific to the given context.
SiteProcessMap* GetSiteProcessMapForBrowserContext(BrowserContext* context) {
  DCHECK(context);
  SiteProcessMap* map = static_cast<SiteProcessMap*>(
      context->GetUserData(kSiteProcessMapKeyName));
  if (!map) {
    map = new SiteProcessMap();
    context->SetUserData(kSiteProcessMapKeyName, map);
  }
  return map;
}

#if defined(OS_POSIX) && !defined(OS_ANDROID) && !defined(OS_MACOSX)
// This static member variable holds the zygote communication information for
// the renderer.
ZygoteHandle g_render_zygote;
#endif  // defined(OS_POSIX) && !defined(OS_ANDROID) && !defined(OS_MACOSX)

// NOTE: changes to this class need to be reviewed by the security team.
class RendererSandboxedProcessLauncherDelegate
    : public SandboxedProcessLauncherDelegate {
 public:
  explicit RendererSandboxedProcessLauncherDelegate(IPC::ChannelProxy* channel)
#if defined(OS_POSIX)
      : ipc_fd_(channel->TakeClientFileDescriptor())
#endif  // OS_POSIX
  {
  }

  ~RendererSandboxedProcessLauncherDelegate() override {}

#if defined(OS_WIN)
  bool PreSpawnTarget(sandbox::TargetPolicy* policy) override {
    AddBaseHandleClosePolicy(policy);

    const base::string16& sid =
        GetContentClient()->browser()->GetAppContainerSidForSandboxType(
            GetSandboxType());
    if (!sid.empty())
      AddAppContainerPolicy(policy, sid.c_str());

    return GetContentClient()->browser()->PreSpawnRenderer(policy);
  }

#elif defined(OS_POSIX)
#if !defined(OS_MACOSX) && !defined(OS_ANDROID)
  ZygoteHandle* GetZygote() override {
    const base::CommandLine& browser_command_line =
        *base::CommandLine::ForCurrentProcess();
    base::CommandLine::StringType renderer_prefix =
        browser_command_line.GetSwitchValueNative(switches::kRendererCmdPrefix);
    if (!renderer_prefix.empty())
      return nullptr;
    return GetGenericZygote();
  }
#endif  // !defined(OS_MACOSX) && !defined(OS_ANDROID)
  base::ScopedFD TakeIpcFd() override { return std::move(ipc_fd_); }
#endif  // OS_WIN

  SandboxType GetSandboxType() override { return SANDBOX_TYPE_RENDERER; }

 private:
#if defined(OS_POSIX)
  base::ScopedFD ipc_fd_;
#endif  // OS_POSIX
};

const char kSessionStorageHolderKey[] = "kSessionStorageHolderKey";

class SessionStorageHolder : public base::SupportsUserData::Data {
 public:
  SessionStorageHolder() {}
  ~SessionStorageHolder() override {}

  void Hold(const SessionStorageNamespaceMap& sessions, int view_route_id) {
    session_storage_namespaces_awaiting_close_[view_route_id] = sessions;
  }

  void Release(int old_route_id) {
    session_storage_namespaces_awaiting_close_.erase(old_route_id);
  }

 private:
  std::map<int, SessionStorageNamespaceMap>
      session_storage_namespaces_awaiting_close_;
  DISALLOW_COPY_AND_ASSIGN(SessionStorageHolder);
};

std::string UintVectorToString(const std::vector<unsigned>& vector) {
  std::string str;
  for (auto it : vector) {
    if (!str.empty())
      str += ",";
    str += base::UintToString(it);
  }
  return str;
}

}  // namespace

RendererMainThreadFactoryFunction g_renderer_main_thread_factory = NULL;

base::MessageLoop* g_in_process_thread;

base::MessageLoop*
RenderProcessHostImpl::GetInProcessRendererThreadForTesting() {
  return g_in_process_thread;
}

// Stores the maximum number of renderer processes the content module can
// create.
static size_t g_max_renderer_count_override = 0;

// static
size_t RenderProcessHost::GetMaxRendererProcessCount() {
  if (g_max_renderer_count_override)
    return g_max_renderer_count_override;

#if defined(OS_ANDROID)
  // On Android we don't maintain a limit of renderer process hosts - we are
  // happy with keeping a lot of these, as long as the number of live renderer
  // processes remains reasonable, and on Android the OS takes care of that.
  return std::numeric_limits<size_t>::max();
#endif

  // On other platforms, we calculate the maximum number of renderer process
  // hosts according to the amount of installed memory as reported by the OS.
  // The calculation assumes that you want the renderers to use half of the
  // installed RAM and assuming that each WebContents uses ~40MB.  If you modify
  // this assumption, you need to adjust the ThirtyFourTabs test to match the
  // expected number of processes.
  //
  // With the given amounts of installed memory below on a 32-bit CPU, the
  // maximum renderer count will roughly be as follows:
  //
  //   128 MB -> 3
  //   512 MB -> 6
  //  1024 MB -> 12
  //  4096 MB -> 51
  // 16384 MB -> 82 (kMaxRendererProcessCount)

  static size_t max_count = 0;
  if (!max_count) {
    const size_t kEstimatedWebContentsMemoryUsage =
#if defined(ARCH_CPU_64_BITS)
        60;  // In MB
#else
        40;  // In MB
#endif
    max_count = base::SysInfo::AmountOfPhysicalMemoryMB() / 2;
    max_count /= kEstimatedWebContentsMemoryUsage;

    const size_t kMinRendererProcessCount = 3;
    max_count = std::max(max_count, kMinRendererProcessCount);
    max_count = std::min(max_count, kMaxRendererProcessCount);
  }
  return max_count;
}

// static
bool g_run_renderer_in_process_ = false;

// static
void RenderProcessHost::SetMaxRendererProcessCount(size_t count) {
  g_max_renderer_count_override = count;
}

#if defined(OS_POSIX) && !defined(OS_ANDROID) && !defined(OS_MACOSX)
// static
void RenderProcessHostImpl::EarlyZygoteLaunch() {
  DCHECK(!g_render_zygote);
  // TODO(kerrnel): Investigate doing this without the ZygoteHostImpl as a
  // proxy. It is currently done this way due to concerns about race
  // conditions.
  ZygoteHostImpl::GetInstance()->SetRendererSandboxStatus(
      (*GetGenericZygote())->GetSandboxStatus());
}
#endif  // defined(OS_POSIX) && !defined(OS_ANDROID) && !defined(OS_MACOSX)

RenderProcessHostImpl::RenderProcessHostImpl(
    BrowserContext* browser_context,
    StoragePartitionImpl* storage_partition_impl,
    bool is_for_guests_only)
    : fast_shutdown_started_(false),
      deleting_soon_(false),
#ifndef NDEBUG
      is_self_deleted_(false),
#endif
      pending_views_(0),
      child_token_(mojo::edk::GenerateRandomToken()),
      visible_widgets_(0),
      is_process_backgrounded_(false),
      is_initialized_(false),
      id_(ChildProcessHostImpl::GenerateChildProcessUniqueId()),
      browser_context_(browser_context),
      storage_partition_impl_(storage_partition_impl),
      sudden_termination_allowed_(true),
      ignore_input_events_(false),
      is_for_guests_only_(is_for_guests_only),
      gpu_observer_registered_(false),
      delayed_cleanup_needed_(false),
      within_process_died_observer_(false),
      power_monitor_broadcaster_(this),
      worker_ref_count_(0),
      max_worker_count_(0),
      permission_service_context_(new PermissionServiceContext(this)),
      channel_connected_(false),
      sent_render_process_ready_(false),
#if defined(OS_ANDROID)
      never_signaled_(base::WaitableEvent::ResetPolicy::MANUAL,
                      base::WaitableEvent::InitialState::NOT_SIGNALED),
#endif
      weak_factory_(this) {
  widget_helper_ = new RenderWidgetHelper();

  ChildProcessSecurityPolicyImpl::GetInstance()->Add(GetID());

  CHECK(!BrowserMainRunner::ExitedMainMessageLoop());
  RegisterHost(GetID(), this);
  g_all_hosts.Get().set_check_on_null_data(true);
  // Initialize |child_process_activity_time_| to a reasonable value.
  mark_child_process_activity_time();

  if (!GetBrowserContext()->IsOffTheRecord() &&
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableGpuShaderDiskCache)) {
    BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                            base::Bind(&CacheShaderInfo, GetID(),
                                       storage_partition_impl_->GetPath()));
  }

#if defined(OS_MACOSX)
  if (BootstrapSandboxManager::ShouldEnable())
    AddObserver(BootstrapSandboxManager::GetInstance());
#endif

#if USE_ATTACHMENT_BROKER
  // Construct the privileged attachment broker early in the life cycle of a
  // render process. This ensures that when a test is being run in one of the
  // single process modes, the global attachment broker is the privileged
  // attachment broker, rather than an unprivileged attachment broker.
#if defined(OS_MACOSX)
  IPC::AttachmentBrokerPrivileged::CreateBrokerIfNeeded(
      MachBroker::GetInstance());
#else
  IPC::AttachmentBrokerPrivileged::CreateBrokerIfNeeded();
#endif  // defined(OS_MACOSX)
#endif  // USE_ATTACHMENT_BROKER

  shell::Connector* connector =
      BrowserContext::GetShellConnectorFor(browser_context_);
  // Some embedders may not initialize Mojo or the shell connector for a browser
  // context (e.g. Android WebView)... so just fall back to the per-process
  // connector.
  if (!connector) {
    // Additionally, some test code may not initialize the process-wide
    // MojoShellConnection prior to this point. This class of test code doesn't
    // care about render processes so we can initialize a dummy one.
    if (!MojoShellConnection::GetForProcess()) {
      shell::mojom::ShellClientRequest request =
          mojo::GetProxy(&test_shell_client_);
      MojoShellConnection::SetForProcess(MojoShellConnection::Create(
          std::move(request)));
    }
    connector = MojoShellConnection::GetForProcess()->GetConnector();
  }
  mojo_child_connection_.reset(new MojoChildConnection(
      kRendererMojoApplicationName,
      base::StringPrintf("%d_%d", id_, instance_id_++),
      child_token_,
      connector));
}

// static
void RenderProcessHostImpl::ShutDownInProcessRenderer() {
  DCHECK(g_run_renderer_in_process_);

  switch (g_all_hosts.Pointer()->size()) {
    case 0:
      return;
    case 1: {
      RenderProcessHostImpl* host = static_cast<RenderProcessHostImpl*>(
          AllHostsIterator().GetCurrentValue());
      FOR_EACH_OBSERVER(RenderProcessHostObserver, host->observers_,
                        RenderProcessHostDestroyed(host));
#ifndef NDEBUG
      host->is_self_deleted_ = true;
#endif
      delete host;
      return;
    }
    default:
      NOTREACHED() << "There should be only one RenderProcessHost when running "
                   << "in-process.";
  }
}

void RenderProcessHostImpl::RegisterRendererMainThreadFactory(
    RendererMainThreadFactoryFunction create) {
  g_renderer_main_thread_factory = create;
}

RenderProcessHostImpl::~RenderProcessHostImpl() {
#ifndef NDEBUG
  DCHECK(is_self_deleted_)
      << "RenderProcessHostImpl is destroyed by something other than itself";
#endif

  // Make sure to clean up the in-process renderer before the channel, otherwise
  // it may still run and have its IPCs fail, causing asserts.
  in_process_renderer_.reset();

  ChildProcessSecurityPolicyImpl::GetInstance()->Remove(GetID());

  if (gpu_observer_registered_) {
    ui::GpuSwitchingManager::GetInstance()->RemoveObserver(this);
    gpu_observer_registered_ = false;
  }

#if USE_ATTACHMENT_BROKER
  IPC::AttachmentBroker::GetGlobal()->DeregisterCommunicationChannel(
      channel_.get());
#endif
  // We may have some unsent messages at this point, but that's OK.
  channel_.reset();
  while (!queued_messages_.empty()) {
    delete queued_messages_.front();
    queued_messages_.pop();
  }

  UnregisterHost(GetID());

  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableGpuShaderDiskCache)) {
    BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                            base::Bind(&RemoveShaderInfo, GetID()));
  }
}

void RenderProcessHostImpl::EnableSendQueue() {
  is_initialized_ = false;
}

bool RenderProcessHostImpl::Init() {
  // calling Init() more than once does nothing, this makes it more convenient
  // for the view host which may not be sure in some cases
  if (channel_)
    return true;

  base::CommandLine::StringType renderer_prefix;
  // A command prefix is something prepended to the command line of the spawned
  // process.
  const base::CommandLine& browser_command_line =
      *base::CommandLine::ForCurrentProcess();
  renderer_prefix =
      browser_command_line.GetSwitchValueNative(switches::kRendererCmdPrefix);

#if defined(OS_LINUX)
  int flags = renderer_prefix.empty() ? ChildProcessHost::CHILD_ALLOW_SELF
                                      : ChildProcessHost::CHILD_NORMAL;
#else
  int flags = ChildProcessHost::CHILD_NORMAL;
#endif

  // Find the renderer before creating the channel so if this fails early we
  // return without creating the channel.
  base::FilePath renderer_path = ChildProcessHost::GetChildPath(flags);
  if (renderer_path.empty())
    return false;

  channel_connected_ = false;
  sent_render_process_ready_ = false;

  // Setup the IPC channel.
  const std::string channel_id =
      IPC::Channel::GenerateVerifiedChannelID(std::string());
  channel_ = CreateChannelProxy(channel_id);

  // Call the embedder first so that their IPC filters have priority.
  GetContentClient()->browser()->RenderProcessWillLaunch(this);

  CreateMessageFilters();
  RegisterMojoInterfaces();

  if (run_renderer_in_process()) {
    DCHECK(g_renderer_main_thread_factory);
    // Crank up a thread and run the initialization there.  With the way that
    // messages flow between the browser and renderer, this thread is required
    // to prevent a deadlock in single-process mode.  Since the primordial
    // thread in the renderer process runs the WebKit code and can sometimes
    // make blocking calls to the UI thread (i.e. this thread), they need to run
    // on separate threads.
    in_process_renderer_.reset(
        g_renderer_main_thread_factory(InProcessChildThreadParams(
            channel_id,
            BrowserThread::UnsafeGetMessageLoopForThread(BrowserThread::IO)
                ->task_runner(),
            mojo_channel_token_,
            mojo_child_connection_->shell_client_token())));

    base::Thread::Options options;
#if defined(OS_WIN) && !defined(OS_MACOSX) && !defined(TOOLKIT_QT)
    // In-process plugins require this to be a UI message loop.
    options.message_loop_type = base::MessageLoop::TYPE_UI;
#else
    // We can't have multiple UI loops on Linux and Android, so we don't support
    // in-process plugins.
    options.message_loop_type = base::MessageLoop::TYPE_DEFAULT;
#endif

    // As for execution sequence, this callback should have no any dependency
    // on starting in-process-render-thread.
    // So put it here to trigger ChannelMojo initialization earlier to enable
    // in-process-render-thread using ChannelMojo there.
    OnProcessLaunched();  // Fake a callback that the process is ready.

    in_process_renderer_->StartWithOptions(options);

    g_in_process_thread = in_process_renderer_->message_loop();

  } else {
    // Build command line for renderer.  We call AppendRendererCommandLine()
    // first so the process type argument will appear first.
    base::CommandLine* cmd_line = new base::CommandLine(renderer_path);
    if (!renderer_prefix.empty())
      cmd_line->PrependWrapper(renderer_prefix);
    AppendRendererCommandLine(cmd_line);
    cmd_line->AppendSwitchASCII(switches::kProcessChannelID, channel_id);

    // Spawn the child process asynchronously to avoid blocking the UI thread.
    // As long as there's no renderer prefix, we can use the zygote process
    // at this stage.
    child_process_launcher_.reset(new ChildProcessLauncher(
        new RendererSandboxedProcessLauncherDelegate(channel_.get()), cmd_line,
        GetID(), this, child_token_,
        base::Bind(&RenderProcessHostImpl::OnMojoError,
                   weak_factory_.GetWeakPtr(),
                   base::ThreadTaskRunnerHandle::Get())));

    fast_shutdown_started_ = false;
  }

  if (!gpu_observer_registered_) {
    gpu_observer_registered_ = true;
    ui::GpuSwitchingManager::GetInstance()->AddObserver(this);
  }

  power_monitor_broadcaster_.Init();

  is_initialized_ = true;
  init_time_ = base::TimeTicks::Now();
  return true;
}

std::unique_ptr<IPC::ChannelProxy> RenderProcessHostImpl::CreateChannelProxy(
    const std::string& channel_id) {
  scoped_refptr<base::SingleThreadTaskRunner> runner =
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO);
  mojo_channel_token_ = mojo::edk::GenerateRandomToken();
  mojo::ScopedMessagePipeHandle handle =
      mojo::edk::CreateParentMessagePipe(mojo_channel_token_, child_token_);

  // Do NOT expand ifdef or run time condition checks here! Synchronous
  // IPCs from browser process are banned. It is only narrowly allowed
  // for Android WebView to maintain backward compatibility.
  // See crbug.com/526842 for details.
#if defined(OS_ANDROID)
  if (GetContentClient()->UsingSynchronousCompositing()) {
    return IPC::SyncChannel::Create(
        IPC::ChannelMojo::CreateServerFactory(std::move(handle)), this,
        runner.get(), true, &never_signaled_);
  }
#endif  // OS_ANDROID

  std::unique_ptr<IPC::ChannelProxy> channel(
      new IPC::ChannelProxy(this, runner.get()));
#if USE_ATTACHMENT_BROKER
  IPC::AttachmentBroker::GetGlobal()->RegisterCommunicationChannel(
      channel.get(), content::BrowserThread::GetMessageLoopProxyForThread(
      content::BrowserThread::IO));
#endif
  channel->Init(IPC::ChannelMojo::CreateServerFactory(std::move(handle)), true);
  return channel;
}

void RenderProcessHostImpl::CreateMessageFilters() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  AddFilter(new ResourceSchedulerFilter(GetID()));
  MediaInternals* media_internals = MediaInternals::GetInstance();
  media::AudioManager* audio_manager =
      BrowserMainLoop::GetInstance()->audio_manager();
  // Add BrowserPluginMessageFilter to ensure it gets the first stab at messages
  // from guests.
  scoped_refptr<BrowserPluginMessageFilter> bp_message_filter(
      new BrowserPluginMessageFilter(GetID()));
  AddFilter(bp_message_filter.get());

  scoped_refptr<net::URLRequestContextGetter> request_context(
      storage_partition_impl_->GetURLRequestContext());
  scoped_refptr<RenderMessageFilter> render_message_filter(
      new RenderMessageFilter(
          GetID(), GetBrowserContext(), request_context.get(),
          widget_helper_.get(), audio_manager, media_internals,
          storage_partition_impl_->GetDOMStorageContext(),
          storage_partition_impl_->GetCacheStorageContext()));
  AddFilter(render_message_filter.get());
  AddFilter(new RenderFrameMessageFilter(
      GetID(),
#if defined(ENABLE_PLUGINS)
      PluginServiceImpl::GetInstance(),
#else
      nullptr,
#endif
      GetBrowserContext(),
      request_context.get(),
      widget_helper_.get()));
  BrowserContext* browser_context = GetBrowserContext();
  ResourceContext* resource_context = browser_context->GetResourceContext();

  scoped_refptr<net::URLRequestContextGetter> media_request_context(
      GetStoragePartition()->GetMediaURLRequestContext());

  ResourceMessageFilter::GetContextsCallback get_contexts_callback(
      base::Bind(&GetContexts, browser_context->GetResourceContext(),
                 request_context, media_request_context));

  // Several filters need the Blob storage context, so fetch it in advance.
  scoped_refptr<ChromeBlobStorageContext> blob_storage_context =
      ChromeBlobStorageContext::GetFor(browser_context);

  ResourceMessageFilter* resource_message_filter = new ResourceMessageFilter(
      GetID(), PROCESS_TYPE_RENDERER,
      storage_partition_impl_->GetAppCacheService(),
      blob_storage_context.get(),
      storage_partition_impl_->GetFileSystemContext(),
      storage_partition_impl_->GetServiceWorkerContext(),
      storage_partition_impl_->GetHostZoomLevelContext(),
      get_contexts_callback);

  AddFilter(resource_message_filter);
  MediaStreamManager* media_stream_manager =
      BrowserMainLoop::GetInstance()->media_stream_manager();
  // The AudioInputRendererHost and AudioRendererHost needs to be available for
  // lookup, so it's stashed in a member variable.
  audio_input_renderer_host_ = new AudioInputRendererHost(
      GetID(), base::GetProcId(GetHandle()), audio_manager,
      media_stream_manager, AudioMirroringManager::GetInstance(),
      BrowserMainLoop::GetInstance()->user_input_monitor());
  AddFilter(audio_input_renderer_host_.get());
  audio_renderer_host_ = new AudioRendererHost(
      GetID(), audio_manager, AudioMirroringManager::GetInstance(),
      media_internals, media_stream_manager,
      browser_context->GetResourceContext()->GetMediaDeviceIDSalt());
  AddFilter(audio_renderer_host_.get());
  AddFilter(
      new MidiHost(GetID(), BrowserMainLoop::GetInstance()->midi_manager()));
  AddFilter(new VideoCaptureHost(media_stream_manager));
  AddFilter(new AppCacheDispatcherHost(
      storage_partition_impl_->GetAppCacheService(), GetID()));
  AddFilter(new ClipboardMessageFilter(blob_storage_context));
  AddFilter(new DOMStorageMessageFilter(
      storage_partition_impl_->GetDOMStorageContext()));
  AddFilter(new IndexedDBDispatcherHost(
      GetID(), storage_partition_impl_->GetURLRequestContext(),
      storage_partition_impl_->GetIndexedDBContext(),
      blob_storage_context.get()));

#if defined(ENABLE_WEBRTC)
  AddFilter(new WebRTCIdentityServiceHost(
      GetID(), storage_partition_impl_->GetWebRTCIdentityStore(),
      resource_context));
  peer_connection_tracker_host_ = new PeerConnectionTrackerHost(GetID());
  AddFilter(peer_connection_tracker_host_.get());
  AddFilter(new MediaStreamDispatcherHost(
      GetID(), browser_context->GetResourceContext()->GetMediaDeviceIDSalt(),
      media_stream_manager));
  AddFilter(new MediaStreamTrackMetricsHost());
#endif
#if defined(ENABLE_PLUGINS)
  AddFilter(new PepperRendererConnection(GetID()));
#endif
#if defined(ENABLE_WEB_SPEECH) || defined(OS_ANDROID)
  AddFilter(new SpeechRecognitionDispatcherHost(
      GetID(), storage_partition_impl_->GetURLRequestContext()));
#endif
  AddFilter(new FileAPIMessageFilter(
      GetID(), storage_partition_impl_->GetURLRequestContext(),
      storage_partition_impl_->GetFileSystemContext(),
      blob_storage_context.get(), StreamContext::GetFor(browser_context)));
  AddFilter(new BlobDispatcherHost(
      GetID(), blob_storage_context,
      make_scoped_refptr(storage_partition_impl_->GetFileSystemContext())));
  AddFilter(new FileUtilitiesMessageFilter(GetID()));
  AddFilter(
      new DatabaseMessageFilter(storage_partition_impl_->GetDatabaseTracker()));
#if defined(OS_MACOSX)
  AddFilter(new TextInputClientMessageFilter());
#elif defined(OS_WIN)
  AddFilter(new DWriteFontProxyMessageFilter());

  // The FontCacheDispatcher is required only when we're using GDI rendering.
  // TODO(scottmg): pdf/ppapi still require the renderer to be able to precache
  // GDI fonts (http://crbug.com/383227), even when using DirectWrite. This
  // should eventually be if (!ShouldUseDirectWrite()) guarded.
  channel_->AddFilter(new FontCacheDispatcher());
#elif defined(OS_ANDROID)
  browser_demuxer_android_ = new BrowserDemuxerAndroid();
  AddFilter(browser_demuxer_android_.get());
#endif
#if defined(ENABLE_BROWSER_CDMS)
  AddFilter(new BrowserCdmManager(GetID(), NULL));
#endif

  WebSocketDispatcherHost::GetRequestContextCallback
      websocket_request_context_callback(
          base::Bind(&GetRequestContext, request_context, media_request_context,
                     RESOURCE_TYPE_SUB_RESOURCE));

  AddFilter(new WebSocketDispatcherHost(
      GetID(), websocket_request_context_callback, blob_storage_context.get(),
      storage_partition_impl_));

  message_port_message_filter_ = new MessagePortMessageFilter(
      base::Bind(&RenderWidgetHelper::GetNextRoutingID,
                 base::Unretained(widget_helper_.get())));
  AddFilter(message_port_message_filter_.get());

  scoped_refptr<CacheStorageDispatcherHost> cache_storage_filter =
      new CacheStorageDispatcherHost();
  cache_storage_filter->Init(storage_partition_impl_->GetCacheStorageContext());
  AddFilter(cache_storage_filter.get());

  scoped_refptr<ServiceWorkerDispatcherHost> service_worker_filter =
      new ServiceWorkerDispatcherHost(
          GetID(), message_port_message_filter_.get(), resource_context);
  service_worker_filter->Init(
      storage_partition_impl_->GetServiceWorkerContext());
  AddFilter(service_worker_filter.get());

  AddFilter(new SharedWorkerMessageFilter(
      GetID(), resource_context,
      WorkerStoragePartition(
          storage_partition_impl_->GetURLRequestContext(),
          storage_partition_impl_->GetMediaURLRequestContext(),
          storage_partition_impl_->GetAppCacheService(),
          storage_partition_impl_->GetQuotaManager(),
          storage_partition_impl_->GetFileSystemContext(),
          storage_partition_impl_->GetDatabaseTracker(),
          storage_partition_impl_->GetIndexedDBContext(),
          storage_partition_impl_->GetServiceWorkerContext()),
      message_port_message_filter_.get()));

#if defined(ENABLE_WEBRTC)
  p2p_socket_dispatcher_host_ = new P2PSocketDispatcherHost(
      resource_context, request_context.get());
  AddFilter(p2p_socket_dispatcher_host_.get());
#endif

  AddFilter(new TraceMessageFilter(GetID()));
  AddFilter(new ResolveProxyMsgHelper(request_context.get()));
  AddFilter(new QuotaDispatcherHost(
      GetID(), storage_partition_impl_->GetQuotaManager(),
      GetContentClient()->browser()->CreateQuotaPermissionContext()));

  notification_message_filter_ = new NotificationMessageFilter(
      GetID(), storage_partition_impl_->GetPlatformNotificationContext(),
      resource_context, browser_context);
  AddFilter(notification_message_filter_.get());

  AddFilter(new GamepadBrowserMessageFilter());
  AddFilter(new DeviceLightMessageFilter());
  AddFilter(new DeviceMotionMessageFilter());
  AddFilter(new DeviceOrientationMessageFilter());
  AddFilter(new DeviceOrientationAbsoluteMessageFilter());
  AddFilter(new ProfilerMessageFilter(PROCESS_TYPE_RENDERER));
  AddFilter(new HistogramMessageFilter());
  AddFilter(new MemoryMessageFilter(this));
  AddFilter(new PushMessagingMessageFilter(
      GetID(), storage_partition_impl_->GetServiceWorkerContext()));
#if defined(OS_ANDROID)
  AddFilter(new ScreenOrientationMessageFilterAndroid());
#endif
}

void RenderProcessHostImpl::RegisterMojoInterfaces() {
#if !defined(OS_ANDROID)
  GetInterfaceRegistry()->AddInterface(
      base::Bind(&device::BatteryMonitorImpl::Create));
#endif

  GetInterfaceRegistry()->AddInterface(
      base::Bind(&PermissionServiceContext::CreateService,
                 base::Unretained(permission_service_context_.get())));

  // TODO(mcasas): finalize arguments.
  GetInterfaceRegistry()->AddInterface(
      base::Bind(&ImageCaptureImpl::Create));

  GetInterfaceRegistry()->AddInterface(
      base::Bind(&OffscreenCanvasSurfaceImpl::Create));

  GetInterfaceRegistry()->AddInterface(base::Bind(
      &BackgroundSyncContext::CreateService,
      base::Unretained(storage_partition_impl_->GetBackgroundSyncContext())));

  GetInterfaceRegistry()->AddInterface(base::Bind(
      &PlatformNotificationContextImpl::CreateService,
      base::Unretained(
          storage_partition_impl_->GetPlatformNotificationContext()), GetID()));

  GetInterfaceRegistry()->AddInterface(
      base::Bind(&RenderProcessHostImpl::CreateStoragePartitionService,
                 base::Unretained(this)));

  GetInterfaceRegistry()->AddInterface(
      base::Bind(&webmessaging::BroadcastChannelProvider::Connect,
                 base::Unretained(
                     storage_partition_impl_->GetBroadcastChannelProvider())));

  GetInterfaceRegistry()->AddInterface(
      base::Bind(&MimeRegistryImpl::Create),
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE));

#if defined(OS_ANDROID)
  ServiceRegistrarAndroid::RegisterProcessHostServices(
      mojo_child_connection_->service_registry_android());
#endif

  GetContentClient()->browser()->ExposeInterfacesToRenderer(
      GetInterfaceRegistry(), this);
}

void RenderProcessHostImpl::CreateStoragePartitionService(
    mojo::InterfaceRequest<mojom::StoragePartitionService> request) {
  // DO NOT REMOVE THIS COMMAND LINE CHECK WITHOUT SECURITY REVIEW!
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kMojoLocalStorage)) {
    storage_partition_impl_->Bind(std::move(request));
  }
}

int RenderProcessHostImpl::GetNextRoutingID() {
  return widget_helper_->GetNextRoutingID();
}

void RenderProcessHostImpl::ResumeDeferredNavigation(
    const GlobalRequestID& request_id) {
  widget_helper_->ResumeDeferredNavigation(request_id);
}

void RenderProcessHostImpl::NotifyTimezoneChange(const std::string& zone_id) {
  Send(new ViewMsg_TimezoneChange(zone_id));
}

shell::InterfaceRegistry* RenderProcessHostImpl::GetInterfaceRegistry() {
  return GetChildConnection()->GetInterfaceRegistry();
}

shell::InterfaceProvider* RenderProcessHostImpl::GetRemoteInterfaces() {
  return GetChildConnection()->GetRemoteInterfaces();
}

shell::Connection* RenderProcessHostImpl::GetChildConnection() {
  DCHECK(mojo_child_connection_);
  return mojo_child_connection_->connection();
}

std::unique_ptr<base::SharedPersistentMemoryAllocator>
RenderProcessHostImpl::TakeMetricsAllocator() {
  return std::move(metrics_allocator_);
}

const base::TimeTicks& RenderProcessHostImpl::GetInitTimeForNavigationMetrics()
    const {
  return init_time_;
}

#if defined(ENABLE_BROWSER_CDMS)
scoped_refptr<media::MediaKeys> RenderProcessHostImpl::GetCdm(
    int render_frame_id,
    int cdm_id) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserCdmManager* manager = BrowserCdmManager::FromProcess(GetID());
  if (!manager)
    return nullptr;
  return manager->GetCdm(render_frame_id, cdm_id);
}
#endif

bool RenderProcessHostImpl::IsProcessBackgrounded() const {
  return is_process_backgrounded_;
}

void RenderProcessHostImpl::IncrementWorkerRefCount() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ++worker_ref_count_;
  if (worker_ref_count_ > max_worker_count_)
    max_worker_count_ = worker_ref_count_;
}

void RenderProcessHostImpl::DecrementWorkerRefCount() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_GT(worker_ref_count_, 0);
  --worker_ref_count_;
  if (worker_ref_count_ == 0)
    Cleanup();
}

void RenderProcessHostImpl::PurgeAndSuspend() {
  Send(new ChildProcessMsg_PurgeAndSuspend());
}

void RenderProcessHostImpl::AddRoute(int32_t routing_id,
                                     IPC::Listener* listener) {
  CHECK(!listeners_.Lookup(routing_id)) << "Found Routing ID Conflict: "
                                        << routing_id;
  listeners_.AddWithID(listener, routing_id);
}

void RenderProcessHostImpl::RemoveRoute(int32_t routing_id) {
  DCHECK(listeners_.Lookup(routing_id) != NULL);
  listeners_.Remove(routing_id);
  Cleanup();
}

void RenderProcessHostImpl::AddObserver(RenderProcessHostObserver* observer) {
  observers_.AddObserver(observer);
}

void RenderProcessHostImpl::RemoveObserver(
    RenderProcessHostObserver* observer) {
  observers_.RemoveObserver(observer);
}

void RenderProcessHostImpl::ShutdownForBadMessage() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kDisableKillAfterBadIPC))
    return;

  if (run_renderer_in_process()) {
    // In single process mode it is better if we don't suicide but just
    // crash.
    CHECK(false);
  }

  // We kill the renderer but don't include a NOTREACHED, because we want the
  // browser to try to survive when it gets illegal messages from the renderer.
  Shutdown(RESULT_CODE_KILLED_BAD_MESSAGE, false);

  // Report a crash, since none will be generated by the killed renderer.
  base::debug::DumpWithoutCrashing();

  // Log the renderer kill to the histogram tracking all kills.
  BrowserChildProcessHostImpl::HistogramBadMessageTerminated(
      PROCESS_TYPE_RENDERER);
}

void RenderProcessHostImpl::WidgetRestored() {
  visible_widgets_++;
  UpdateProcessPriority();
  DCHECK(!is_process_backgrounded_);
}

void RenderProcessHostImpl::WidgetHidden() {
  // On startup, the browser will call Hide. We ignore this call.
  if (visible_widgets_ == 0)
    return;

  --visible_widgets_;
  if (visible_widgets_ == 0) {
    DCHECK(!is_process_backgrounded_);
    UpdateProcessPriority();
  }
}

int RenderProcessHostImpl::VisibleWidgetCount() const {
  return visible_widgets_;
}

void RenderProcessHostImpl::AudioStateChanged() {
  UpdateProcessPriority();
}

bool RenderProcessHostImpl::IsForGuestsOnly() const {
  return is_for_guests_only_;
}

StoragePartition* RenderProcessHostImpl::GetStoragePartition() const {
  return storage_partition_impl_;
}

static void AppendCompositorCommandLineFlags(base::CommandLine* command_line) {
  command_line->AppendSwitchASCII(
      switches::kNumRasterThreads,
      base::IntToString(NumberOfRendererRasterThreads()));

  if (IsGpuRasterizationEnabled())
    command_line->AppendSwitch(switches::kEnableGpuRasterization);

  if (IsAsyncWorkerContextEnabled())
    command_line->AppendSwitch(switches::kEnableGpuAsyncWorkerContext);

  int msaa_sample_count = GpuRasterizationMSAASampleCount();
  if (msaa_sample_count >= 0) {
    command_line->AppendSwitchASCII(switches::kGpuRasterizationMSAASampleCount,
                                    base::IntToString(msaa_sample_count));
  }

  if (IsZeroCopyUploadEnabled())
    command_line->AppendSwitch(switches::kEnableZeroCopy);
  if (!IsPartialRasterEnabled())
    command_line->AppendSwitch(switches::kDisablePartialRaster);

  if (IsForceGpuRasterizationEnabled())
    command_line->AppendSwitch(switches::kForceGpuRasterization);

  if (IsGpuMemoryBufferCompositorResourcesEnabled()) {
    command_line->AppendSwitch(
        switches::kEnableGpuMemoryBufferCompositorResources);
  }

  if (IsMainFrameBeforeActivationEnabled())
    command_line->AppendSwitch(cc::switches::kEnableMainFrameBeforeActivation);

  // Persistent buffers may come at a performance hit (not all platform specific
  // buffers support it), so only enable them if partial raster is enabled and
  // we are actually going to use them.
  // TODO(dcastagna): Once GPU_READ_CPU_READ_WRITE_PERSISTENT is removed
  // kContentImageTextureTarget and kVideoImageTextureTarget can be merged into
  // one flag.
  gfx::BufferUsage buffer_usage =
      IsPartialRasterEnabled()
          ? gfx::BufferUsage::GPU_READ_CPU_READ_WRITE_PERSISTENT
          : gfx::BufferUsage::GPU_READ_CPU_READ_WRITE;
  std::vector<unsigned> image_targets(
      static_cast<size_t>(gfx::BufferFormat::LAST) + 1, GL_TEXTURE_2D);
  for (size_t format = 0;
       format < static_cast<size_t>(gfx::BufferFormat::LAST) + 1; format++) {
    image_targets[format] =
        BrowserGpuMemoryBufferManager::GetImageTextureTarget(
            static_cast<gfx::BufferFormat>(format), buffer_usage);
  }
  command_line->AppendSwitchASCII(switches::kContentImageTextureTarget,
                                  UintVectorToString(image_targets));

  for (size_t format = 0;
       format < static_cast<size_t>(gfx::BufferFormat::LAST) + 1; format++) {
    image_targets[format] =
        BrowserGpuMemoryBufferManager::GetImageTextureTarget(
            static_cast<gfx::BufferFormat>(format),
            gfx::BufferUsage::GPU_READ_CPU_READ_WRITE);
  }
  command_line->AppendSwitchASCII(switches::kVideoImageTextureTarget,
                                  UintVectorToString(image_targets));

  // Appending disable-gpu-feature switches due to software rendering list.
  GpuDataManagerImpl* gpu_data_manager = GpuDataManagerImpl::GetInstance();
  DCHECK(gpu_data_manager);
  gpu_data_manager->AppendRendererCommandLine(command_line);
}

void RenderProcessHostImpl::AppendRendererCommandLine(
    base::CommandLine* command_line) const {
  // Pass the process type first, so it shows first in process listings.
  command_line->AppendSwitchASCII(switches::kProcessType,
                                  switches::kRendererProcess);

#if defined(OS_WIN)
  command_line->AppendArg(switches::kPrefetchArgumentRenderer);
#endif  // defined(OS_WIN)

  // Now send any options from our own command line we want to propagate.
  const base::CommandLine& browser_command_line =
      *base::CommandLine::ForCurrentProcess();
  PropagateBrowserCommandLineToRenderer(browser_command_line, command_line);

  // Pass on the browser locale.
  const std::string locale =
      GetContentClient()->browser()->GetApplicationLocale();
  command_line->AppendSwitchASCII(switches::kLang, locale);

  GetContentClient()->browser()->AppendExtraCommandLineSwitches(command_line,
                                                                GetID());

  if (IsPinchToZoomEnabled())
    command_line->AppendSwitch(switches::kEnablePinch);

#if defined(OS_WIN)
  command_line->AppendSwitchASCII(
      switches::kDeviceScaleFactor,
      base::DoubleToString(display::win::GetDPIScale()));
#endif

  AppendCompositorCommandLineFlags(command_line);

  if (!mojo_channel_token_.empty()) {
    command_line->AppendSwitchASCII(switches::kMojoChannelToken,
                                    mojo_channel_token_);
  }
  command_line->AppendSwitchASCII(switches::kMojoApplicationChannelToken,
                                  mojo_child_connection_->shell_client_token());
}

void RenderProcessHostImpl::PropagateBrowserCommandLineToRenderer(
    const base::CommandLine& browser_cmd,
    base::CommandLine* renderer_cmd) const {
  // Propagate the following switches to the renderer command line (along
  // with any associated values) if present in the browser command line.
  static const char* const kSwitchNames[] = {
    switches::kAgcStartupMinVolume,
    switches::kAecRefinedAdaptiveFilter,
    switches::kAllowLoopbackInPeerConnection,
    switches::kAndroidFontsPath,
    switches::kAudioBufferSize,
    switches::kBlinkPlatformLogChannels,
    switches::kBlinkSettings,
    switches::kCastEncoderUtilHeuristic,
    switches::kDefaultTileWidth,
    switches::kDefaultTileHeight,
    switches::kDisable2dCanvasImageChromium,
    switches::kDisable3DAPIs,
    switches::kDisableAcceleratedJpegDecoding,
    switches::kDisableAcceleratedVideoDecode,
    switches::kDisableBlinkFeatures,
    switches::kDisableBreakpad,
    switches::kDisablePreferCompositingToLCDText,
    switches::kDisableDatabases,
    switches::kDisableDisplayList2dCanvas,
    switches::kDisableDistanceFieldText,
    switches::kDisableFileSystem,
    switches::kDisableGestureRequirementForMediaPlayback,
    switches::kDisableGestureRequirementForPresentation,
    switches::kDisableGpuCompositing,
    switches::kDisableGpuMemoryBufferVideoFrames,
    switches::kDisableGpuVsync,
    switches::kDisableLowResTiling,
    switches::kDisableHistogramCustomizer,
    switches::kDisableIconNtp,
    switches::kDisableLCDText,
    switches::kDisableLocalStorage,
    switches::kDisableLogging,
    switches::kDisableMediaSuspend,
    switches::kDisableNotifications,
    switches::kDisableOverlayScrollbar,
    switches::kDisablePermissionsAPI,
    switches::kDisablePresentationAPI,
    switches::kDisablePinch,
    switches::kDisableRGBA4444Textures,
    switches::kDisableRTCSmoothnessAlgorithm,
    switches::kDisableSeccompFilterSandbox,
    switches::kDisableSharedWorkers,
    switches::kDisableSmoothScrolling,
    switches::kDisableSpeechAPI,
    switches::kDisableThreadedCompositing,
    switches::kDisableThreadedScrolling,
    switches::kDisableTouchAdjustment,
    switches::kDisableTouchDragDrop,
    switches::kDisableV8IdleTasks,
    switches::kDisableWebGLImageChromium,
    switches::kDomAutomationController,
    switches::kEnableBlinkFeatures,
    switches::kEnableBrowserSideNavigation,
    switches::kEnableDisplayList2dCanvas,
    switches::kEnableDistanceFieldText,
    switches::kEnableExperimentalCanvasFeatures,
    switches::kEnableExperimentalWebPlatformFeatures,
    switches::kEnableHeapProfiling,
    switches::kEnableGPUClientLogging,
    switches::kEnableGpuClientTracing,
    switches::kEnableGpuMemoryBufferVideoFrames,
    switches::kEnableGPUServiceLogging,
    switches::kEnableIconNtp,
    switches::kEnableLowResTiling,
    switches::kEnableMediaSuspend,
    switches::kEnableInbandTextTracks,
    switches::kEnableLCDText,
    switches::kEnableLogging,
    switches::kEnableMemoryBenchmarking,
    switches::kEnableNetworkInformation,
    switches::kEnableOverlayScrollbar,
    switches::kEnablePinch,
    switches::kEnablePluginPlaceholderTesting,
    switches::kEnablePreciseMemoryInfo,
    switches::kEnablePreferCompositingToLCDText,
    switches::kEnableRGBA4444Textures,
    switches::kEnableSkiaBenchmarking,
    switches::kEnableSlimmingPaintV2,
    switches::kEnableSmoothScrolling,
    switches::kEnableStatsTable,
    switches::kEnableThreadedCompositing,
    switches::kEnableTouchDragDrop,
    switches::kEnableUnsafeES3APIs,
    switches::kEnableUseZoomForDSF,
    switches::kEnableViewport,
    switches::kEnableVp9InMp4,
    switches::kEnableVtune,
    switches::kEnableWebBluetooth,
    switches::kEnableWebFontsInterventionTrigger,
    switches::kEnableWebFontsInterventionV2,
    switches::kEnableWebGLDraftExtensions,
    switches::kEnableWebGLImageChromium,
    switches::kEnableWebVR,
    switches::kExplicitlyAllowedPorts,
    switches::kForceDeviceScaleFactor,
    switches::kForceDisplayList2dCanvas,
    switches::kForceOverlayFullscreenVideo,
    switches::kFullMemoryCrashReport,
    switches::kInertVisualViewport,
    switches::kIPCConnectionTimeout,
    switches::kJavaScriptFlags,
    switches::kLoggingLevel,
    switches::kMainFrameResizesAreOrientationChanges,
    switches::kMaxUntiledLayerWidth,
    switches::kMaxUntiledLayerHeight,
    switches::kMemoryMetrics,
    switches::kMojoLocalStorage,
    switches::kNoReferrers,
    switches::kNoSandbox,
    switches::kOverridePluginPowerSaverForTesting,
    switches::kPassiveListenersDefault,
    switches::kPpapiInProcess,
    switches::kProfilerTiming,
    switches::kReducedReferrerGranularity,
    switches::kReduceSecurityForTesting,
    switches::kRegisterPepperPlugins,
    switches::kRendererStartupDialog,
    switches::kRootLayerScrolls,
    switches::kShowPaintRects,
    switches::kSitePerProcess,
    switches::kStatsCollectionController,
    switches::kTestType,
    switches::kTopDocumentIsolation,
    switches::kTouchEvents,
    switches::kTouchTextSelectionStrategy,
    switches::kTraceConfigFile,
    switches::kTraceToConsole,
    switches::kUseCrossProcessFramesForGuests,
    switches::kUseFakeUIForMediaStream,
    // This flag needs to be propagated to the renderer process for
    // --in-process-webgl.
    switches::kUseGL,
    switches::kUseMobileUserAgent,
    switches::kUseRemoteCompositing,
    switches::kV,
    switches::kV8CacheStrategiesForCacheStorage,
    switches::kVideoThreads,
    switches::kVideoUnderflowThresholdMs,
    switches::kVModule,
    // Please keep these in alphabetical order. Compositor switches here should
    // also be added to chrome/browser/chromeos/login/chrome_restart_request.cc.
    cc::switches::kDisableCachedPictureRaster,
    cc::switches::kDisableCompositedAntialiasing,
    cc::switches::kDisableThreadedAnimation,
    cc::switches::kEnableBeginFrameScheduling,
    cc::switches::kEnableGpuBenchmarking,
    cc::switches::kEnableLayerLists,
    cc::switches::kEnableTileCompression,
    cc::switches::kShowCompositedLayerBorders,
    cc::switches::kShowFPSCounter,
    cc::switches::kShowLayerAnimationBounds,
    cc::switches::kShowPropertyChangedRects,
    cc::switches::kShowReplicaScreenSpaceRects,
    cc::switches::kShowScreenSpaceRects,
    cc::switches::kShowSurfaceDamageRects,
    cc::switches::kSlowDownRasterScaleFactor,
    cc::switches::kTopControlsHideThreshold,
    cc::switches::kTopControlsShowThreshold,

    scheduler::switches::kDisableBackgroundTimerThrottling,

#if defined(ENABLE_PLUGINS)
    switches::kEnablePepperTesting,
#endif
#if defined(ENABLE_WEBRTC)
    switches::kDisableWebRtcHWDecoding,
    switches::kDisableWebRtcHWEncoding,
    switches::kEnableWebRtcDtls12,
    switches::kEnableWebRtcHWH264Encoding,
    switches::kEnableWebRtcStunOrigin,
    switches::kEnforceWebRtcIPPermissionCheck,
    switches::kForceWebRtcIPHandlingPolicy,
    switches::kWebRtcMaxCaptureFramerate,
#endif
    switches::kEnableLowEndDeviceMode,
    switches::kDisableLowEndDeviceMode,
#if defined(OS_ANDROID)
    switches::kDisableUnifiedMediaPipeline,
    switches::kRendererWaitForJavaDebugger,
#endif
#if defined(OS_MACOSX)
    // Allow this to be set when invoking the browser and relayed along.
    switches::kEnableSandboxLogging,
#endif
#if defined(OS_WIN)
    switches::kDisableWin32kRendererLockDown,
    switches::kEnableWin7WebRtcHWH264Decoding,
    switches::kTrySupportedChannelLayouts,
    switches::kTraceExportEventsToETW,
#endif
#if defined(USE_OZONE)
    switches::kOzonePlatform,
#endif
#if defined(OS_CHROMEOS)
    switches::kDisableVaapiAcceleratedVideoEncode,
#endif
#if defined(ENABLE_IPC_FUZZER)
    switches::kIpcDumpDirectory,
    switches::kIpcFuzzerTestcase,
#endif
#if defined(MOJO_SHELL_CLIENT) && defined(USE_AURA)
    switches::kUseMusInRenderer,
    mus::switches::kUseMojoGpuCommandBufferInMus,
#endif
  };
  renderer_cmd->CopySwitchesFrom(browser_cmd, kSwitchNames,
                                 arraysize(kSwitchNames));

  BrowserChildProcessHostImpl::CopyFeatureAndFieldTrialFlags(renderer_cmd);

  if (browser_cmd.HasSwitch(switches::kTraceStartup) &&
      BrowserMainLoop::GetInstance()->is_tracing_startup_for_duration()) {
    // Pass kTraceStartup switch to renderer only if startup tracing has not
    // finished.
    renderer_cmd->AppendSwitchASCII(
        switches::kTraceStartup,
        browser_cmd.GetSwitchValueASCII(switches::kTraceStartup));
  }

#if defined(ENABLE_WEBRTC)
  // Only run the Stun trials in the first renderer.
  if (!has_done_stun_trials &&
      browser_cmd.HasSwitch(switches::kWebRtcStunProbeTrialParameter)) {
    has_done_stun_trials = true;
    renderer_cmd->AppendSwitchASCII(
        switches::kWebRtcStunProbeTrialParameter,
        browser_cmd.GetSwitchValueASCII(
            switches::kWebRtcStunProbeTrialParameter));
  }
#endif

  // Disable databases in incognito mode.
  if (GetBrowserContext()->IsOffTheRecord() &&
      !browser_cmd.HasSwitch(switches::kDisableDatabases)) {
    renderer_cmd->AppendSwitch(switches::kDisableDatabases);
  }

  // Add kWaitForDebugger to let renderer process wait for a debugger.
  if (browser_cmd.HasSwitch(switches::kWaitForDebuggerChildren)) {
    // Look to pass-on the kWaitForDebugger flag.
    std::string value =
        browser_cmd.GetSwitchValueASCII(switches::kWaitForDebuggerChildren);
    if (value.empty() || value == switches::kRendererProcess) {
      renderer_cmd->AppendSwitch(switches::kWaitForDebugger);
    }
  }

  DCHECK(mojo_child_connection_);
  renderer_cmd->AppendSwitchASCII(switches::kPrimordialPipeToken,
                                  mojo_child_connection_->shell_client_token());

#if defined(OS_WIN) && !defined(OFFICIAL_BUILD)
  // Needed because we can't show the dialog from the sandbox. Don't pass
  // --no-sandbox in official builds because that would bypass the bad_flgs
  // prompt.
  if (renderer_cmd->HasSwitch(switches::kRendererStartupDialog) &&
      !renderer_cmd->HasSwitch(switches::kNoSandbox)) {
    renderer_cmd->AppendSwitch(switches::kNoSandbox);
  }
#endif
}

base::ProcessHandle RenderProcessHostImpl::GetHandle() const {
  if (run_renderer_in_process())
    return base::GetCurrentProcessHandle();

  if (!child_process_launcher_.get() || child_process_launcher_->IsStarting())
    return base::kNullProcessHandle;

  return child_process_launcher_->GetProcess().Handle();
}

bool RenderProcessHostImpl::IsReady() const {
  // The process launch result (that sets GetHandle()) and the channel
  // connection (that sets channel_connected_) can happen in either order.
  return GetHandle() && channel_connected_;
}

bool RenderProcessHostImpl::Shutdown(int exit_code, bool wait) {
  if (run_renderer_in_process())
    return false;  // Single process mode never shuts down the renderer.

#if defined(OS_ANDROID)
  // Android requires a different approach for killing.
  StopChildProcess(GetHandle());
  return true;
#else
  if (!child_process_launcher_.get() || child_process_launcher_->IsStarting())
    return false;

  return child_process_launcher_->GetProcess().Terminate(exit_code, wait);
#endif
}

bool RenderProcessHostImpl::FastShutdownIfPossible() {
  if (run_renderer_in_process())
    return false;  // Single process mode never shuts down the renderer.

  if (!GetContentClient()->browser()->IsFastShutdownPossible())
    return false;

  if (!child_process_launcher_.get() || child_process_launcher_->IsStarting() ||
      !GetHandle())
    return false;  // Render process hasn't started or is probably crashed.

  // Test if there's an unload listener.
  // NOTE: It's possible that an onunload listener may be installed
  // while we're shutting down, so there's a small race here.  Given that
  // the window is small, it's unlikely that the web page has much
  // state that will be lost by not calling its unload handlers properly.
  if (!SuddenTerminationAllowed())
    return false;

  if (worker_ref_count_ != 0) {
    if (survive_for_worker_start_time_.is_null())
      survive_for_worker_start_time_ = base::TimeTicks::Now();
    return false;
  }

  // Set this before ProcessDied() so observers can tell if the render process
  // died due to fast shutdown versus another cause.
  fast_shutdown_started_ = true;

  ProcessDied(false /* already_dead */, nullptr);
  return true;
}

bool RenderProcessHostImpl::Send(IPC::Message* msg) {
  TRACE_EVENT0("renderer_host", "RenderProcessHostImpl::Send");
#if !defined(OS_ANDROID)
  DCHECK(!msg->is_sync());
#endif

  if (!channel_) {
#if defined(OS_ANDROID)
    if (msg->is_sync()) {
      delete msg;
      return false;
    }
#endif
    if (!is_initialized_) {
      queued_messages_.push(msg);
      return true;
    } else {
      delete msg;
      return false;
    }
  }

  if (child_process_launcher_.get() && child_process_launcher_->IsStarting()) {
#if defined(OS_ANDROID)
    if (msg->is_sync()) {
      delete msg;
      return false;
    }
#endif
    queued_messages_.push(msg);
    return true;
  }

  return channel_->Send(msg);
}

bool RenderProcessHostImpl::OnMessageReceived(const IPC::Message& msg) {
  // If we're about to be deleted, or have initiated the fast shutdown sequence,
  // we ignore incoming messages.

  if (deleting_soon_ || fast_shutdown_started_)
    return false;

  mark_child_process_activity_time();
  if (msg.routing_id() == MSG_ROUTING_CONTROL) {
    // Dispatch control messages.
    IPC_BEGIN_MESSAGE_MAP(RenderProcessHostImpl, msg)
      IPC_MESSAGE_HANDLER(ChildProcessHostMsg_ShutdownRequest,
                          OnShutdownRequest)
      IPC_MESSAGE_HANDLER(RenderProcessHostMsg_SuddenTerminationChanged,
                          SuddenTerminationChanged)
      IPC_MESSAGE_HANDLER(ViewHostMsg_UserMetricsRecordAction,
                          OnUserMetricsRecordAction)
      IPC_MESSAGE_HANDLER(ViewHostMsg_Close_ACK, OnCloseACK)
#if defined(ENABLE_WEBRTC)
      IPC_MESSAGE_HANDLER(AecDumpMsg_RegisterAecDumpConsumer,
                          OnRegisterAecDumpConsumer)
      IPC_MESSAGE_HANDLER(WebRTCEventLogMsg_RegisterEventLogConsumer,
                          OnRegisterEventLogConsumer)
      IPC_MESSAGE_HANDLER(AecDumpMsg_UnregisterAecDumpConsumer,
                          OnUnregisterAecDumpConsumer)
      IPC_MESSAGE_HANDLER(WebRTCEventLogMsg_UnregisterEventLogConsumer,
                          OnUnregisterEventLogConsumer)
#endif
    // Adding single handlers for your service here is fine, but once your
    // service needs more than one handler, please extract them into a new
    // message filter and add that filter to CreateMessageFilters().
    IPC_END_MESSAGE_MAP()

    return true;
  }

  // Dispatch incoming messages to the appropriate IPC::Listener.
  IPC::Listener* listener = listeners_.Lookup(msg.routing_id());
  if (!listener) {
    if (msg.is_sync()) {
      // The listener has gone away, so we must respond or else the caller will
      // hang waiting for a reply.
      IPC::Message* reply = IPC::SyncMessage::GenerateReply(&msg);
      reply->set_reply_error();
      Send(reply);
    }
    return true;
  }
  return listener->OnMessageReceived(msg);
}

void RenderProcessHostImpl::OnChannelConnected(int32_t peer_pid) {
  channel_connected_ = true;
  if (IsReady()) {
    DCHECK(!sent_render_process_ready_);
    sent_render_process_ready_ = true;
    // Send RenderProcessReady only if we already received the process handle.
    FOR_EACH_OBSERVER(RenderProcessHostObserver,
                      observers_,
                      RenderProcessReady(this));
  }

#if defined(IPC_MESSAGE_LOG_ENABLED)
  Send(new ChildProcessMsg_SetIPCLoggingEnabled(
      IPC::Logging::GetInstance()->Enabled()));
#endif

  tracked_objects::ThreadData::Status status =
      tracked_objects::ThreadData::status();
  Send(new ChildProcessMsg_SetProfilerStatus(status));

  // Inform AudioInputRendererHost about the new render process PID.
  // AudioInputRendererHost is reference counted, so its lifetime is
  // guaranteed during the lifetime of the closure.
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&AudioInputRendererHost::set_renderer_pid,
                                     audio_input_renderer_host_, peer_pid));
}

void RenderProcessHostImpl::OnChannelError() {
  ProcessDied(true /* already_dead */, nullptr);
}

void RenderProcessHostImpl::OnBadMessageReceived(const IPC::Message& message) {
  // Message de-serialization failed. We consider this a capital crime. Kill the
  // renderer if we have one.
  auto type = message.type();
  LOG(ERROR) << "bad message " << type << " terminating renderer.";

  // The ReceivedBadMessage call below will trigger a DumpWithoutCrashing. Alias
  // enough information here so that we can determine what the bad message was.
  base::debug::Alias(&type);

  bad_message::ReceivedBadMessage(this,
                                  bad_message::RPH_DESERIALIZATION_FAILED);
}

BrowserContext* RenderProcessHostImpl::GetBrowserContext() const {
  return browser_context_;
}

bool RenderProcessHostImpl::InSameStoragePartition(
    StoragePartition* partition) const {
  return storage_partition_impl_ == partition;
}

int RenderProcessHostImpl::GetID() const {
  return id_;
}

bool RenderProcessHostImpl::HasConnection() const {
  return channel_.get() != NULL;
}

void RenderProcessHostImpl::SetIgnoreInputEvents(bool ignore_input_events) {
  ignore_input_events_ = ignore_input_events;
}

bool RenderProcessHostImpl::IgnoreInputEvents() const {
  return ignore_input_events_;
}

void RenderProcessHostImpl::Cleanup() {
  // Keep the one renderer thread around forever in single process mode.
  if (run_renderer_in_process())
    return;

  // If within_process_died_observer_ is true, one of our observers performed an
  // action that caused us to die (e.g. http://crbug.com/339504). Therefore,
  // delay the destruction until all of the observer callbacks have been made,
  // and guarantee that the RenderProcessHostDestroyed observer callback is
  // always the last callback fired.
  if (within_process_died_observer_) {
    delayed_cleanup_needed_ = true;
    return;
  }
  delayed_cleanup_needed_ = false;

  // Records the time when the process starts surviving for workers for UMA.
  if (listeners_.IsEmpty() && worker_ref_count_ > 0 &&
      survive_for_worker_start_time_.is_null()) {
    survive_for_worker_start_time_ = base::TimeTicks::Now();
  }

#if defined(ENABLE_WEBRTC)
  if (is_initialized_)
    ClearWebRtcLogMessageCallback();
#endif

  // When there are no other owners of this object, we can delete ourselves.
  if (listeners_.IsEmpty() && worker_ref_count_ == 0) {
    if (!survive_for_worker_start_time_.is_null()) {
      UMA_HISTOGRAM_LONG_TIMES(
          "SharedWorker.RendererSurviveForWorkerTime",
          base::TimeTicks::Now() - survive_for_worker_start_time_);
    }

    if (max_worker_count_ > 0) {
      // Record the max number of workers (SharedWorker or ServiceWorker)
      // that are simultaneously hosted in this renderer process.
      UMA_HISTOGRAM_COUNTS("Render.Workers.MaxWorkerCountInRendererProcess",
                           max_worker_count_);
    }

    // We cannot clean up twice; if this fails, there is an issue with our
    // control flow.
    DCHECK(!deleting_soon_);

    DCHECK_EQ(0, pending_views_);

    // If |channel_| is still valid, the process associated with this
    // RenderProcessHost is still alive. Notify all observers that the process
    // has exited cleanly, even though it will be destroyed a bit later.
    // Observers shouldn't rely on this process anymore.
    if (channel_.get()) {
      FOR_EACH_OBSERVER(
          RenderProcessHostObserver, observers_,
          RenderProcessExited(this, base::TERMINATION_STATUS_NORMAL_TERMINATION,
                              0));
    }
    FOR_EACH_OBSERVER(RenderProcessHostObserver, observers_,
                      RenderProcessHostDestroyed(this));
    NotificationService::current()->Notify(
        NOTIFICATION_RENDERER_PROCESS_TERMINATED,
        Source<RenderProcessHost>(this), NotificationService::NoDetails());

#ifndef NDEBUG
    is_self_deleted_ = true;
#endif
    base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
    deleting_soon_ = true;

#if USE_ATTACHMENT_BROKER
    IPC::AttachmentBroker::GetGlobal()->DeregisterCommunicationChannel(
        channel_.get());
#endif

    // It's important not to wait for the DeleteTask to delete the channel
    // proxy. Kill it off now. That way, in case the profile is going away, the
    // rest of the objects attached to this RenderProcessHost start going
    // away first, since deleting the channel proxy will post a
    // OnChannelClosed() to IPC::ChannelProxy::Context on the IO thread.
    channel_.reset();

    // The following members should be cleared in ProcessDied() as well!
    message_port_message_filter_ = NULL;

    RemoveUserData(kSessionStorageHolderKey);

    // Remove ourself from the list of renderer processes so that we can't be
    // reused in between now and when the Delete task runs.
    UnregisterHost(GetID());
  }
}

void RenderProcessHostImpl::AddPendingView() {
  pending_views_++;
}

void RenderProcessHostImpl::RemovePendingView() {
  DCHECK(pending_views_);
  pending_views_--;
}

void RenderProcessHostImpl::SetSuddenTerminationAllowed(bool enabled) {
  sudden_termination_allowed_ = enabled;
}

bool RenderProcessHostImpl::SuddenTerminationAllowed() const {
  return sudden_termination_allowed_;
}

base::TimeDelta RenderProcessHostImpl::GetChildProcessIdleTime() const {
  return base::TimeTicks::Now() - child_process_activity_time_;
}

void RenderProcessHostImpl::FilterURL(bool empty_allowed, GURL* url) {
  FilterURL(this, empty_allowed, url);
}

#if defined(ENABLE_WEBRTC)
void RenderProcessHostImpl::EnableAudioDebugRecordings(
    const base::FilePath& file) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Enable AEC dump for each registered consumer.
  base::FilePath file_with_extensions = GetAecDumpFilePathWithExtensions(file);
  for (std::vector<int>::iterator it = aec_dump_consumers_.begin();
       it != aec_dump_consumers_.end(); ++it) {
    EnableAecDumpForId(file_with_extensions, *it);
  }

  // Enable mic input recording. AudioInputRendererHost is reference counted, so
  // its lifetime is guaranteed during the lifetime of the closure.
  if (audio_input_renderer_host_) {
    // Not null if RenderProcessHostImpl::Init has already been called.
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&AudioInputRendererHost::EnableDebugRecording,
                   audio_input_renderer_host_, file));
  }
}

void RenderProcessHostImpl::DisableAudioDebugRecordings() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Posting on the FILE thread and then replying back on the UI thread is only
  // for avoiding races between enable and disable. Nothing is done on the FILE
  // thread.
  BrowserThread::PostTaskAndReply(
      BrowserThread::FILE, FROM_HERE, base::Bind(&base::DoNothing),
      base::Bind(&RenderProcessHostImpl::SendDisableAecDumpToRenderer,
                 weak_factory_.GetWeakPtr()));

  // AudioInputRendererHost is reference counted, so it's lifetime is
  // guaranteed during the lifetime of the closure.
  if (audio_input_renderer_host_) {
    // Not null if RenderProcessHostImpl::Init has already been called.
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&AudioInputRendererHost::DisableDebugRecording,
                   audio_input_renderer_host_));
  }
}

void RenderProcessHostImpl::EnableEventLogRecordings(
    const base::FilePath& file) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Enable Event log for each registered consumer.
  base::FilePath file_with_extensions = GetEventLogFilePathWithExtensions(file);
  for (int id : aec_dump_consumers_)
    EnableEventLogForId(file_with_extensions, id);
}

void RenderProcessHostImpl::DisableEventLogRecordings() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Posting on the FILE thread and then replying back on the UI thread is only
  // for avoiding races between enable and disable. Nothing is done on the FILE
  // thread.
  BrowserThread::PostTaskAndReply(
      BrowserThread::FILE, FROM_HERE, base::Bind(&base::DoNothing),
      base::Bind(&RenderProcessHostImpl::SendDisableEventLogToRenderer,
                 weak_factory_.GetWeakPtr()));
}

void RenderProcessHostImpl::SetWebRtcLogMessageCallback(
    base::Callback<void(const std::string&)> callback) {
#if defined(ENABLE_WEBRTC)
  BrowserMainLoop::GetInstance()->media_stream_manager()->
      RegisterNativeLogCallback(GetID(), callback);
#endif
}

void RenderProcessHostImpl::ClearWebRtcLogMessageCallback() {
#if defined(ENABLE_WEBRTC)
  BrowserMainLoop::GetInstance()
      ->media_stream_manager()
      ->UnregisterNativeLogCallback(GetID());
#endif
}

RenderProcessHostImpl::WebRtcStopRtpDumpCallback
RenderProcessHostImpl::StartRtpDump(
    bool incoming,
    bool outgoing,
    const WebRtcRtpPacketCallback& packet_callback) {
  if (!p2p_socket_dispatcher_host_.get())
    return WebRtcStopRtpDumpCallback();

  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&P2PSocketDispatcherHost::StartRtpDump,
                                     p2p_socket_dispatcher_host_, incoming,
                                     outgoing, packet_callback));

  if (stop_rtp_dump_callback_.is_null()) {
    stop_rtp_dump_callback_ =
        base::Bind(&P2PSocketDispatcherHost::StopRtpDumpOnUIThread,
                   p2p_socket_dispatcher_host_);
  }
  return stop_rtp_dump_callback_;
}
#endif

IPC::ChannelProxy* RenderProcessHostImpl::GetChannel() {
  return channel_.get();
}

void RenderProcessHostImpl::AddFilter(BrowserMessageFilter* filter) {
  channel_->AddFilter(filter->GetFilter());
}

bool RenderProcessHostImpl::FastShutdownForPageCount(size_t count) {
  if (GetActiveViewCount() == count)
    return FastShutdownIfPossible();
  return false;
}

bool RenderProcessHostImpl::FastShutdownStarted() const {
  return fast_shutdown_started_;
}

// static
void RenderProcessHostImpl::RegisterHost(int host_id, RenderProcessHost* host) {
  g_all_hosts.Get().AddWithID(host, host_id);
}

// static
void RenderProcessHostImpl::UnregisterHost(int host_id) {
  RenderProcessHost* host = g_all_hosts.Get().Lookup(host_id);
  if (!host)
    return;

  g_all_hosts.Get().Remove(host_id);

  // Look up the map of site to process for the given browser_context,
  // in case we need to remove this process from it.  It will be registered
  // under any sites it rendered that use process-per-site mode.
  SiteProcessMap* map =
      GetSiteProcessMapForBrowserContext(host->GetBrowserContext());
  map->RemoveProcess(host);
}

// static
void RenderProcessHostImpl::FilterURL(RenderProcessHost* rph,
                                      bool empty_allowed,
                                      GURL* url) {
  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();

  if (empty_allowed && url->is_empty())
    return;

  if (!url->is_valid()) {
    // Have to use about:blank for the denied case, instead of an empty GURL.
    // This is because the browser treats navigation to an empty GURL as a
    // navigation to the home page. This is often a privileged page
    // (chrome://newtab/) which is exactly what we don't want.
    *url = GURL(url::kAboutBlankURL);
    return;
  }

  if (url->SchemeIs(url::kAboutScheme)) {
    // The renderer treats all URLs in the about: scheme as being about:blank.
    // Canonicalize about: URLs to about:blank.
    *url = GURL(url::kAboutBlankURL);
  }

  // Do not allow browser plugin guests to navigate to non-web URLs, since they
  // cannot swap processes or grant bindings.
  bool non_web_url_in_guest =
      rph->IsForGuestsOnly() &&
      !(url->is_valid() && policy->IsWebSafeScheme(url->scheme()));

  if (non_web_url_in_guest || !policy->CanRequestURL(rph->GetID(), *url)) {
    // If this renderer is not permitted to request this URL, we invalidate the
    // URL.  This prevents us from storing the blocked URL and becoming confused
    // later.
    VLOG(1) << "Blocked URL " << url->spec();
    *url = GURL(url::kAboutBlankURL);
  }
}

// static
bool RenderProcessHostImpl::IsSuitableHost(RenderProcessHost* host,
                                           BrowserContext* browser_context,
                                           const GURL& site_url) {
  if (run_renderer_in_process()) {
    DCHECK_EQ(host->GetBrowserContext(), browser_context)
        << " Single-process mode does not support multiple browser contexts.";
    return true;
  }

  if (host->GetBrowserContext() != browser_context)
    return false;

  // Do not allow sharing of guest hosts. This is to prevent bugs where guest
  // and non-guest storage gets mixed. In the future, we might consider enabling
  // the sharing of guests, in this case this check should be removed and
  // InSameStoragePartition should handle the possible sharing.
  if (host->IsForGuestsOnly())
    return false;

  // Check whether the given host and the intended site_url will be using the
  // same StoragePartition, since a RenderProcessHost can only support a single
  // StoragePartition.  This is relevant for packaged apps.
  StoragePartition* dest_partition =
      BrowserContext::GetStoragePartitionForSite(browser_context, site_url);
  if (!host->InSameStoragePartition(dest_partition))
    return false;

  // TODO(nick): Consult the SiteIsolationPolicy here. https://crbug.com/513036
  if (ChildProcessSecurityPolicyImpl::GetInstance()->HasWebUIBindings(
          host->GetID()) !=
      WebUIControllerFactoryRegistry::GetInstance()->UseWebUIBindingsForURL(
          browser_context, site_url)) {
    return false;
  }

  return GetContentClient()->browser()->IsSuitableHost(host, site_url);
}

// static
bool RenderProcessHost::run_renderer_in_process() {
  return g_run_renderer_in_process_;
}

// static
void RenderProcessHost::SetRunRendererInProcess(bool value) {
  g_run_renderer_in_process_ = value;

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (value) {
    if (!command_line->HasSwitch(switches::kLang)) {
      // Modify the current process' command line to include the browser locale,
      // as the renderer expects this flag to be set.
      const std::string locale =
          GetContentClient()->browser()->GetApplicationLocale();
      command_line->AppendSwitchASCII(switches::kLang, locale);
    }
    // TODO(piman): we should really send configuration through bools rather
    // than by parsing strings, i.e. sending an IPC rather than command line
    // args. crbug.com/314909
    AppendCompositorCommandLineFlags(command_line);
  }
}

// static
RenderProcessHost::iterator RenderProcessHost::AllHostsIterator() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return iterator(g_all_hosts.Pointer());
}

// static
RenderProcessHost* RenderProcessHost::FromID(int render_process_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return g_all_hosts.Get().Lookup(render_process_id);
}

// static
bool RenderProcessHost::ShouldTryToUseExistingProcessHost(
    BrowserContext* browser_context,
    const GURL& url) {
  // This needs to be checked first to ensure that --single-process
  // and --site-per-process can be used together.
  if (run_renderer_in_process())
    return true;

  // If --site-per-process is enabled, do not try to reuse renderer processes
  // when over the limit.
  // TODO(nick): This is overly conservative and isn't launchable. Move this
  // logic into IsSuitableHost, and check |url| against the URL the process is
  // dedicated to. This will allow pages from the same site to share, and will
  // also allow non-isolated sites to share processes. https://crbug.com/513036
  if (SiteIsolationPolicy::UseDedicatedProcessesForAllSites())
    return false;

  // NOTE: Sometimes it's necessary to create more render processes than
  //       GetMaxRendererProcessCount(), for instance when we want to create
  //       a renderer process for a browser context that has no existing
  //       renderers. This is OK in moderation, since the
  //       GetMaxRendererProcessCount() is conservative.
  if (g_all_hosts.Get().size() >= GetMaxRendererProcessCount())
    return true;

  return GetContentClient()->browser()->ShouldTryToUseExistingProcessHost(
      browser_context, url);
}

// static
RenderProcessHost* RenderProcessHost::GetExistingProcessHost(
    BrowserContext* browser_context,
    const GURL& site_url) {
  // First figure out which existing renderers we can use.
  std::vector<RenderProcessHost*> suitable_renderers;
  suitable_renderers.reserve(g_all_hosts.Get().size());

  iterator iter(AllHostsIterator());
  while (!iter.IsAtEnd()) {
    if (GetContentClient()->browser()->MayReuseHost(iter.GetCurrentValue()) &&
        RenderProcessHostImpl::IsSuitableHost(iter.GetCurrentValue(),
                                              browser_context, site_url)) {
      suitable_renderers.push_back(iter.GetCurrentValue());
    }
    iter.Advance();
  }

  // Now pick a random suitable renderer, if we have any.
  if (!suitable_renderers.empty()) {
    int suitable_count = static_cast<int>(suitable_renderers.size());
    int random_index = base::RandInt(0, suitable_count - 1);
    return suitable_renderers[random_index];
  }

  return NULL;
}

// static
bool RenderProcessHost::ShouldUseProcessPerSite(BrowserContext* browser_context,
                                                const GURL& url) {
  // Returns true if we should use the process-per-site model.  This will be
  // the case if the --process-per-site switch is specified, or in
  // process-per-site-instance for particular sites (e.g., WebUI).
  // Note that --single-process is handled in ShouldTryToUseExistingProcessHost.
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kProcessPerSite))
    return true;

  // We want to consolidate particular sites like WebUI even when we are using
  // the process-per-tab or process-per-site-instance models.
  // Note: DevTools pages have WebUI type but should not reuse the same host.
  if (WebUIControllerFactoryRegistry::GetInstance()->UseWebUIForURL(
          browser_context, url) &&
      !url.SchemeIs(kChromeDevToolsScheme)) {
    return true;
  }

  // Otherwise let the content client decide, defaulting to false.
  return GetContentClient()->browser()->ShouldUseProcessPerSite(browser_context,
                                                                url);
}

// static
RenderProcessHost* RenderProcessHostImpl::GetProcessHostForSite(
    BrowserContext* browser_context,
    const GURL& url) {
  // Look up the map of site to process for the given browser_context.
  SiteProcessMap* map = GetSiteProcessMapForBrowserContext(browser_context);

  // See if we have an existing process with appropriate bindings for this site.
  // If not, the caller should create a new process and register it.
  std::string site =
      SiteInstance::GetSiteForURL(browser_context, url).possibly_invalid_spec();
  RenderProcessHost* host = map->FindProcess(site);
  if (host && (!GetContentClient()->browser()->MayReuseHost(host) ||
               !IsSuitableHost(host, browser_context, url))) {
    // The registered process does not have an appropriate set of bindings for
    // the url.  Remove it from the map so we can register a better one.
    RecordAction(
        base::UserMetricsAction("BindingsMismatch_GetProcessHostPerSite"));
    map->RemoveProcess(host);
    host = NULL;
  }

  return host;
}

void RenderProcessHostImpl::RegisterProcessHostForSite(
    BrowserContext* browser_context,
    RenderProcessHost* process,
    const GURL& url) {
  // Look up the map of site to process for the given browser_context.
  SiteProcessMap* map = GetSiteProcessMapForBrowserContext(browser_context);

  // Only register valid, non-empty sites.  Empty or invalid sites will not
  // use process-per-site mode.  We cannot check whether the process has
  // appropriate bindings here, because the bindings have not yet been granted.
  std::string site =
      SiteInstance::GetSiteForURL(browser_context, url).possibly_invalid_spec();
  if (!site.empty())
    map->RegisterProcess(site, process);
}

void RenderProcessHostImpl::CreateSharedRendererHistogramAllocator() {
  DCHECK(!metrics_allocator_);

  // Create a persistent memory segment for renderer histograms only if
  // they're active in the browser.
  if (!base::GlobalHistogramAllocator::Get())
    return;

  // Get handle to the renderer process. Stop if there is none.
  base::ProcessHandle destination = GetHandle();
  if (destination == base::kNullProcessHandle)
    return;

  // TODO(bcwhite): Update this with the correct memory size.
  std::unique_ptr<base::SharedMemory> shm(new base::SharedMemory());
  if (!shm->CreateAndMapAnonymous(2 << 20))  // 2 MiB
    return;
  metrics_allocator_.reset(new base::SharedPersistentMemoryAllocator(
      std::move(shm), GetID(), "RendererMetrics", /*readonly=*/false));

  base::SharedMemoryHandle shm_handle;
  metrics_allocator_->shared_memory()->ShareToProcess(destination, &shm_handle);
  Send(new ChildProcessMsg_SetHistogramMemory(
      shm_handle, metrics_allocator_->shared_memory()->mapped_size()));
}

void RenderProcessHostImpl::ProcessDied(bool already_dead,
                                        RendererClosedDetails* known_details) {
  // Our child process has died.  If we didn't expect it, it's a crash.
  // In any case, we need to let everyone know it's gone.
  // The OnChannelError notification can fire multiple times due to nested sync
  // calls to a renderer. If we don't have a valid channel here it means we
  // already handled the error.

  // It should not be possible for us to be called re-entrantly.
  DCHECK(!within_process_died_observer_);

  // It should not be possible for a process death notification to come in while
  // we are dying.
  DCHECK(!deleting_soon_);

  // child_process_launcher_ can be NULL in single process mode or if fast
  // termination happened.
  base::TerminationStatus status = base::TERMINATION_STATUS_NORMAL_TERMINATION;
  int exit_code = 0;
  if (known_details) {
    status = known_details->status;
    exit_code = known_details->exit_code;
  } else if (child_process_launcher_.get()) {
    status = child_process_launcher_->GetChildTerminationStatus(already_dead,
                                                                &exit_code);
    if (already_dead && status == base::TERMINATION_STATUS_STILL_RUNNING) {
      // May be in case of IPC error, if it takes long time for renderer
      // to exit. Child process will be killed in any case during
      // child_process_launcher_.reset(). Make sure we will not broadcast
      // FrameHostMsg_RenderProcessGone with status
      // TERMINATION_STATUS_STILL_RUNNING, since this will break WebContentsImpl
      // logic.
      status = base::TERMINATION_STATUS_PROCESS_CRASHED;
    }
  }

  RendererClosedDetails details(status, exit_code);

  child_process_launcher_.reset();
#if USE_ATTACHMENT_BROKER
  IPC::AttachmentBroker::GetGlobal()->DeregisterCommunicationChannel(
      channel_.get());
#endif
  channel_.reset();
  while (!queued_messages_.empty()) {
    delete queued_messages_.front();
    queued_messages_.pop();
  }
  UpdateProcessPriority();
  DCHECK(!is_process_backgrounded_);

  // RenderProcessExited observers and RenderProcessGone handlers might
  // navigate or perform other actions that require a connection. Ensure that
  // there is one before calling them.
  child_token_ = mojo::edk::GenerateRandomToken();
  shell::Connector* connector =
      BrowserContext::GetShellConnectorFor(browser_context_);
  if (!connector)
    connector = MojoShellConnection::GetForProcess()->GetConnector();
  mojo_child_connection_.reset(new MojoChildConnection(
      kRendererMojoApplicationName,
      base::StringPrintf("%d_%d", id_, instance_id_++),
      child_token_,
      connector));

  within_process_died_observer_ = true;
  NotificationService::current()->Notify(
      NOTIFICATION_RENDERER_PROCESS_CLOSED, Source<RenderProcessHost>(this),
      Details<RendererClosedDetails>(&details));
  FOR_EACH_OBSERVER(RenderProcessHostObserver, observers_,
                    RenderProcessExited(this, status, exit_code));
  within_process_died_observer_ = false;

  message_port_message_filter_ = NULL;
  RemoveUserData(kSessionStorageHolderKey);

  IDMap<IPC::Listener>::iterator iter(&listeners_);
  while (!iter.IsAtEnd()) {
    iter.GetCurrentValue()->OnMessageReceived(FrameHostMsg_RenderProcessGone(
        iter.GetCurrentKey(), static_cast<int>(status), exit_code));
    iter.Advance();
  }

  // It's possible that one of the calls out to the observers might have caused
  // this object to be no longer needed.
  if (delayed_cleanup_needed_)
    Cleanup();

  // This object is not deleted at this point and might be reused later.
  // TODO(darin): clean this up
}

size_t RenderProcessHost::GetActiveViewCount() {
  size_t num_active_views = 0;
  std::unique_ptr<RenderWidgetHostIterator> widgets(
      RenderWidgetHost::GetRenderWidgetHosts());
  while (RenderWidgetHost* widget = widgets->GetNextHost()) {
    // Count only RenderWidgetHosts in this process.
    if (widget->GetProcess()->GetID() == GetID())
      num_active_views++;
  }
  return num_active_views;
}

void RenderProcessHostImpl::ReleaseOnCloseACK(
    RenderProcessHost* host,
    const SessionStorageNamespaceMap& sessions,
    int view_route_id) {
  DCHECK(host);
  if (sessions.empty())
    return;
  SessionStorageHolder* holder = static_cast<SessionStorageHolder*>(
      host->GetUserData(kSessionStorageHolderKey));
  if (!holder) {
    holder = new SessionStorageHolder();
    host->SetUserData(kSessionStorageHolderKey, holder);
  }
  holder->Hold(sessions, view_route_id);
}

void RenderProcessHostImpl::OnShutdownRequest() {
  // Don't shut down if there are active RenderViews, or if there are pending
  // RenderViews being swapped back in.
  // In single process mode, we never shutdown the renderer.
  if (pending_views_ || run_renderer_in_process() || GetActiveViewCount() > 0)
    return;

  // Notify any contents that might have swapped out renderers from this
  // process. They should not attempt to swap them back in.
  FOR_EACH_OBSERVER(RenderProcessHostObserver, observers_,
                    RenderProcessWillExit(this));

  Send(new ChildProcessMsg_Shutdown());
}

void RenderProcessHostImpl::SuddenTerminationChanged(bool enabled) {
  SetSuddenTerminationAllowed(enabled);
}

void RenderProcessHostImpl::UpdateProcessPriority() {
  if (!child_process_launcher_.get() || child_process_launcher_->IsStarting()) {
    is_process_backgrounded_ = false;
    return;
  }

  // We background a process as soon as it hosts no active audio streams and no
  // visible widgets -- the callers must call this function whenever we
  // transition in/out of those states.
  const bool should_background =
      visible_widgets_ == 0 && !audio_renderer_host_->HasActiveAudio() &&
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableRendererBackgrounding);

// TODO(sebsg): Remove this ifdef when https://crbug.com/537671 is fixed.
#if !defined(OS_ANDROID)
  if (is_process_backgrounded_ == should_background)
    return;
#endif

  TRACE_EVENT1("renderer_host", "RenderProcessHostImpl::UpdateProcessPriority",
               "should_background", should_background);
  is_process_backgrounded_ = should_background;

#if defined(OS_WIN)
  // The cbstext.dll loads as a global GetMessage hook in the browser process
  // and intercepts/unintercepts the kernel32 API SetPriorityClass in a
  // background thread. If the UI thread invokes this API just when it is
  // intercepted the stack is messed up on return from the interceptor
  // which causes random crashes in the browser process. Our hack for now
  // is to not invoke the SetPriorityClass API if the dll is loaded.
  if (GetModuleHandle(L"cbstext.dll"))
    return;
#endif  // OS_WIN

  // Control the background state from the browser process, otherwise the task
  // telling the renderer to "unbackground" itself may be preempted by other
  // tasks executing at lowered priority ahead of it or simply by not being
  // swiftly scheduled by the OS per the low process priority
  // (http://crbug.com/398103).
  child_process_launcher_->SetProcessBackgrounded(should_background);

  // Notify the child process of background state.
  Send(new ChildProcessMsg_SetProcessBackgrounded(should_background));
}

void RenderProcessHostImpl::OnProcessLaunched() {
  // No point doing anything, since this object will be destructed soon.  We
  // especially don't want to send the RENDERER_PROCESS_CREATED notification,
  // since some clients might expect a RENDERER_PROCESS_TERMINATED afterwards to
  // properly cleanup.
  if (deleting_soon_)
    return;

  if (child_process_launcher_) {
    DCHECK(child_process_launcher_->GetProcess().IsValid());
    DCHECK(!is_process_backgrounded_);

    if (mojo_child_connection_) {
      mojo_child_connection_->SetProcessHandle(
          child_process_launcher_->GetProcess().Handle());
    }

    // Not all platforms launch processes in the same backgrounded state. Make
    // sure |is_process_backgrounded_| reflects this platform's initial process
    // state.
    is_process_backgrounded_ =
        child_process_launcher_->GetProcess().IsProcessBackgrounded();

    // Disable updating process priority on startup for now as it incorrectly
    // results in backgrounding foreground navigations until their first commit
    // is made. A better long term solution would be to be aware of the tab's
    // visibility at this point. https://crbug.com/560446.
    // Except on Android for now because of https://crbug.com/601184 :-(.
#if defined(OS_ANDROID)
    UpdateProcessPriority();
#endif

    // Share histograms between the renderer and this process.
    CreateSharedRendererHistogramAllocator();
  }

  // NOTE: This needs to be before sending queued messages because
  // ExtensionService uses this notification to initialize the renderer process
  // with state that must be there before any JavaScript executes.
  //
  // The queued messages contain such things as "navigate". If this notification
  // was after, we can end up executing JavaScript before the initialization
  // happens.
  NotificationService::current()->Notify(NOTIFICATION_RENDERER_PROCESS_CREATED,
                                         Source<RenderProcessHost>(this),
                                         NotificationService::NoDetails());

  while (!queued_messages_.empty()) {
    Send(queued_messages_.front());
    queued_messages_.pop();
  }

  if (IsReady()) {
    DCHECK(!sent_render_process_ready_);
    sent_render_process_ready_ = true;
    // Send RenderProcessReady only if the channel is already connected.
    FOR_EACH_OBSERVER(RenderProcessHostObserver,
                      observers_,
                      RenderProcessReady(this));
  }

#if defined(ENABLE_WEBRTC)
  if (WebRTCInternals::GetInstance()->IsAudioDebugRecordingsEnabled()) {
    EnableAudioDebugRecordings(
        WebRTCInternals::GetInstance()->GetAudioDebugRecordingsFilePath());
  }
#endif
}

void RenderProcessHostImpl::OnProcessLaunchFailed(int error_code) {
  // If this object will be destructed soon, then observers have already been
  // sent a RenderProcessHostDestroyed notification, and we must observe our
  // contract that says that will be the last call.
  if (deleting_soon_)
    return;

  RendererClosedDetails details{base::TERMINATION_STATUS_LAUNCH_FAILED,
                                error_code};
  ProcessDied(true, &details);
}

scoped_refptr<AudioRendererHost> RenderProcessHostImpl::audio_renderer_host()
    const {
  return audio_renderer_host_;
}

void RenderProcessHostImpl::OnUserMetricsRecordAction(
    const std::string& action) {
  RecordComputedAction(action);
}

void RenderProcessHostImpl::OnCloseACK(int old_route_id) {
  SessionStorageHolder* holder =
      static_cast<SessionStorageHolder*>(GetUserData(kSessionStorageHolderKey));
  if (!holder)
    return;
  holder->Release(old_route_id);
}

void RenderProcessHostImpl::OnGpuSwitched() {
  RecomputeAndUpdateWebKitPreferences();
}

#if defined(ENABLE_WEBRTC)
void RenderProcessHostImpl::OnRegisterAecDumpConsumer(int id) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&RenderProcessHostImpl::RegisterAecDumpConsumerOnUIThread,
                 weak_factory_.GetWeakPtr(), id));
}

void RenderProcessHostImpl::OnRegisterEventLogConsumer(int id) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&RenderProcessHostImpl::RegisterEventLogConsumerOnUIThread,
                 weak_factory_.GetWeakPtr(), id));
}

void RenderProcessHostImpl::OnUnregisterAecDumpConsumer(int id) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&RenderProcessHostImpl::UnregisterAecDumpConsumerOnUIThread,
                 weak_factory_.GetWeakPtr(), id));
}

void RenderProcessHostImpl::OnUnregisterEventLogConsumer(int id) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&RenderProcessHostImpl::UnregisterEventLogConsumerOnUIThread,
                 weak_factory_.GetWeakPtr(), id));
}

void RenderProcessHostImpl::RegisterAecDumpConsumerOnUIThread(int id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  aec_dump_consumers_.push_back(id);

  if (WebRTCInternals::GetInstance()->IsAudioDebugRecordingsEnabled()) {
    base::FilePath file_with_extensions = GetAecDumpFilePathWithExtensions(
        WebRTCInternals::GetInstance()->GetAudioDebugRecordingsFilePath());
    EnableAecDumpForId(file_with_extensions, id);
  }
}

void RenderProcessHostImpl::RegisterEventLogConsumerOnUIThread(int id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  aec_dump_consumers_.push_back(id);

  if (WebRTCInternals::GetInstance()->IsEventLogRecordingsEnabled()) {
    base::FilePath file_with_extensions = GetEventLogFilePathWithExtensions(
        WebRTCInternals::GetInstance()->GetEventLogRecordingsFilePath());
    EnableEventLogForId(file_with_extensions, id);
  }
}

void RenderProcessHostImpl::UnregisterAecDumpConsumerOnUIThread(int id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  for (std::vector<int>::iterator it = aec_dump_consumers_.begin();
       it != aec_dump_consumers_.end(); ++it) {
    if (*it == id) {
      aec_dump_consumers_.erase(it);
      break;
    }
  }
}

void RenderProcessHostImpl::UnregisterEventLogConsumerOnUIThread(int id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  for (std::vector<int>::iterator it = aec_dump_consumers_.begin();
       it != aec_dump_consumers_.end(); ++it) {
    if (*it == id) {
      aec_dump_consumers_.erase(it);
      break;
    }
  }
}

void RenderProcessHostImpl::EnableAecDumpForId(const base::FilePath& file,
                                               int id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&CreateFileForProcess, file.AddExtension(IntToStringType(id))),
      base::Bind(&RenderProcessHostImpl::SendAecDumpFileToRenderer,
                 weak_factory_.GetWeakPtr(), id));
}

void RenderProcessHostImpl::EnableEventLogForId(const base::FilePath& file,
                                                int id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&CreateFileForProcess, file.AddExtension(IntToStringType(id))),
      base::Bind(&RenderProcessHostImpl::SendEventLogFileToRenderer,
                 weak_factory_.GetWeakPtr(), id));
}

void RenderProcessHostImpl::SendAecDumpFileToRenderer(
    int id,
    IPC::PlatformFileForTransit file_for_transit) {
  if (file_for_transit == IPC::InvalidPlatformFileForTransit())
    return;
  Send(new AecDumpMsg_EnableAecDump(id, file_for_transit));
}

void RenderProcessHostImpl::SendEventLogFileToRenderer(
    int id,
    IPC::PlatformFileForTransit file_for_transit) {
  if (file_for_transit == IPC::InvalidPlatformFileForTransit())
    return;
  Send(new WebRTCEventLogMsg_EnableEventLog(id, file_for_transit));
}

void RenderProcessHostImpl::SendDisableAecDumpToRenderer() {
  Send(new AecDumpMsg_DisableAecDump());
}

void RenderProcessHostImpl::SendDisableEventLogToRenderer() {
  Send(new WebRTCEventLogMsg_DisableEventLog());
}

base::FilePath RenderProcessHostImpl::GetAecDumpFilePathWithExtensions(
    const base::FilePath& file) {
  return file.AddExtension(IntToStringType(base::GetProcId(GetHandle())))
      .AddExtension(kAecDumpFileNameAddition);
}

base::FilePath RenderProcessHostImpl::GetEventLogFilePathWithExtensions(
    const base::FilePath& file) {
  return file.AddExtension(IntToStringType(base::GetProcId(GetHandle())))
      .AddExtension(kEventLogFileNameAddition);
}
#endif  // defined(ENABLE_WEBRTC)

void RenderProcessHostImpl::GetAudioOutputControllers(
    const GetAudioOutputControllersCallback& callback) const {
  audio_renderer_host()->GetOutputControllers(callback);
}

BluetoothAdapterFactoryWrapper*
RenderProcessHostImpl::GetBluetoothAdapterFactoryWrapper() {
  return &bluetooth_adapter_factory_wrapper_;
}

void RenderProcessHostImpl::RecomputeAndUpdateWebKitPreferences() {
  // We are updating all widgets including swapped out ones.
  std::unique_ptr<RenderWidgetHostIterator> widgets(
      RenderWidgetHostImpl::GetAllRenderWidgetHosts());
  while (RenderWidgetHost* widget = widgets->GetNextHost()) {
    RenderViewHost* rvh = RenderViewHost::From(widget);
    if (!rvh)
      continue;

    // Skip widgets in other processes.
    if (rvh->GetProcess()->GetID() != GetID())
      continue;

    rvh->OnWebkitPreferencesChanged();
  }
}

// static
void RenderProcessHostImpl::OnMojoError(
    base::WeakPtr<RenderProcessHostImpl> process,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    const std::string& error) {
  if (!task_runner->BelongsToCurrentThread()) {
    task_runner->PostTask(FROM_HERE,
                          base::Bind(&RenderProcessHostImpl::OnMojoError,
                                     process, task_runner, error));
  }
  if (!process)
    return;
  LOG(ERROR) << "Terminating render process for bad Mojo message: " << error;

  // The ReceivedBadMessage call below will trigger a DumpWithoutCrashing. Alias
  // enough information here so that we can determine what the bad message was.
  base::debug::Alias(&error);
  bad_message::ReceivedBadMessage(process.get(),
                                  bad_message::RPH_MOJO_PROCESS_ERROR);
}

}  // namespace content
