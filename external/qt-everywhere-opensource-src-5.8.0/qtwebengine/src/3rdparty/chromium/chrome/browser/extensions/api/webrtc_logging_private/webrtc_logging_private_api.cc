// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/webrtc_logging_private/webrtc_logging_private_api.h"

#include "base/command_line.h"
#include "base/hash.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/supports_user_data.h"
#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/error_utils.h"

namespace extensions {

using api::webrtc_logging_private::MetaDataEntry;
using api::webrtc_logging_private::RequestInfo;
using content::BrowserThread;

namespace Discard = api::webrtc_logging_private::Discard;
namespace SetMetaData = api::webrtc_logging_private::SetMetaData;
namespace SetUploadOnRenderClose =
    api::webrtc_logging_private::SetUploadOnRenderClose;
namespace Start = api::webrtc_logging_private::Start;
namespace StartRtpDump = api::webrtc_logging_private::StartRtpDump;
namespace Stop = api::webrtc_logging_private::Stop;
namespace StopRtpDump = api::webrtc_logging_private::StopRtpDump;
namespace Store = api::webrtc_logging_private::Store;
namespace Upload = api::webrtc_logging_private::Upload;
namespace UploadStored = api::webrtc_logging_private::UploadStored;
namespace StartAudioDebugRecordings =
    api::webrtc_logging_private::StartAudioDebugRecordings;
namespace StopAudioDebugRecordings =
    api::webrtc_logging_private::StopAudioDebugRecordings;
namespace StartWebRtcEventLogging =
    api::webrtc_logging_private::StartWebRtcEventLogging;
namespace StopWebRtcEventLogging =
    api::webrtc_logging_private::StopWebRtcEventLogging;

namespace {
std::string HashIdWithOrigin(const std::string& security_origin,
                             const std::string& log_id) {
  return base::UintToString(base::Hash(security_origin + log_id));
}
}  // namespace

content::RenderProcessHost* WebrtcLoggingPrivateFunction::RphFromRequest(
    const RequestInfo& request, const std::string& security_origin) {
  // If |guest_process_id| is defined, directly use this id to find the
  // corresponding RenderProcessHost.
  if (request.guest_process_id.get())
    return content::RenderProcessHost::FromID(*request.guest_process_id.get());

  // Otherwise, use the |tab_id|. If there's no |tab_id| and no
  // |guest_process_id|, we can't look up the RenderProcessHost.
  if (!request.tab_id.get())
    return NULL;

  int tab_id = *request.tab_id.get();
  content::WebContents* contents = NULL;
  if (!ExtensionTabUtil::GetTabById(
           tab_id, GetProfile(), true, NULL, NULL, &contents, NULL)) {
    error_ = extensions::ErrorUtils::FormatErrorMessage(
        extensions::tabs_constants::kTabNotFoundError,
        base::IntToString(tab_id));
    return NULL;
  }
  if (!contents) {
    error_ = extensions::ErrorUtils::FormatErrorMessage(
        "Web contents for tab not found",
        base::IntToString(tab_id));
    return NULL;
  }
  GURL expected_origin = contents->GetLastCommittedURL().GetOrigin();
  if (expected_origin.spec() != security_origin) {
    error_ = extensions::ErrorUtils::FormatErrorMessage(
        "Invalid security origin. Expected=" + expected_origin.spec() +
            ", actual=" + security_origin,
        base::IntToString(tab_id));
    return NULL;
  }
  return contents->GetRenderProcessHost();
}

scoped_refptr<WebRtcLoggingHandlerHost>
WebrtcLoggingPrivateFunction::LoggingHandlerFromRequest(
    const api::webrtc_logging_private::RequestInfo& request,
    const std::string& security_origin) {
  content::RenderProcessHost* host = RphFromRequest(request, security_origin);
  if (!host)
    return nullptr;

  return base::UserDataAdapter<WebRtcLoggingHandlerHost>::Get(
      host, WebRtcLoggingHandlerHost::kWebRtcLoggingHandlerHostKey);
}

scoped_refptr<WebRtcLoggingHandlerHost>
WebrtcLoggingPrivateFunctionWithGenericCallback::PrepareTask(
    const RequestInfo& request, const std::string& security_origin,
    WebRtcLoggingHandlerHost::GenericDoneCallback* callback) {
  *callback = base::Bind(
      &WebrtcLoggingPrivateFunctionWithGenericCallback::FireCallback, this);
  return LoggingHandlerFromRequest(request, security_origin);
}

void WebrtcLoggingPrivateFunctionWithGenericCallback::FireCallback(
    bool success, const std::string& error_message) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!success)
    SetError(error_message);
  SendResponse(success);
}

