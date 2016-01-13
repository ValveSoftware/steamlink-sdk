/*
 * Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#define NACL_LOG_MODULE_NAME "Plugin_ServiceRuntime"

#include "ppapi/native_client/src/trusted/plugin/service_runtime.h"

#include <string.h>
#include <set>
#include <string>
#include <utility>

#include "base/compiler_specific.h"

#include "native_client/src/include/checked_cast.h"
#include "native_client/src/include/portability_io.h"
#include "native_client/src/include/portability_string.h"
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/include/nacl_string.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/platform/nacl_sync.h"
#include "native_client/src/shared/platform/nacl_sync_checked.h"
#include "native_client/src/shared/platform/nacl_sync_raii.h"
#include "native_client/src/shared/platform/scoped_ptr_refcount.h"
#include "native_client/src/trusted/desc/nacl_desc_imc.h"
// remove when we no longer need to cast the DescWrapper below.
#include "native_client/src/trusted/desc/nacl_desc_io.h"
#include "native_client/src/trusted/desc/nrd_xfer.h"
#include "native_client/src/trusted/nonnacl_util/sel_ldr_launcher.h"

#include "native_client/src/public/imc_types.h"
#include "native_client/src/public/nacl_file_info.h"
#include "native_client/src/trusted/service_runtime/nacl_error_code.h"

#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/core.h"
#include "ppapi/cpp/completion_callback.h"

#include "ppapi/native_client/src/trusted/plugin/plugin.h"
#include "ppapi/native_client/src/trusted/plugin/plugin_error.h"
#include "ppapi/native_client/src/trusted/plugin/pnacl_resources.h"
#include "ppapi/native_client/src/trusted/plugin/sel_ldr_launcher_chrome.h"
#include "ppapi/native_client/src/trusted/plugin/srpc_client.h"
#include "ppapi/native_client/src/trusted/plugin/utility.h"
#include "ppapi/native_client/src/trusted/weak_ref/call_on_main_thread.h"

namespace plugin {

class OpenManifestEntryAsyncCallback {
 public:
  OpenManifestEntryAsyncCallback(PP_OpenResourceCompletionCallback callback,
                                 void* callback_user_data)
      : callback_(callback), callback_user_data_(callback_user_data) {
  }

  ~OpenManifestEntryAsyncCallback() {
    if (callback_)
      callback_(callback_user_data_, PP_kInvalidFileHandle);
  }

  void Run(int32_t pp_error) {
#if defined(OS_WIN)
    // Currently, this is used only for non-SFI mode, and now the mode is not
    // supported on windows.
    // TODO(hidehiko): Support it on Windows when we switch to use
    // ManifestService also in SFI-mode.
    NACL_NOTREACHED();
#elif defined(OS_POSIX)
    // On posix, PlatformFile is the file descriptor.
    callback_(callback_user_data_, (pp_error == PP_OK) ? info_.desc : -1);
    callback_ = NULL;
#endif
  }

  NaClFileInfo* mutable_info() { return &info_; }

 private:
  NaClFileInfo info_;
  PP_OpenResourceCompletionCallback callback_;
  void* callback_user_data_;
  DISALLOW_COPY_AND_ASSIGN(OpenManifestEntryAsyncCallback);
};

namespace {

class ManifestService {
 public:
  ManifestService(nacl::WeakRefAnchor* anchor,
                  PluginReverseInterface* plugin_reverse)
      : anchor_(anchor),
        plugin_reverse_(plugin_reverse) {
  }

  ~ManifestService() {
    anchor_->Unref();
  }

  bool Quit() {
    delete this;
    return false;
  }

  bool StartupInitializationComplete() {
    // Release this instance if the ServiceRuntime is already destructed.
    if (anchor_->is_abandoned()) {
      delete this;
      return false;
    }

    plugin_reverse_->StartupInitializationComplete();
    return true;
  }

  bool OpenResource(const char* entry_key,
                    PP_OpenResourceCompletionCallback callback,
                    void* callback_user_data) {
    // Release this instance if the ServiceRuntime is already destructed.
    if (anchor_->is_abandoned()) {
      callback(callback_user_data, PP_kInvalidFileHandle);
      delete this;
      return false;
    }

    OpenManifestEntryAsyncCallback* open_manifest_callback =
        new OpenManifestEntryAsyncCallback(callback, callback_user_data);
    plugin_reverse_->OpenManifestEntryAsync(
        entry_key,
        open_manifest_callback->mutable_info(),
        open_manifest_callback);
    return true;
  }

  static PP_Bool QuitTrampoline(void* user_data) {
    return PP_FromBool(static_cast<ManifestService*>(user_data)->Quit());
  }

  static PP_Bool StartupInitializationCompleteTrampoline(void* user_data) {
    return PP_FromBool(static_cast<ManifestService*>(user_data)->
                       StartupInitializationComplete());
  }

  static PP_Bool OpenResourceTrampoline(
      void* user_data,
      const char* entry_key,
      PP_OpenResourceCompletionCallback callback,
      void* callback_user_data) {
    return PP_FromBool(static_cast<ManifestService*>(user_data)->OpenResource(
        entry_key, callback, callback_user_data));
  }

 private:
  // Weak reference to check if plugin_reverse is legally accessible or not.
  nacl::WeakRefAnchor* anchor_;
  PluginReverseInterface* plugin_reverse_;

  DISALLOW_COPY_AND_ASSIGN(ManifestService);
};

// Vtable to pass functions to LaunchSelLdr.
const PPP_ManifestService kManifestServiceVTable = {
  &ManifestService::QuitTrampoline,
  &ManifestService::StartupInitializationCompleteTrampoline,
  &ManifestService::OpenResourceTrampoline,
};

}  // namespace

OpenManifestEntryResource::~OpenManifestEntryResource() {
  MaybeRunCallback(PP_ERROR_ABORTED);
}

void OpenManifestEntryResource::MaybeRunCallback(int32_t pp_error) {
  if (!callback)
    return;

  callback->Run(pp_error);
  delete callback;
  callback = NULL;
}

PluginReverseInterface::PluginReverseInterface(
    nacl::WeakRefAnchor* anchor,
    Plugin* plugin,
    ServiceRuntime* service_runtime,
    pp::CompletionCallback init_done_cb,
    pp::CompletionCallback crash_cb)
      : anchor_(anchor),
        plugin_(plugin),
        service_runtime_(service_runtime),
        shutting_down_(false),
        init_done_cb_(init_done_cb),
        crash_cb_(crash_cb) {
  NaClXMutexCtor(&mu_);
  NaClXCondVarCtor(&cv_);
}

PluginReverseInterface::~PluginReverseInterface() {
  NaClCondVarDtor(&cv_);
  NaClMutexDtor(&mu_);
}

void PluginReverseInterface::ShutDown() {
  NaClLog(4, "PluginReverseInterface::Shutdown: entered\n");
  nacl::MutexLocker take(&mu_);
  shutting_down_ = true;
  NaClXCondVarBroadcast(&cv_);
  NaClLog(4, "PluginReverseInterface::Shutdown: broadcasted, exiting\n");
}

void PluginReverseInterface::DoPostMessage(nacl::string message) {
  std::string full_message = std::string("DEBUG_POSTMESSAGE:") + message;
  GetNaClInterface()->PostMessageToJavaScript(plugin_->pp_instance(),
                                              full_message.c_str());
}

void PluginReverseInterface::StartupInitializationComplete() {
  NaClLog(4, "PluginReverseInterface::StartupInitializationComplete\n");
  if (init_done_cb_.pp_completion_callback().func != NULL) {
    NaClLog(4,
            "PluginReverseInterface::StartupInitializationComplete:"
            " invoking CB\n");
    pp::Module::Get()->core()->CallOnMainThread(0, init_done_cb_, PP_OK);
  } else {
    NaClLog(1,
            "PluginReverseInterface::StartupInitializationComplete:"
            " init_done_cb_ not valid, skipping.\n");
  }
}

// TODO(bsy): OpenManifestEntry should use the manifest to ResolveKey
// and invoke StreamAsFile with a completion callback that invokes
// GetPOSIXFileDesc.
bool PluginReverseInterface::OpenManifestEntry(nacl::string url_key,
                                               struct NaClFileInfo* info) {
  bool op_complete = false;  // NB: mu_ and cv_ also controls access to this!
  // The to_open object is owned by the weak ref callback. Because this function
  // waits for the callback to finish, the to_open object will be deallocated on
  // the main thread before this function can return. The pointers it contains
  // to stack variables will not leak.
  OpenManifestEntryResource* to_open =
      new OpenManifestEntryResource(url_key, info, &op_complete, NULL);
  CHECK(to_open != NULL);
  NaClLog(4, "PluginReverseInterface::OpenManifestEntry: %s\n",
          url_key.c_str());
  // This assumes we are not on the main thread.  If false, we deadlock.
  plugin::WeakRefCallOnMainThread(
      anchor_,
      0,
      this,
      &plugin::PluginReverseInterface::OpenManifestEntry_MainThreadContinuation,
      to_open);
  NaClLog(4,
          "PluginReverseInterface::OpenManifestEntry:"
          " waiting on main thread\n");

  {
    nacl::MutexLocker take(&mu_);
    while (!shutting_down_ && !op_complete)
      NaClXCondVarWait(&cv_, &mu_);
    NaClLog(4, "PluginReverseInterface::OpenManifestEntry: done!\n");
    if (shutting_down_) {
      NaClLog(4,
              "PluginReverseInterface::OpenManifestEntry:"
              " plugin is shutting down\n");
      return false;
    }
  }

  // info->desc has the returned descriptor if successful, else -1.

  // The caller is responsible for not closing info->desc.  If it is
  // closed prematurely, then another open could re-use the OS
  // descriptor, confusing the opened_ map.  If the caller is going to
  // want to make a NaClDesc object and transfer it etc., then the
  // caller should DUP the descriptor (but remember the original
  // value) for use by the NaClDesc object, which closes when the
  // object is destroyed.
  NaClLog(4,
          "PluginReverseInterface::OpenManifestEntry: info->desc = %d\n",
          info->desc);
  if (info->desc == -1) {
    // TODO(bsy,ncbray): what else should we do with the error?  This
    // is a runtime error that may simply be a programming error in
    // the untrusted code, or it may be something else wrong w/ the
    // manifest.
    NaClLog(4, "OpenManifestEntry: failed for key %s", url_key.c_str());
  }
  return true;
}

void PluginReverseInterface::OpenManifestEntryAsync(
    const nacl::string& entry_key,
    struct NaClFileInfo* info,
    OpenManifestEntryAsyncCallback* callback) {
  bool op_complete = false;
  OpenManifestEntryResource to_open(
      entry_key, info, &op_complete, callback);
  OpenManifestEntry_MainThreadContinuation(&to_open, PP_OK);
}

// Transfer point from OpenManifestEntry() which runs on the main thread
// (Some PPAPI actions -- like StreamAsFile -- can only run on the main thread).
// OpenManifestEntry() is waiting on a condvar for this continuation to
// complete.  We Broadcast and awaken OpenManifestEntry() whenever we are done
// either here, or in a later MainThreadContinuation step, if there are
// multiple steps.
void PluginReverseInterface::OpenManifestEntry_MainThreadContinuation(
    OpenManifestEntryResource* p,
    int32_t err) {
  UNREFERENCED_PARAMETER(err);
  // CallOnMainThread continuations always called with err == PP_OK.

  NaClLog(4, "Entered OpenManifestEntry_MainThreadContinuation\n");

  PP_Var pp_mapped_url;
  PP_PNaClOptions pnacl_options = {PP_FALSE, PP_FALSE, 2};
  if (!GetNaClInterface()->ManifestResolveKey(
          plugin_->pp_instance(),
          PP_FromBool(!service_runtime_->main_service_runtime()),
          p->url.c_str(),
          &pp_mapped_url,
          &pnacl_options)) {
    NaClLog(4, "OpenManifestEntry_MainThreadContinuation: ResolveKey failed\n");
    // Failed, and error_info has the details on what happened.  Wake
    // up requesting thread -- we are done.
    {
      nacl::MutexLocker take(&mu_);
      *p->op_complete_ptr = true;  // done...
      p->file_info->desc = -1;  // but failed.
      NaClXCondVarBroadcast(&cv_);
    }
    p->MaybeRunCallback(PP_OK);
    return;
  }
  nacl::string mapped_url = pp::Var(pp_mapped_url).AsString();
  NaClLog(4,
          "OpenManifestEntry_MainThreadContinuation: "
          "ResolveKey: %s -> %s (pnacl_translate(%d))\n",
          p->url.c_str(), mapped_url.c_str(), pnacl_options.translate);

  if (pnacl_options.translate) {
    // Requires PNaCl translation, but that's not supported.
    NaClLog(4,
            "OpenManifestEntry_MainThreadContinuation: "
            "Requires PNaCl translation -- not supported\n");
    {
      nacl::MutexLocker take(&mu_);
      *p->op_complete_ptr = true;  // done...
      p->file_info->desc = -1;  // but failed.
      NaClXCondVarBroadcast(&cv_);
    }
    p->MaybeRunCallback(PP_OK);
    return;
  }

  // Because p is owned by the callback of this invocation, so it is necessary
  // to create another instance.
  OpenManifestEntryResource* open_cont = new OpenManifestEntryResource(*p);
  open_cont->url = mapped_url;
  // Callback is now delegated from p to open_cont. So, here we manually clear
  // complete callback.
  p->callback = NULL;

  pp::CompletionCallback stream_cc = WeakRefNewCallback(
      anchor_,
      this,
      &PluginReverseInterface::StreamAsFile_MainThreadContinuation,
      open_cont);

  GetNaClInterface()->DownloadFile(plugin_->pp_instance(),
                                   mapped_url.c_str(),
                                   &open_cont->pp_file_info,
                                   stream_cc.pp_completion_callback());
  // p is deleted automatically.
}

void PluginReverseInterface::StreamAsFile_MainThreadContinuation(
    OpenManifestEntryResource* p,
    int32_t result) {
  NaClLog(4, "Entered StreamAsFile_MainThreadContinuation\n");
  {
    nacl::MutexLocker take(&mu_);
    if (result == PP_OK) {
      // We downloaded this file to temporary storage for this plugin; it's
      // reasonable to provide a file descriptor with write access.
      p->file_info->desc = ConvertFileDescriptor(p->pp_file_info.handle, false);
      p->file_info->file_token.lo = p->pp_file_info.token_lo;
      p->file_info->file_token.hi = p->pp_file_info.token_hi;
      NaClLog(4,
              "StreamAsFile_MainThreadContinuation: PP_OK, desc %d\n",
              p->file_info->desc);
    } else {
      NaClLog(
          4,
          "StreamAsFile_MainThreadContinuation: !PP_OK, setting desc -1\n");
      p->file_info->desc = -1;
    }
    *p->op_complete_ptr = true;
    NaClXCondVarBroadcast(&cv_);
  }
  p->MaybeRunCallback(PP_OK);
}

bool PluginReverseInterface::CloseManifestEntry(int32_t desc) {
  // We don't take any action on a call to CloseManifestEntry today, so always
  // return success.
  return true;
}

void PluginReverseInterface::ReportCrash() {
  NaClLog(4, "PluginReverseInterface::ReportCrash\n");

  if (crash_cb_.pp_completion_callback().func != NULL) {
    NaClLog(4, "PluginReverseInterface::ReportCrash: invoking CB\n");
    pp::Module::Get()->core()->CallOnMainThread(0, crash_cb_, PP_OK);
    // Clear the callback to avoid it gets invoked twice.
    crash_cb_ = pp::CompletionCallback();
  } else {
    NaClLog(1,
            "PluginReverseInterface::ReportCrash:"
            " crash_cb_ not valid, skipping\n");
  }
}

void PluginReverseInterface::ReportExitStatus(int exit_status) {
  service_runtime_->set_exit_status(exit_status);
}

int64_t PluginReverseInterface::RequestQuotaForWrite(
    nacl::string file_id, int64_t offset, int64_t bytes_to_write) {
  return bytes_to_write;
}

// Thin wrapper for the arguments of LoadNexeAndStart(), as WeakRefNewCallback
// can take only one argument. Also, this dtor has the responsibility to invoke
// callbacks on destruction.
struct ServiceRuntime::LoadNexeAndStartData {
  explicit LoadNexeAndStartData(const pp::CompletionCallback& callback)
      : callback(callback) {
  }

  ~LoadNexeAndStartData() {
    // We must call the callbacks here if they are not yet called, otherwise
    // the resource would be leaked.
    if (callback.pp_completion_callback().func)
      callback.RunAndClear(PP_ERROR_ABORTED);
  }

  // On success path, this must be invoked manually. Otherwise the dtor would
  // invoke callbacks with error code unexpectedly.
  void Clear() {
    callback = pp::CompletionCallback();
  }

  pp::CompletionCallback callback;
};

ServiceRuntime::ServiceRuntime(Plugin* plugin,
                               bool main_service_runtime,
                               bool uses_nonsfi_mode,
                               pp::CompletionCallback init_done_cb,
                               pp::CompletionCallback crash_cb)
    : plugin_(plugin),
      main_service_runtime_(main_service_runtime),
      uses_nonsfi_mode_(uses_nonsfi_mode),
      reverse_service_(NULL),
      anchor_(new nacl::WeakRefAnchor()),
      rev_interface_(new PluginReverseInterface(anchor_, plugin, this,
                                                init_done_cb, crash_cb)),
      start_sel_ldr_done_(false),
      nexe_started_(false) {
  NaClSrpcChannelInitialize(&command_channel_);
  NaClXMutexCtor(&mu_);
  NaClXCondVarCtor(&cond_);
}

void ServiceRuntime::LoadNexeAndStartAfterLoadModule(
    LoadNexeAndStartData* data, int32_t pp_error) {
  if (pp_error != PP_OK) {
    DidLoadNexeAndStart(data, pp_error);
    return;
  }

  // Here, LoadModule is successfully done. So the remaining task is just
  // calling StartModule(), here.
  DidLoadNexeAndStart(data, StartModule() ? PP_OK : PP_ERROR_FAILED);
}

void ServiceRuntime::DidLoadNexeAndStart(
    LoadNexeAndStartData* data, int32_t pp_error) {
  if (pp_error == PP_OK) {
    NaClLog(4, "ServiceRuntime::LoadNexeAndStart (success)\n");
  } else {
    // On a load failure the service runtime does not crash itself to
    // avoid a race where the no-more-senders error on the reverse
    // channel esrvice thread might cause the crash-detection logic to
    // kick in before the start_module RPC reply has been received. So
    // we induce a service runtime crash here. We do not release
    // subprocess_ since it's needed to collect crash log output after
    // the error is reported.
    Log(LOG_FATAL, "reap logs");
    if (NULL == reverse_service_) {
      // No crash detector thread.
      NaClLog(LOG_ERROR, "scheduling to get crash log\n");
      // Invoking rev_interface's method is workaround to avoid crash_cb
      // gets called twice or more. We should clean this up later.
      rev_interface_->ReportCrash();
      NaClLog(LOG_ERROR, "should fire soon\n");
    } else {
      NaClLog(LOG_ERROR, "Reverse service thread will pick up crash log\n");
    }
  }

  pp::Module::Get()->core()->CallOnMainThread(0, data->callback, pp_error);

  // Because the ownership of data is taken by caller, we must clear it
  // manually here. Otherwise, its dtor invokes callbacks again.
  data->Clear();
}

bool ServiceRuntime::SetupCommandChannel() {
  NaClLog(4, "ServiceRuntime::SetupCommand (this=%p, subprocess=%p)\n",
          static_cast<void*>(this),
          static_cast<void*>(subprocess_.get()));
  if (!subprocess_->SetupCommand(&command_channel_)) {
    if (main_service_runtime_) {
      ErrorInfo error_info;
      error_info.SetReport(PP_NACL_ERROR_SEL_LDR_COMMUNICATION_CMD_CHANNEL,
                           "ServiceRuntime: command channel creation failed");
      plugin_->ReportLoadError(error_info);
    }
    return false;
  }
  return true;
}

void ServiceRuntime::LoadModule(PP_NaClFileInfo file_info,
                                pp::CompletionCallback callback) {
  NaClFileInfo nacl_file_info;
  nacl_file_info.desc = ConvertFileDescriptor(file_info.handle, true);
  nacl_file_info.file_token.lo = file_info.token_lo;
  nacl_file_info.file_token.hi = file_info.token_hi;
  NaClDesc* desc = NaClDescIoFromFileInfo(nacl_file_info, O_RDONLY);
  if (desc == NULL) {
    DidLoadModule(callback, PP_ERROR_FAILED);
    return;
  }

  // We don't use a scoped_ptr here since we would immediately release the
  // DescWrapper to LoadModule().
  nacl::DescWrapper* wrapper =
      plugin_->wrapper_factory()->MakeGenericCleanup(desc);

  // TODO(teravest, hidehiko): Replace this by Chrome IPC.
  bool result = subprocess_->LoadModule(&command_channel_, wrapper);
  DidLoadModule(callback, result ? PP_OK : PP_ERROR_FAILED);
}

void ServiceRuntime::DidLoadModule(pp::CompletionCallback callback,
                                   int32_t pp_error) {
  if (pp_error != PP_OK) {
    ErrorInfo error_info;
    error_info.SetReport(PP_NACL_ERROR_SEL_LDR_COMMUNICATION_CMD_CHANNEL,
                         "ServiceRuntime: load module failed");
    plugin_->ReportLoadError(error_info);
  }
  callback.Run(pp_error);
}

bool ServiceRuntime::InitReverseService() {
  if (uses_nonsfi_mode_) {
    // In non-SFI mode, no reverse service is set up. Just returns success.
    return true;
  }

  // Hook up the reverse service channel.  We are the IMC client, but
  // provide SRPC service.
  NaClDesc* out_conn_cap;
  NaClSrpcResultCodes rpc_result =
      NaClSrpcInvokeBySignature(&command_channel_,
                                "reverse_setup::h",
                                &out_conn_cap);

  if (NACL_SRPC_RESULT_OK != rpc_result) {
    if (main_service_runtime_) {
      ErrorInfo error_info;
      error_info.SetReport(PP_NACL_ERROR_SEL_LDR_COMMUNICATION_REV_SETUP,
                           "ServiceRuntime: reverse setup rpc failed");
      plugin_->ReportLoadError(error_info);
    }
    return false;
  }
  //  Get connection capability to service runtime where the IMC
  //  server/SRPC client is waiting for a rendezvous.
  NaClLog(4, "ServiceRuntime: got 0x%" NACL_PRIxPTR "\n",
          (uintptr_t) out_conn_cap);
  nacl::DescWrapper* conn_cap = plugin_->wrapper_factory()->MakeGenericCleanup(
      out_conn_cap);
  if (conn_cap == NULL) {
    if (main_service_runtime_) {
      ErrorInfo error_info;
      error_info.SetReport(PP_NACL_ERROR_SEL_LDR_COMMUNICATION_WRAPPER,
                           "ServiceRuntime: wrapper allocation failure");
      plugin_->ReportLoadError(error_info);
    }
    return false;
  }
  out_conn_cap = NULL;  // ownership passed
  NaClLog(4, "ServiceRuntime::InitReverseService: starting reverse service\n");
  reverse_service_ = new nacl::ReverseService(conn_cap, rev_interface_->Ref());
  if (!reverse_service_->Start()) {
    if (main_service_runtime_) {
      ErrorInfo error_info;
      error_info.SetReport(PP_NACL_ERROR_SEL_LDR_COMMUNICATION_REV_SERVICE,
                           "ServiceRuntime: starting reverse services failed");
      plugin_->ReportLoadError(error_info);
    }
    return false;
  }
  return true;
}

bool ServiceRuntime::StartModule() {
  // start the module.  otherwise we cannot connect for multimedia
  // subsystem since that is handled by user-level code (not secure!)
  // in libsrpc.
  int load_status = -1;
  if (uses_nonsfi_mode_) {
    // In non-SFI mode, we don't need to call start_module SRPC to launch
    // the plugin.
    load_status = LOAD_OK;
  } else {
    NaClSrpcResultCodes rpc_result =
        NaClSrpcInvokeBySignature(&command_channel_,
                                  "start_module::i",
                                  &load_status);

    if (NACL_SRPC_RESULT_OK != rpc_result) {
      if (main_service_runtime_) {
        ErrorInfo error_info;
        error_info.SetReport(PP_NACL_ERROR_SEL_LDR_START_MODULE,
                             "ServiceRuntime: could not start nacl module");
        plugin_->ReportLoadError(error_info);
      }
      return false;
    }
  }

  NaClLog(4, "ServiceRuntime::StartModule (load_status=%d)\n", load_status);
  if (main_service_runtime_) {
    if (load_status < 0 || load_status > NACL_ERROR_CODE_MAX)
      load_status = LOAD_STATUS_UNKNOWN;
    GetNaClInterface()->ReportSelLdrStatus(plugin_->pp_instance(),
                                           load_status,
                                           NACL_ERROR_CODE_MAX);
  }

  if (LOAD_OK != load_status) {
    if (main_service_runtime_) {
      ErrorInfo error_info;
      error_info.SetReport(
          PP_NACL_ERROR_SEL_LDR_START_STATUS,
          NaClErrorString(static_cast<NaClErrorCode>(load_status)));
      plugin_->ReportLoadError(error_info);
    }
    return false;
  }
  return true;
}

void ServiceRuntime::StartSelLdr(const SelLdrStartParams& params,
                                 pp::CompletionCallback callback) {
  NaClLog(4, "ServiceRuntime::Start\n");

  nacl::scoped_ptr<SelLdrLauncherChrome>
      tmp_subprocess(new SelLdrLauncherChrome());
  if (NULL == tmp_subprocess.get()) {
    NaClLog(LOG_ERROR, "ServiceRuntime::Start (subprocess create failed)\n");
    if (main_service_runtime_) {
      ErrorInfo error_info;
      error_info.SetReport(
          PP_NACL_ERROR_SEL_LDR_CREATE_LAUNCHER,
          "ServiceRuntime: failed to create sel_ldr launcher");
      plugin_->ReportLoadError(error_info);
    }
    pp::Module::Get()->core()->CallOnMainThread(0, callback, PP_ERROR_FAILED);
    return;
  }

  ManifestService* manifest_service =
      new ManifestService(anchor_->Ref(), rev_interface_);
  bool enable_dev_interfaces =
      GetNaClInterface()->DevInterfacesEnabled(plugin_->pp_instance());

  tmp_subprocess->Start(plugin_->pp_instance(),
                        main_service_runtime_,
                        params.url.c_str(),
                        params.uses_irt,
                        params.uses_ppapi,
                        uses_nonsfi_mode_,
                        enable_dev_interfaces,
                        params.enable_dyncode_syscalls,
                        params.enable_exception_handling,
                        params.enable_crash_throttling,
                        &kManifestServiceVTable,
                        manifest_service,
                        callback);
  subprocess_.reset(tmp_subprocess.release());
}

bool ServiceRuntime::WaitForSelLdrStart() {
  // Time to wait on condvar (for browser to create a new sel_ldr process on
  // our behalf). Use 6 seconds to be *fairly* conservative.
  //
  // On surfaway, the CallOnMainThread above may never get scheduled
  // to unblock this condvar, or the IPC reply from the browser to renderer
  // might get canceled/dropped. However, it is currently important to
  // avoid waiting indefinitely because ~PnaclCoordinator will attempt to
  // join() the PnaclTranslateThread, and the PnaclTranslateThread is waiting
  // for the signal before exiting.
  static int64_t const kWaitTimeMicrosecs = 6 * NACL_MICROS_PER_UNIT;
  int64_t left_to_wait = kWaitTimeMicrosecs;
  int64_t deadline = NaClGetTimeOfDayMicroseconds() + left_to_wait;
  nacl::MutexLocker take(&mu_);
  while(!start_sel_ldr_done_ && left_to_wait > 0) {
    struct nacl_abi_timespec left_timespec;
    left_timespec.tv_sec = left_to_wait / NACL_MICROS_PER_UNIT;
    left_timespec.tv_nsec =
        (left_to_wait % NACL_MICROS_PER_UNIT) * NACL_NANOS_PER_MICRO;
    NaClXCondVarTimedWaitRelative(&cond_, &mu_, &left_timespec);
    int64_t now = NaClGetTimeOfDayMicroseconds();
    left_to_wait = deadline - now;
  }
  return start_sel_ldr_done_;
}

void ServiceRuntime::SignalStartSelLdrDone() {
  nacl::MutexLocker take(&mu_);
  start_sel_ldr_done_ = true;
  NaClXCondVarSignal(&cond_);
}

void ServiceRuntime::WaitForNexeStart() {
  nacl::MutexLocker take(&mu_);
  while (!nexe_started_)
    NaClXCondVarWait(&cond_, &mu_);
  // Reset nexe_started_ here in case we run again.
  nexe_started_ = false;
}

void ServiceRuntime::SignalNexeStarted() {
  nacl::MutexLocker take(&mu_);
  nexe_started_ = true;
  NaClXCondVarSignal(&cond_);
}

void ServiceRuntime::LoadNexeAndStart(PP_NaClFileInfo file_info,
                                      const pp::CompletionCallback& callback) {
  NaClLog(4, "ServiceRuntime::LoadNexeAndStart (handle_valid=%d "
             "token_lo=%" NACL_PRIu64 " token_hi=%" NACL_PRIu64 ")\n",
      file_info.handle != PP_kInvalidFileHandle,
      file_info.token_lo,
      file_info.token_hi);

  nacl::scoped_ptr<LoadNexeAndStartData> data(
      new LoadNexeAndStartData(callback));
  if (!SetupCommandChannel() || !InitReverseService()) {
    DidLoadNexeAndStart(data.get(), PP_ERROR_FAILED);
    return;
  }

  LoadModule(
      file_info,
      WeakRefNewCallback(anchor_,
                         this,
                         &ServiceRuntime::LoadNexeAndStartAfterLoadModule,
                         data.release()));  // Delegate the ownership.
}

SrpcClient* ServiceRuntime::SetupAppChannel() {
  NaClLog(4, "ServiceRuntime::SetupAppChannel (subprocess_=%p)\n",
          reinterpret_cast<void*>(subprocess_.get()));
  nacl::DescWrapper* connect_desc = subprocess_->socket_addr()->Connect();
  if (NULL == connect_desc) {
    NaClLog(LOG_ERROR, "ServiceRuntime::SetupAppChannel (connect failed)\n");
    return NULL;
  } else {
    NaClLog(4, "ServiceRuntime::SetupAppChannel (conect_desc=%p)\n",
            static_cast<void*>(connect_desc));
    SrpcClient* srpc_client = SrpcClient::New(connect_desc);
    NaClLog(4, "ServiceRuntime::SetupAppChannel (srpc_client=%p)\n",
            static_cast<void*>(srpc_client));
    delete connect_desc;
    return srpc_client;
  }
}

bool ServiceRuntime::Log(int severity, const nacl::string& msg) {
  NaClSrpcResultCodes rpc_result =
      NaClSrpcInvokeBySignature(&command_channel_,
                                "log:is:",
                                severity,
                                strdup(msg.c_str()));
  return (NACL_SRPC_RESULT_OK == rpc_result);
}

void ServiceRuntime::Shutdown() {
  rev_interface_->ShutDown();
  anchor_->Abandon();
  // Abandon callbacks, tell service threads to quit if they were
  // blocked waiting for main thread operations to finish.  Note that
  // some callbacks must still await their completion event, e.g.,
  // CallOnMainThread must still wait for the time out, or I/O events
  // must finish, so resources associated with pending events cannot
  // be deallocated.

  // Note that this does waitpid() to get rid of any zombie subprocess.
  subprocess_.reset(NULL);

  NaClSrpcDtor(&command_channel_);

  // subprocess_ has been shut down, but threads waiting on messages
  // from the service runtime may not have noticed yet.  The low-level
  // NaClSimpleRevService code takes care to refcount the data objects
  // that it needs, and reverse_service_ is also refcounted.  We wait
  // for the service threads to get their EOF indications.
  if (reverse_service_ != NULL) {
    reverse_service_->WaitForServiceThreadsToExit();
    reverse_service_->Unref();
    reverse_service_ = NULL;
  }
}

ServiceRuntime::~ServiceRuntime() {
  NaClLog(4, "ServiceRuntime::~ServiceRuntime (this=%p)\n",
          static_cast<void*>(this));
  // We do this just in case Shutdown() was not called.
  subprocess_.reset(NULL);
  if (reverse_service_ != NULL)
    reverse_service_->Unref();

  rev_interface_->Unref();

  anchor_->Unref();
  NaClCondVarDtor(&cond_);
  NaClMutexDtor(&mu_);
}

void ServiceRuntime::set_exit_status(int exit_status) {
  nacl::MutexLocker take(&mu_);
  if (main_service_runtime_)
    plugin_->set_exit_status(exit_status & 0xff);
}

nacl::string ServiceRuntime::GetCrashLogOutput() {
  if (NULL != subprocess_.get()) {
    return subprocess_->GetCrashLogOutput();
  } else {
    return std::string();
  }
}

}  // namespace plugin
