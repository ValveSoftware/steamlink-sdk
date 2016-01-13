// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Developer tools consist of the following parts:
//
// DevToolsAgent lives in the renderer of an inspected page and provides access
// to the pages resources, DOM, v8 etc. by means of IPC messages.
//
// DevToolsClient is a thin delegate that lives in the tools front-end
// renderer and converts IPC messages to frontend method calls and allows the
// frontend to send messages to the DevToolsAgent.
//
// All the messages are routed through browser process. There is a
// DevToolsManager living in the browser process that is responsible for
// routing logistics. It is also capable of sending direct messages to the
// agent rather than forwarding messages between agents and clients only.
//
// Chain of communication between the components may be described by the
// following diagram:
//  ----------------------------
// | (tools frontend            |
// | renderer process)          |
// |                            |            --------------------
// |tools    <--> DevToolsClient+<-- IPC -->+ (browser process)  |
// |frontend                    |           |                    |
//  ----------------------------             ---------+----------
//                                                    ^
//                                                    |
//                                                   IPC
//                                                    |
//                                                    v
//                          --------------------------+--------
//                         | inspected page <--> DevToolsAgent |
//                         |                                   |
//                         | (inspected page renderer process) |
//                          -----------------------------------
//
// This file describes developer tools message types.

// Multiply-included message file, no standard include guard.
#include <map>
#include <string>

#include "content/common/content_export.h"
#include "content/public/common/common_param_traits.h"
#include "content/public/common/console_message_level.h"
#include "ipc/ipc_message_macros.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT

#define IPC_MESSAGE_START DevToolsMsgStart

// These are messages sent from DevToolsAgent to DevToolsClient through the
// browser.
// WebKit-level transport.
IPC_MESSAGE_ROUTED1(DevToolsClientMsg_DispatchOnInspectorFrontend,
                    std::string /* message */)

//-----------------------------------------------------------------------------
// These are messages sent from DevToolsClient to DevToolsAgent through the
// browser.
// Tells agent that there is a client host connected to it.
IPC_MESSAGE_ROUTED1(DevToolsAgentMsg_Attach,
                    std::string /* host_id */)

// Tells agent that a client host was disconnected from another agent and
// connected to this one.
IPC_MESSAGE_ROUTED2(DevToolsAgentMsg_Reattach,
                    std::string /* host_id */,
                    std::string /* agent_state */)

// Tells agent that there is no longer a client host connected to it.
IPC_MESSAGE_ROUTED0(DevToolsAgentMsg_Detach)

// WebKit-level transport.
IPC_MESSAGE_ROUTED1(DevToolsAgentMsg_DispatchOnInspectorBackend,
                    std::string /* message */)

// Inspect element with the given coordinates.
IPC_MESSAGE_ROUTED3(DevToolsAgentMsg_InspectElement,
                    std::string /* host_id */,
                    int /* x */,
                    int /* y */)

// Add message to the devtools console.
IPC_MESSAGE_ROUTED2(DevToolsAgentMsg_AddMessageToConsole,
                    content::ConsoleMessageLevel /* level */,
                    std::string /* message */)

// Worker DevTools agent should resume worker execution.
IPC_MESSAGE_ROUTED0(DevToolsAgentMsg_ResumeWorkerContext)

//-----------------------------------------------------------------------------
// These are messages sent from the browser to the renderer.

// RenderViewHostDelegate::RenderViewCreated method sends this message to a
// new renderer to notify it that it will host developer tools UI and should
// set up all neccessary bindings and create DevToolsClient instance that
// will handle communication with inspected page DevToolsAgent.
IPC_MESSAGE_ROUTED0(DevToolsMsg_SetupDevToolsClient)


//-----------------------------------------------------------------------------
// These are messages sent from the renderer to the browser.

// Transport from Inspector frontend to frontend host.
IPC_MESSAGE_ROUTED1(DevToolsHostMsg_DispatchOnEmbedder,
                    std::string /* message */)

// Updates agent runtime state stored in devtools manager in order to support
// cross-navigation instrumentation.
IPC_MESSAGE_ROUTED1(DevToolsHostMsg_SaveAgentRuntimeState,
                    std::string /* state */)

//-----------------------------------------------------------------------------
// These are messages sent from the GPU process to the inspected renderer.

IPC_STRUCT_BEGIN(GpuTaskInfo)
  IPC_STRUCT_MEMBER(double, timestamp)
  IPC_STRUCT_MEMBER(int, phase)
  IPC_STRUCT_MEMBER(bool, foreign)
  IPC_STRUCT_MEMBER(uint64, gpu_memory_used_bytes)
  IPC_STRUCT_MEMBER(uint64, gpu_memory_limit_bytes)
IPC_STRUCT_END()

// Recorded events are passed in chunks to the renderer process.
IPC_MESSAGE_ROUTED1(DevToolsAgentMsg_GpuTasksChunk,
                    std::vector<GpuTaskInfo> /* gpu_tasks */)

//-----------------------------------------------------------------------------
// These are messages sent from the inspected page renderer to the worker
// renderer.