void WebrtcLoggingPrivateFunctionWithUploadCallback::FireCallback(
    bool success, const std::string& report_id,
    const std::string& error_message) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (success) {
    api::webrtc_logging_private::UploadResult result;
    result.report_id = report_id;
    SetResult(result.ToValue());
  } else {
    SetError(error_message);
  }
  SendResponse(success);
}

void WebrtcLoggingPrivateFunctionWithRecordingDoneCallback::FireErrorCallback(
    const std::string& error_message) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  SetError(error_message);
  SendResponse(false);
}

void WebrtcLoggingPrivateFunctionWithRecordingDoneCallback::FireCallback(
    const std::string& prefix_path,
    bool did_stop,
    bool did_manual_stop) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  api::webrtc_logging_private::RecordingInfo result;
  result.prefix_path = prefix_path;
  result.did_stop = did_stop;
  result.did_manual_stop = did_manual_stop;
  SetResult(result.ToValue());
  SendResponse(true);
}

bool WebrtcLoggingPrivateSetMetaDataFunction::RunAsync() {
  std::unique_ptr<SetMetaData::Params> params(
      SetMetaData::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  WebRtcLoggingHandlerHost::GenericDoneCallback callback;
  scoped_refptr<WebRtcLoggingHandlerHost> webrtc_logging_handler_host =
      PrepareTask(params->request, params->security_origin, &callback);
  if (!webrtc_logging_handler_host.get())
    return false;

  std::unique_ptr<MetaDataMap> meta_data(new MetaDataMap());
  for (const MetaDataEntry& entry : params->meta_data)
    (*meta_data.get())[entry.key] = entry.value;

  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE, base::Bind(
      &WebRtcLoggingHandlerHost::SetMetaData, webrtc_logging_handler_host,
      base::Passed(&meta_data), callback));

  return true;
}

bool WebrtcLoggingPrivateStartFunction::RunAsync() {
  std::unique_ptr<Start::Params> params(Start::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  WebRtcLoggingHandlerHost::GenericDoneCallback callback;
  scoped_refptr<WebRtcLoggingHandlerHost> webrtc_logging_handler_host =
      PrepareTask(params->request, params->security_origin, &callback);
  if (!webrtc_logging_handler_host.get())
    return false;

  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE, base::Bind(
      &WebRtcLoggingHandlerHost::StartLogging, webrtc_logging_handler_host,
      callback));

  return true;
}

