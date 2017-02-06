// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Message definition file, included multiple times, hence no include guard.

#include <stdint.h>

#include <string>

#include "content/common/service_worker/embedded_worker_settings.h"
#include "content/public/common/console_message_level.h"
#include "content/public/common/web_preferences.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_param_traits.h"
#include "url/gurl.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT

#define IPC_MESSAGE_START EmbeddedWorkerMsgStart

IPC_STRUCT_TRAITS_BEGIN(content::EmbeddedWorkerSettings)
  IPC_STRUCT_TRAITS_MEMBER(v8_cache_options)
  IPC_STRUCT_TRAITS_MEMBER(data_saver_enabled)
IPC_STRUCT_TRAITS_END()

// Parameters structure for EmbeddedWorkerMsg_StartWorker.
IPC_STRUCT_BEGIN(EmbeddedWorkerMsg_StartWorker_Params)
  IPC_STRUCT_MEMBER(int, embedded_worker_id)
  IPC_STRUCT_MEMBER(int64_t, service_worker_version_id)
  IPC_STRUCT_MEMBER(GURL, scope)
  IPC_STRUCT_MEMBER(GURL, script_url)
  IPC_STRUCT_MEMBER(int, worker_devtools_agent_route_id)
  IPC_STRUCT_MEMBER(bool, pause_after_download)
  IPC_STRUCT_MEMBER(bool, wait_for_debugger)
  IPC_STRUCT_MEMBER(bool, is_installed)
  IPC_STRUCT_MEMBER(content::EmbeddedWorkerSettings, settings)
IPC_STRUCT_END()

// Parameters structure for EmbeddedWorkerHostMsg_ReportConsoleMessage.
// The data members directly correspond to parameters of
// WorkerMessagingProxy::reportConsoleMessage()
IPC_STRUCT_BEGIN(EmbeddedWorkerHostMsg_ReportConsoleMessage_Params)
  IPC_STRUCT_MEMBER(int, source_identifier)
  IPC_STRUCT_MEMBER(int, message_level)
  IPC_STRUCT_MEMBER(base::string16, message)
  IPC_STRUCT_MEMBER(int, line_number)
  IPC_STRUCT_MEMBER(GURL, source_url)
IPC_STRUCT_END()

// Browser -> Renderer message to create a new embedded worker context.
IPC_MESSAGE_CONTROL1(EmbeddedWorkerMsg_StartWorker,
                     EmbeddedWorkerMsg_StartWorker_Params /* params */)

// Browser -> Renderer message to resume a worker that has been started
// with the pause_after_download option.
IPC_MESSAGE_CONTROL1(EmbeddedWorkerMsg_ResumeAfterDownload,
                     int /* embedded_worker_id */)

// Browser -> Renderer message to stop (terminate) the embedded worker.
IPC_MESSAGE_CONTROL1(EmbeddedWorkerMsg_StopWorker,
                     int /* embedded_worker_id */)

// Browser -> Renderer message to add message to the devtools console.
IPC_MESSAGE_CONTROL3(EmbeddedWorkerMsg_AddMessageToConsole,
                     int /* embedded_worker_id */,
                     content::ConsoleMessageLevel /* level */,
                     std::string /* message */)

// Renderer -> Browser message to indicate that the worker is ready for
// inspection.
IPC_MESSAGE_CONTROL1(EmbeddedWorkerHostMsg_WorkerReadyForInspection,
                     int /* embedded_worker_id */)

// Renderer -> Browser message to indicate that the worker has loaded the
// script.
IPC_MESSAGE_CONTROL1(EmbeddedWorkerHostMsg_WorkerScriptLoaded,
                     int /* embedded_worker_id */)

// Renderer -> Browser message to indicate that the worker thread is started.
IPC_MESSAGE_CONTROL3(EmbeddedWorkerHostMsg_WorkerThreadStarted,
                     int /* embedded_worker_id */,
                     int /* thread_id */,
                     int /* provider_id */)

// Renderer -> Browser message to indicate that the worker has failed to load
// the script.
IPC_MESSAGE_CONTROL1(EmbeddedWorkerHostMsg_WorkerScriptLoadFailed,
                     int /* embedded_worker_id */)

// Renderer -> Browser message to indicate that the worker has evaluated the
// script.
IPC_MESSAGE_CONTROL2(EmbeddedWorkerHostMsg_WorkerScriptEvaluated,
                     int /* embedded_worker_id */,
                     bool /* success */)

// Renderer -> Browser message to indicate that the worker is started.
IPC_MESSAGE_CONTROL1(EmbeddedWorkerHostMsg_WorkerStarted,
                     int /* embedded_worker_id */)

// Renderer -> Browser message to indicate that the worker is stopped.
IPC_MESSAGE_CONTROL1(EmbeddedWorkerHostMsg_WorkerStopped,
                     int /* embedded_worker_id */)

// Renderer -> Browser message to report an exception.
IPC_MESSAGE_CONTROL5(EmbeddedWorkerHostMsg_ReportException,
                     int /* embedded_worker_id */,
                     base::string16 /* error_message */,
                     int /* line_number */,
                     int /* column_number */,
                     GURL /* source_url */)

// Renderer -> Browser message to report console message.
IPC_MESSAGE_CONTROL2(
    EmbeddedWorkerHostMsg_ReportConsoleMessage,
    int /* embedded_worker_id */,
    EmbeddedWorkerHostMsg_ReportConsoleMessage_Params /* params */)

// ---------------------------------------------------------------------------
// For EmbeddedWorkerContext related messages, which are directly sent from
// browser to the worker thread in the child process. We use a new message class
// for this for easier cross-thread message dispatching.

#undef IPC_MESSAGE_START
#define IPC_MESSAGE_START EmbeddedWorkerContextMsgStart

// Browser -> Renderer message to send message.
IPC_MESSAGE_CONTROL3(EmbeddedWorkerContextMsg_MessageToWorker,
                     int /* thread_id */,
                     int /* embedded_worker_id */,
                     IPC::Message /* message */)
