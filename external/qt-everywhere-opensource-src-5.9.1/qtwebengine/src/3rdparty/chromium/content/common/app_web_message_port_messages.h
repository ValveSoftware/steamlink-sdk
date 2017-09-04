// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included file, no traditional include guard.
#include <vector>

#include "content/common/content_export.h"
#include "ipc/ipc_message_macros.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT
#define IPC_MESSAGE_START AwMessagePortMsgStart

//-----------------------------------------------------------------------------
// MessagePort messages
// These are messages sent from the browser to the renderer process.

// Normally the postmessages are exchanged between the renderers and the message
// itself is opaque to the browser process. The format of the message is a
// WebSerializesScriptValue.  A WebSerializedScriptValue is a blink structure
// and can only be serialized/deserialized in renderer. Further, we could not
// have Blink or V8 on the browser side due to their relience on static
// variables.
//
// For posting messages from Java (Android apps) to JS, we pass the
// browser/renderer boundary an extra time and convert the messages to a type
// that browser can use. Within the current implementation specificications,
// where we use the main frame on the browser side and it always stays within
// the same process this is not expensive, but if we can do the conversion at
// the browser, then we can drop this code.

// Important Note about multi-process situation: In a multi-process scenario,
// the renderer that does the conversion can be theoretically different then the
// renderer that receives the message. Although in the current implementation
// this doesn't become an issue, there are 2 possible solutions to deal with
// this and make the overall system more robust to future changes:
// 1. Do the conversion at the browser side by writing a new serializer
// deserializer for WebSerializedScriptValue
// 2. Do the conversion at the content layer, at the renderer at the time of
// receiving the message. This may need adding new flags to indicate that
// message needs to be converted. However, this is complicated due to queing
// at the browser side and possibility of ports being shipped to a different
// renderer or browser delegate.

// Tells the renderer to convert the message from a WebSerializeScript
// format to a base::ListValue. This IPC is used for messages that are
// incoming to Android apps from JS.
IPC_MESSAGE_ROUTED3(AppWebMessagePortMsg_WebToAppMessage,
                    int /* recipient message port id */,
                    base::string16 /* message */,
                    std::vector<int> /* sent message port_ids */)

// Tells the renderer to convert the message from a String16
// format to a WebSerializedScriptValue. This IPC is used for messages that
// are outgoing from Android apps to JS.
// TODO(sgurun) when we start supporting other types, use a ListValue instead
// of string16
IPC_MESSAGE_ROUTED3(AppWebMessagePortMsg_AppToWebMessage,
                    int /* recipient message port id */,
                    base::string16 /* message */,
                    std::vector<int> /* sent message port_ids */)

// Used to defer message port closing until after all in-flight messages
// are flushed from renderer to browser. Renderer piggy-backs the message
// to browser.
IPC_MESSAGE_ROUTED1(AppWebMessagePortMsg_ClosePort, int /* message port id */)

//-----------------------------------------------------------------------------
// These are messages sent from the renderer to the browser process.

// Response to AppWebMessagePortMessage_WebToAppMessage
IPC_MESSAGE_ROUTED3(AppWebMessagePortHostMsg_ConvertedWebToAppMessage,
                    int /* recipient message port id */,
                    base::ListValue /* converted message */,
                    std::vector<int> /* sent message port_ids */)

// Response to AppWebMessagePortMessage_AppToWebMessage
IPC_MESSAGE_ROUTED3(AppWebMessagePortHostMsg_ConvertedAppToWebMessage,
                    int /* recipient message port id */,
                    base::string16 /* converted message */,
                    std::vector<int> /* sent message port_ids */)

// Response to AppWebMessagePortMsg_ClosePort
IPC_MESSAGE_ROUTED1(AppWebMessagePortHostMsg_ClosePortAck,
                    int /* message port id */)