bool WebrtcLoggingPrivateSetUploadOnRenderCloseFunction::RunAsync() {
  std::unique_ptr<SetUploadOnRenderClose::Params> params(
      SetUploadOnRenderClose::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  scoped_refptr<WebRtcLoggingHandlerHost> webrtc_logging_handler_host(
      LoggingHandlerFromRequest(params->request, params->security_origin));
  if (!webrtc_logging_handler_host.get())
    return false;

  webrtc_logging_handler_host->set_upload_log_on_render_close(
      params->should_upload);

  return true;
}

bool WebrtcLoggingPrivateStopFunction::RunAsync() {
  std::unique_ptr<Stop::Params> params(Stop::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  WebRtcLoggingHandlerHost::GenericDoneCallback callback;
  scoped_refptr<WebRtcLoggingHandlerHost> webrtc_logging_handler_host =
      PrepareTask(params->request, params->security_origin, &callback);
  if (!webrtc_logging_handler_host.get())
    return false;

  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE, base::Bind(
      &WebRtcLoggingHandlerHost::StopLogging, webrtc_logging_handler_host,
      callback));

  return true;
}

bool WebrtcLoggingPrivateStoreFunction::RunAsync() {
  std::unique_ptr<Store::Params> params(Store::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  WebRtcLoggingHandlerHost::GenericDoneCallback callback;
  scoped_refptr<WebRtcLoggingHandlerHost> webrtc_logging_handler_host =
      PrepareTask(params->request, params->security_origin, &callback);
  if (!webrtc_logging_handler_host.get())
    return false;

  const std::string local_log_id(HashIdWithOrigin(params->security_origin,
                                                  params->log_id));

  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE, base::Bind(
      &WebRtcLoggingHandlerHost::StoreLog,
      webrtc_logging_handler_host, local_log_id, callback));

  return true;
}

bool WebrtcLoggingPrivateUploadStoredFunction::RunAsync() {
  std::unique_ptr<UploadStored::Params> params(
      UploadStored::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  scoped_refptr<WebRtcLoggingHandlerHost> logging_handler(
      LoggingHandlerFromRequest(params->request, params->security_origin));
  if (!logging_handler.get())
    return false;

  WebRtcLoggingHandlerHost::UploadDoneCallback callback = base::Bind(
      &WebrtcLoggingPrivateUploadStoredFunction::FireCallback, this);

  const std::string local_log_id(HashIdWithOrigin(params->security_origin,
                                                  params->log_id));

  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE, base::Bind(
      &WebRtcLoggingHandlerHost::UploadStoredLog, logging_handler, local_log_id,
      callback));

  return true;
}

bool WebrtcLoggingPrivateUploadFunction::RunAsync() {
  std::unique_ptr<Upload::Params> params(Upload::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  scoped_refptr<WebRtcLoggingHandlerHost> logging_handler(
      LoggingHandlerFromRequest(params->request, params->security_origin));
  if (!logging_handler.get())
    return false;

  WebRtcLoggingHandlerHost::UploadDoneCallback callback = base::Bind(
      &WebrtcLoggingPrivateUploadFunction::FireCallback, this);

  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE, base::Bind(
      &WebRtcLoggingHandlerHost::UploadLog, logging_handler, callback));

  return true;
}

bool WebrtcLoggingPrivateDiscardFunction::RunAsync() {
  std::unique_ptr<Discard::Params> params(Discard::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  WebRtcLoggingHandlerHost::GenericDoneCallback callback;
  scoped_refptr<WebRtcLoggingHandlerHost> webrtc_logging_handler_host =
      PrepareTask(params->request, params->security_origin, &callback);
  if (!webrtc_logging_handler_host.get())
    return false;

  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE, base::Bind(
      &WebRtcLoggingHandlerHost::DiscardLog, webrtc_logging_handler_host,
      callback));

  return true;
}

bool WebrtcLoggingPrivateStartRtpDumpFunction::RunAsync() {
  std::unique_ptr<StartRtpDump::Params> params(
      StartRtpDump::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  if (!params->incoming && !params->outgoing) {
    FireCallback(false, "Either incoming or outgoing must be true.");
    return true;
  }

  RtpDumpType type =
      (params->incoming && params->outgoing)
          ? RTP_DUMP_BOTH
          : (params->incoming ? RTP_DUMP_INCOMING : RTP_DUMP_OUTGOING);

  content::RenderProcessHost* host =
      RphFromRequest(params->request, params->security_origin);
  if (!host)
    return false;

  scoped_refptr<WebRtcLoggingHandlerHost> webrtc_logging_handler_host(
      base::UserDataAdapter<WebRtcLoggingHandlerHost>::Get(
          host, WebRtcLoggingHandlerHost::kWebRtcLoggingHandlerHostKey));

  WebRtcLoggingHandlerHost::GenericDoneCallback callback = base::Bind(
      &WebrtcLoggingPrivateStartRtpDumpFunction::FireCallback, this);

  // This call cannot fail.
  content::RenderProcessHost::WebRtcStopRtpDumpCallback stop_callback =
      host->StartRtpDump(params->incoming,
                         params->outgoing,
                         base::Bind(&WebRtcLoggingHandlerHost::OnRtpPacket,
                                    webrtc_logging_handler_host));

  BrowserThread::PostTask(BrowserThread::IO,
                          FROM_HERE,
                          base::Bind(&WebRtcLoggingHandlerHost::StartRtpDump,
                                     webrtc_logging_handler_host,
                                     type,
                                     callback,
                                     stop_callback));
  return true;
}

bool WebrtcLoggingPrivateStopRtpDumpFunction::RunAsync() {
  std::unique_ptr<StopRtpDump::Params> params(
      StopRtpDump::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  if (!params->incoming && !params->outgoing) {
    FireCallback(false, "Either incoming or outgoing must be true.");
    return true;
  }

  RtpDumpType type =
      (params->incoming && params->outgoing)
          ? RTP_DUMP_BOTH
          : (params->incoming ? RTP_DUMP_INCOMING : RTP_DUMP_OUTGOING);

  content::RenderProcessHost* host =
      RphFromRequest(params->request, params->security_origin);
  if (!host)
    return false;

  scoped_refptr<WebRtcLoggingHandlerHost> webrtc_logging_handler_host(
      base::UserDataAdapter<WebRtcLoggingHandlerHost>::Get(
          host, WebRtcLoggingHandlerHost::kWebRtcLoggingHandlerHostKey));

  WebRtcLoggingHandlerHost::GenericDoneCallback callback = base::Bind(
      &WebrtcLoggingPrivateStopRtpDumpFunction::FireCallback, this);

  BrowserThread::PostTask(BrowserThread::IO,
                          FROM_HERE,
                          base::Bind(&WebRtcLoggingHandlerHost::StopRtpDump,
                                     webrtc_logging_handler_host,
                                     type,
                                     callback));
  return true;
}

bool WebrtcLoggingPrivateStartAudioDebugRecordingsFunction::RunAsync() {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableAudioDebugRecordingsFromExtension)) {
    return false;
  }

  std::unique_ptr<StartAudioDebugRecordings::Params> params(
      StartAudioDebugRecordings::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  if (params->seconds < 0) {
    FireErrorCallback("seconds must be greater than or equal to 0");
    return true;
  }

  content::RenderProcessHost* host =
      RphFromRequest(params->request, params->security_origin);
  if (!host)
    return false;

  scoped_refptr<AudioDebugRecordingsHandler> audio_debug_recordings_handler(
      base::UserDataAdapter<AudioDebugRecordingsHandler>::Get(
          host, AudioDebugRecordingsHandler::kAudioDebugRecordingsHandlerKey));

  audio_debug_recordings_handler->StartAudioDebugRecordings(
      host, base::TimeDelta::FromSeconds(params->seconds),
      base::Bind(
          &WebrtcLoggingPrivateStartAudioDebugRecordingsFunction::FireCallback,
          this),
      base::Bind(&WebrtcLoggingPrivateStartAudioDebugRecordingsFunction::
                     FireErrorCallback,
                 this));
  return true;
}

bool WebrtcLoggingPrivateStopAudioDebugRecordingsFunction::RunAsync() {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableAudioDebugRecordingsFromExtension)) {
    return false;
  }

  std::unique_ptr<StopAudioDebugRecordings::Params> params(
      StopAudioDebugRecordings::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  content::RenderProcessHost* host =
      RphFromRequest(params->request, params->security_origin);
  if (!host)
    return false;

  scoped_refptr<AudioDebugRecordingsHandler> audio_debug_recordings_handler(
      base::UserDataAdapter<AudioDebugRecordingsHandler>::Get(
          host, AudioDebugRecordingsHandler::kAudioDebugRecordingsHandlerKey));

  audio_debug_recordings_handler->StopAudioDebugRecordings(
      host,
      base::Bind(
          &WebrtcLoggingPrivateStopAudioDebugRecordingsFunction::FireCallback,
          this),
      base::Bind(&WebrtcLoggingPrivateStopAudioDebugRecordingsFunction::
                     FireErrorCallback,
                 this));
  return true;
}

bool WebrtcLoggingPrivateStartWebRtcEventLoggingFunction::RunAsync() {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableWebRtcEventLoggingFromExtension)) {
    return false;
  }

  std::unique_ptr<StartWebRtcEventLogging::Params> params(
      StartWebRtcEventLogging::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  if (params->seconds < 0) {
    FireErrorCallback("seconds must be greater than or equal to 0");
    return true;
  }

  content::RenderProcessHost* host =
      RphFromRequest(params->request, params->security_origin);
  if (!host)
    return false;

  scoped_refptr<WebRtcEventLogHandler> webrtc_event_log_handler(
      base::UserDataAdapter<WebRtcEventLogHandler>::Get(
          host, WebRtcEventLogHandler::kWebRtcEventLogHandlerKey));

  webrtc_event_log_handler->StartWebRtcEventLogging(
      host, base::TimeDelta::FromSeconds(params->seconds),
      base::Bind(
        &WebrtcLoggingPrivateStartWebRtcEventLoggingFunction::FireCallback,
        this),
      base::Bind(
        &WebrtcLoggingPrivateStartWebRtcEventLoggingFunction::FireErrorCallback,
        this));
  return true;
}

bool WebrtcLoggingPrivateStopWebRtcEventLoggingFunction::RunAsync() {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableWebRtcEventLoggingFromExtension)) {
    return false;
  }

  std::unique_ptr<StopWebRtcEventLogging::Params> params(
      StopWebRtcEventLogging::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  content::RenderProcessHost* host =
      RphFromRequest(params->request, params->security_origin);
  if (!host)
    return false;

  scoped_refptr<WebRtcEventLogHandler> webrtc_event_log_handler(
      base::UserDataAdapter<WebRtcEventLogHandler>::Get(
          host, WebRtcEventLogHandler::kWebRtcEventLogHandlerKey));

  webrtc_event_log_handler->StopWebRtcEventLogging(
      host,
      base::Bind(
        &WebrtcLoggingPrivateStopWebRtcEventLoggingFunction::FireCallback,
        this),
      base::Bind(
        &WebrtcLoggingPrivateStopWebRtcEventLoggingFunction::FireErrorCallback,
        this));
  return true;
}

}  // namespace extensions
