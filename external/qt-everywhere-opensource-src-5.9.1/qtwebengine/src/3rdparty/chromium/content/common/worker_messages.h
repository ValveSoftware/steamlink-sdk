// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines messages between the browser and worker process, as well as between
// the renderer and worker process.

// Multiply-included message file, hence no include guard.

#include <string>
#include <utility>
#include <vector>

#include "base/strings/string16.h"
#include "content/common/content_export.h"
#include "content/common/content_param_traits.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_utils.h"
#include "url/gurl.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT
#define IPC_MESSAGE_START WorkerMsgStart

// Parameters structure for WorkerHostMsg_PostConsoleMessageToWorkerObject,
// which has too many data parameters to be reasonably put in a predefined
// IPC message. The data members directly correspond to parameters of
// WebWorkerClient::postConsoleMessageToWorkerObject()
IPC_STRUCT_BEGIN(WorkerHostMsg_PostConsoleMessageToWorkerObject_Params)
  IPC_STRUCT_MEMBER(int, source_identifier)
  IPC_STRUCT_MEMBER(int, message_type)
  IPC_STRUCT_MEMBER(int, message_level)
  IPC_STRUCT_MEMBER(base::string16, message)
  IPC_STRUCT_MEMBER(int, line_number)
  IPC_STRUCT_MEMBER(base::string16, source_url)
IPC_STRUCT_END()

// Parameter structure for WorkerProcessMsg_CreateWorker.
IPC_STRUCT_BEGIN(WorkerProcessMsg_CreateWorker_Params)
  IPC_STRUCT_MEMBER(GURL, url)
  IPC_STRUCT_MEMBER(base::string16, name)
  IPC_STRUCT_MEMBER(base::string16, content_security_policy)
  IPC_STRUCT_MEMBER(blink::WebContentSecurityPolicyType, security_policy_type)
  IPC_STRUCT_MEMBER(blink::WebAddressSpace, creation_address_space)
  IPC_STRUCT_MEMBER(bool, pause_on_start)
  IPC_STRUCT_MEMBER(int, route_id)
IPC_STRUCT_END()

//-----------------------------------------------------------------------------
// WorkerProcess messages
// These are messages sent from the browser to the worker process.
IPC_MESSAGE_CONTROL1(WorkerProcessMsg_CreateWorker,
                     WorkerProcessMsg_CreateWorker_Params)

//-----------------------------------------------------------------------------
// WorkerProcessHost messages
// These are messages sent from the worker process to the browser process.

// Sent by the worker process to check whether access to file system is allowed.
IPC_SYNC_MESSAGE_CONTROL2_1(WorkerProcessHostMsg_RequestFileSystemAccessSync,
                            int /* worker_route_id */,
                            GURL /* origin url */,
                            bool /* result */)

// Sent by the worker process to check whether access to IndexedDB is allowed.
IPC_SYNC_MESSAGE_CONTROL3_1(WorkerProcessHostMsg_AllowIndexedDB,
                            int /* worker_route_id */,
                            GURL /* origin url */,
                            base::string16 /* database name */,
                            bool /* result */)

// Sent by the worker process to request being killed.
IPC_SYNC_MESSAGE_CONTROL0_0(WorkerProcessHostMsg_ForceKillWorker)


//-----------------------------------------------------------------------------
// Worker messages
// These are messages sent from the renderer process to the worker process.
IPC_MESSAGE_ROUTED0(WorkerMsg_TerminateWorkerContext)

IPC_MESSAGE_ROUTED2(WorkerMsg_Connect,
                    int /* sent_message_port_id */,
                    int /* routing_id */)

IPC_MESSAGE_ROUTED0(WorkerMsg_WorkerObjectDestroyed)


//-----------------------------------------------------------------------------
// WorkerHost messages
// These are messages sent from the worker (renderer process) to the worker
// host (browser process).
IPC_MESSAGE_CONTROL1(WorkerHostMsg_WorkerContextClosed,
                     int /* worker_route_id */)

IPC_MESSAGE_CONTROL1(WorkerHostMsg_WorkerContextDestroyed,
                     int /* worker_route_id */)

// Renderer -> Browser message to indicate that the worker is ready for
// inspection.
IPC_MESSAGE_CONTROL1(WorkerHostMsg_WorkerReadyForInspection,
                     int /* worker_route_id */)

IPC_MESSAGE_CONTROL1(WorkerHostMsg_WorkerScriptLoaded,
                     int /* worker_route_id */)

IPC_MESSAGE_CONTROL1(WorkerHostMsg_WorkerScriptLoadFailed,
                     int /* worker_route_id */)

IPC_MESSAGE_CONTROL2(WorkerHostMsg_WorkerConnected,
                     int /* message_port_id */,
                     int /* worker_route_id */)
