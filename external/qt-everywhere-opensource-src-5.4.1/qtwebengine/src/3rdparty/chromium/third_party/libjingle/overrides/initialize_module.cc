// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "allocator_shim/allocator_stub.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "init_webrtc.h"
#include "talk/base/basictypes.h"
#include "talk/media/webrtc/webrtcmediaengine.h"
#include "third_party/libjingle/overrides/talk/base/logging.h"

#if !defined(LIBPEERCONNECTION_IMPLEMENTATION) || defined(LIBPEERCONNECTION_LIB)
#error "Only compile the allocator proxy with the shared_library implementation"
#endif

#if defined(OS_WIN)
#define ALLOC_EXPORT __declspec(dllexport)
#else
#define ALLOC_EXPORT __attribute__((visibility("default")))
#endif

#if !defined(OS_MACOSX) && !defined(OS_ANDROID)
// These are used by our new/delete overrides in
// allocator_shim/allocator_proxy.cc
AllocateFunction g_alloc = NULL;
DellocateFunction g_dealloc = NULL;
#endif

// Forward declare of the libjingle internal factory and destroy methods for the
// WebRTC media engine.
cricket::MediaEngineInterface* CreateWebRtcMediaEngine(
    webrtc::AudioDeviceModule* adm,
    webrtc::AudioDeviceModule* adm_sc,
    cricket::WebRtcVideoEncoderFactory* encoder_factory,
    cricket::WebRtcVideoDecoderFactory* decoder_factory);

void DestroyWebRtcMediaEngine(cricket::MediaEngineInterface* media_engine);

// Define webrtc:field_trial::FindFullName to provide webrtc with a field trial
// implementation. The implementation is provided by the loader via the
// InitializeModule.
namespace {
FieldTrialFindFullName g_field_trial_find_ = NULL;
}

namespace webrtc {
namespace field_trial {
std::string FindFullName(const std::string& trial_name) {
  return g_field_trial_find_(trial_name);
}
}  // namespace field_trial
}  // namespace webrtc

extern "C" {

// Initialize logging, set the forward allocator functions (not on mac), and
// return pointers to libjingle's WebRTC factory methods.
// Called from init_webrtc.cc.
ALLOC_EXPORT
bool InitializeModule(const CommandLine& command_line,
#if !defined(OS_MACOSX) && !defined(OS_ANDROID)
                      AllocateFunction alloc,
                      DellocateFunction dealloc,
#endif
                      FieldTrialFindFullName field_trial_find,
                      logging::LogMessageHandlerFunction log_handler,
                      webrtc::GetCategoryEnabledPtr trace_get_category_enabled,
                      webrtc::AddTraceEventPtr trace_add_trace_event,
                      CreateWebRtcMediaEngineFunction* create_media_engine,
                      DestroyWebRtcMediaEngineFunction* destroy_media_engine,
                      InitDiagnosticLoggingDelegateFunctionFunction*
                          init_diagnostic_logging) {
#if !defined(OS_MACOSX) && !defined(OS_ANDROID)
  g_alloc = alloc;
  g_dealloc = dealloc;
#endif

  g_field_trial_find_ = field_trial_find;

  *create_media_engine = &CreateWebRtcMediaEngine;
  *destroy_media_engine = &DestroyWebRtcMediaEngine;
  *init_diagnostic_logging = &talk_base::InitDiagnosticLoggingDelegateFunction;

  if (CommandLine::Init(0, NULL)) {
#if !defined(OS_WIN)
    // This is not needed on Windows since CommandLine::Init has already
    // done the equivalent thing via the GetCommandLine() API.
    CommandLine::ForCurrentProcess()->AppendArguments(command_line, true);
#endif
    logging::LoggingSettings settings;
    settings.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG;
    logging::InitLogging(settings);

    // Override the log message handler to forward logs to chrome's handler.
    logging::SetLogMessageHandler(log_handler);
    webrtc::SetupEventTracer(trace_get_category_enabled,
                             trace_add_trace_event);
  }

  return true;
}
}  // extern "C"
