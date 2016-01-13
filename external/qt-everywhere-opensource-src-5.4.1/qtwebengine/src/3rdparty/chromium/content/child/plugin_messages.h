// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Multiply-included message file, hence no include guard.

#include "build/build_config.h"
#include "content/child/plugin_param_traits.h"
#include "content/common/content_export.h"
#include "content/common/content_param_traits.h"
#include "content/common/cursors/webcursor.h"
#include "content/public/common/common_param_traits.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_message_macros.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/rect.h"

#if defined(OS_POSIX)
#include "base/file_descriptor_posix.h"
#endif

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT

#define IPC_MESSAGE_START PluginMsgStart

IPC_STRUCT_BEGIN(PluginMsg_Init_Params)
  IPC_STRUCT_MEMBER(GURL,  url)
  IPC_STRUCT_MEMBER(GURL,  page_url)
  IPC_STRUCT_MEMBER(std::vector<std::string>, arg_names)
  IPC_STRUCT_MEMBER(std::vector<std::string>, arg_values)
  IPC_STRUCT_MEMBER(bool, load_manually)
  IPC_STRUCT_MEMBER(int, host_render_view_routing_id)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(PluginHostMsg_URLRequest_Params)
  IPC_STRUCT_MEMBER(std::string, url)
  IPC_STRUCT_MEMBER(std::string, method)
  IPC_STRUCT_MEMBER(std::string, target)
  IPC_STRUCT_MEMBER(std::vector<char>, buffer)
  IPC_STRUCT_MEMBER(int, notify_id)
  IPC_STRUCT_MEMBER(bool, popups_allowed)
  IPC_STRUCT_MEMBER(bool, notify_redirects)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(PluginMsg_DidReceiveResponseParams)
  IPC_STRUCT_MEMBER(unsigned long, id)
  IPC_STRUCT_MEMBER(std::string, mime_type)
  IPC_STRUCT_MEMBER(std::string, headers)
  IPC_STRUCT_MEMBER(uint32, expected_length)
  IPC_STRUCT_MEMBER(uint32, last_modified)
  IPC_STRUCT_MEMBER(bool, request_is_seekable)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(PluginMsg_FetchURL_Params)
  IPC_STRUCT_MEMBER(unsigned long, resource_id)
  IPC_STRUCT_MEMBER(int, notify_id)
  IPC_STRUCT_MEMBER(GURL, url)
  IPC_STRUCT_MEMBER(GURL, first_party_for_cookies)
  IPC_STRUCT_MEMBER(std::string, method)
  IPC_STRUCT_MEMBER(std::vector<char>, post_data)
  IPC_STRUCT_MEMBER(GURL, referrer)
  IPC_STRUCT_MEMBER(bool, notify_redirect)
  IPC_STRUCT_MEMBER(bool, is_plugin_src_load)
  IPC_STRUCT_MEMBER(int, render_frame_id)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(PluginMsg_UpdateGeometry_Param)
  IPC_STRUCT_MEMBER(gfx::Rect, window_rect)
  IPC_STRUCT_MEMBER(gfx::Rect, clip_rect)
  IPC_STRUCT_MEMBER(TransportDIB::Handle, windowless_buffer0)
  IPC_STRUCT_MEMBER(TransportDIB::Handle, windowless_buffer1)
  IPC_STRUCT_MEMBER(int, windowless_buffer_index)
IPC_STRUCT_END()

//-----------------------------------------------------------------------------
// Plugin messages
// These are messages sent from the renderer process to the plugin process.
// Tells the plugin process to create a new plugin instance with the given
// id.  A corresponding WebPluginDelegateStub is created which hosts the
// WebPluginDelegateImpl.
IPC_SYNC_MESSAGE_CONTROL1_1(PluginMsg_CreateInstance,
                            std::string /* mime_type */,
                            int /* instance_id */)

// The WebPluginDelegateProxy sends this to the WebPluginDelegateStub in its
// destructor, so that the stub deletes the actual WebPluginDelegateImpl
// object that it's hosting.
IPC_SYNC_MESSAGE_CONTROL1_0(PluginMsg_DestroyInstance,
                            int /* instance_id */)

IPC_SYNC_MESSAGE_CONTROL0_1(PluginMsg_GenerateRouteID,
                           int /* id */)

// The messages below all map to WebPluginDelegate methods.
IPC_SYNC_MESSAGE_ROUTED1_2(PluginMsg_Init,
                           PluginMsg_Init_Params,
                           bool /* transparent */,
                           bool /* result */)

// Used to synchronously request a paint for windowless plugins.
IPC_SYNC_MESSAGE_ROUTED1_0(PluginMsg_Paint,
                           gfx::Rect /* damaged_rect */)

// Sent by the renderer after it paints from its backing store so that the
// plugin knows it can send more invalidates.
IPC_MESSAGE_ROUTED0(PluginMsg_DidPaint)

IPC_SYNC_MESSAGE_ROUTED0_1(PluginMsg_GetPluginScriptableObject,
                           int /* route_id */)

// Gets the form value of the plugin instance synchronously.
IPC_SYNC_MESSAGE_ROUTED0_2(PluginMsg_GetFormValue,
                           base::string16 /* value */,
                           bool /* success */)

IPC_MESSAGE_ROUTED3(PluginMsg_DidFinishLoadWithReason,
                    GURL /* url */,
                    int /* reason */,
                    int /* notify_id */)

// Updates the plugin location.
IPC_MESSAGE_ROUTED1(PluginMsg_UpdateGeometry,
                    PluginMsg_UpdateGeometry_Param)

// A synchronous version of above.
IPC_SYNC_MESSAGE_ROUTED1_0(PluginMsg_UpdateGeometrySync,
                           PluginMsg_UpdateGeometry_Param)

IPC_SYNC_MESSAGE_ROUTED1_0(PluginMsg_SetFocus,
                           bool /* focused */)

IPC_SYNC_MESSAGE_ROUTED1_2(PluginMsg_HandleInputEvent,
                           IPC::WebInputEventPointer /* event */,
                           bool /* handled */,
                           content::WebCursor /* cursor type*/)

IPC_MESSAGE_ROUTED1(PluginMsg_SetContentAreaFocus,
                    bool /* has_focus */)

IPC_SYNC_MESSAGE_ROUTED3_0(PluginMsg_WillSendRequest,
                           unsigned long /* id */,
                           GURL /* url */,
                           int  /* http_status_code */)

IPC_MESSAGE_ROUTED1(PluginMsg_DidReceiveResponse,
                    PluginMsg_DidReceiveResponseParams)

IPC_MESSAGE_ROUTED3(PluginMsg_DidReceiveData,
                    unsigned long /* id */,
                    std::vector<char> /* buffer */,
                    int /* data_offset */)

IPC_MESSAGE_ROUTED1(PluginMsg_DidFinishLoading,
                    unsigned long /* id */)

IPC_MESSAGE_ROUTED1(PluginMsg_DidFail,
                    unsigned long /* id */)

IPC_MESSAGE_ROUTED4(PluginMsg_SendJavaScriptStream,
                    GURL /* url */,
                    std::string /* result */,
                    bool /* success */,
                    int /* notify_id */)

IPC_MESSAGE_ROUTED2(PluginMsg_DidReceiveManualResponse,
                    GURL /* url */,
                    PluginMsg_DidReceiveResponseParams)

IPC_MESSAGE_ROUTED1(PluginMsg_DidReceiveManualData,
                    std::vector<char> /* buffer */)

IPC_MESSAGE_ROUTED0(PluginMsg_DidFinishManualLoading)

IPC_MESSAGE_ROUTED0(PluginMsg_DidManualLoadFail)

IPC_MESSAGE_ROUTED3(PluginMsg_HandleURLRequestReply,
                    unsigned long /* resource_id */,
                    GURL /* url */,
                    int /* notify_id */)

IPC_MESSAGE_ROUTED2(PluginMsg_HTTPRangeRequestReply,
                    unsigned long /* resource_id */,
                    int /* range_request_id */)

IPC_MESSAGE_CONTROL1(PluginMsg_SignalModalDialogEvent,
                     int /* render_view_id */)

IPC_MESSAGE_CONTROL1(PluginMsg_ResetModalDialogEvent,
                     int /* render_view_id */)

IPC_MESSAGE_ROUTED1(PluginMsg_FetchURL,
                    PluginMsg_FetchURL_Params)

IPC_MESSAGE_CONTROL1(PluginHostMsg_DidAbortLoading,
                     int /* render_view_id */)

#if defined(OS_WIN)
IPC_MESSAGE_ROUTED4(PluginMsg_ImeCompositionUpdated,
                    base::string16 /* text */,
                    std::vector<int> /* clauses */,
                    std::vector<int>, /* target */
                    int /* cursor_position */)

IPC_MESSAGE_ROUTED1(PluginMsg_ImeCompositionCompleted,
                    base::string16 /* text */)
#endif

#if defined(OS_MACOSX)
IPC_MESSAGE_ROUTED1(PluginMsg_SetWindowFocus,
                    bool /* has_focus */)

IPC_MESSAGE_ROUTED0(PluginMsg_ContainerHidden)

IPC_MESSAGE_ROUTED3(PluginMsg_ContainerShown,
                    gfx::Rect /* window_frame */,
                    gfx::Rect /* view_frame */,
                    bool /* has_focus */)

IPC_MESSAGE_ROUTED2(PluginMsg_WindowFrameChanged,
                    gfx::Rect /* window_frame */,
                    gfx::Rect /* view_frame */)

IPC_MESSAGE_ROUTED1(PluginMsg_ImeCompositionCompleted,
                    base::string16 /* text */)
#endif

//-----------------------------------------------------------------------------
// PluginHost messages
// These are messages sent from the plugin process to the renderer process.
// They all map to the corresponding WebPlugin methods.
// Sends the plugin window information to the renderer.
// The window parameter is a handle to the window if the plugin is a windowed
// plugin. It is NULL for windowless plugins.
IPC_SYNC_MESSAGE_ROUTED1_0(PluginHostMsg_SetWindow,
                           gfx::PluginWindowHandle /* window */)

IPC_MESSAGE_ROUTED1(PluginHostMsg_URLRequest,
                    PluginHostMsg_URLRequest_Params)

IPC_MESSAGE_ROUTED1(PluginHostMsg_CancelResource,
                    int /* id */)

IPC_MESSAGE_ROUTED1(PluginHostMsg_InvalidateRect,
                    gfx::Rect /* rect */)

IPC_SYNC_MESSAGE_ROUTED1_1(PluginHostMsg_GetWindowScriptNPObject,
                           int /* route id */,
                           bool /* success */)

IPC_SYNC_MESSAGE_ROUTED1_1(PluginHostMsg_GetPluginElement,
                           int /* route id */,
                           bool /* success */)

IPC_SYNC_MESSAGE_ROUTED1_2(PluginHostMsg_ResolveProxy,
                           GURL /* url */,
                           bool /* result */,
                           std::string /* proxy list */)

IPC_MESSAGE_ROUTED3(PluginHostMsg_SetCookie,
                    GURL /* url */,
                    GURL /* first_party_for_cookies */,
                    std::string /* cookie */)

IPC_SYNC_MESSAGE_ROUTED2_1(PluginHostMsg_GetCookies,
                           GURL /* url */,
                           GURL /* first_party_for_cookies */,
                           std::string /* cookies */)

IPC_MESSAGE_ROUTED0(PluginHostMsg_CancelDocumentLoad)

IPC_MESSAGE_ROUTED3(PluginHostMsg_InitiateHTTPRangeRequest,
                    std::string /* url */,
                    std::string /* range_info */,
                    int         /* range_request_id */)

IPC_MESSAGE_ROUTED0(PluginHostMsg_DidStartLoading)
IPC_MESSAGE_ROUTED0(PluginHostMsg_DidStopLoading)

IPC_MESSAGE_ROUTED2(PluginHostMsg_DeferResourceLoading,
                    unsigned long /* resource_id */,
                    bool /* defer */)

IPC_SYNC_MESSAGE_CONTROL1_0(PluginHostMsg_SetException,
                            std::string /* message */)

IPC_MESSAGE_CONTROL0(PluginHostMsg_PluginShuttingDown)

IPC_MESSAGE_ROUTED2(PluginHostMsg_URLRedirectResponse,
                    bool /* allow */,
                    int  /* resource_id */)

IPC_SYNC_MESSAGE_ROUTED1_1(PluginHostMsg_CheckIfRunInsecureContent,
                           GURL /* url */,
                           bool /* result */)

#if defined(OS_WIN)
// The modal_loop_pump_messages_event parameter is an event handle which is
// passed in for windowless plugins and is used to indicate if messages
// are to be pumped in sync calls to the plugin process. Currently used
// in HandleEvent calls.
IPC_SYNC_MESSAGE_ROUTED2_0(PluginHostMsg_SetWindowlessData,
                           HANDLE /* modal_loop_pump_messages_event */,
                           gfx::NativeViewId /* dummy_activation_window*/)

// Send the IME status retrieved from a windowless plug-in. A windowless plug-in
// uses the IME attached to a browser process as a renderer does. A plug-in
// sends this message to control the IME status of a browser process. I would
// note that a plug-in sends this message to a renderer process that hosts this
// plug-in (not directly to a browser process) so the renderer process can
// update its IME status.
IPC_MESSAGE_ROUTED2(PluginHostMsg_NotifyIMEStatus,
                    int /* input_type */,
                    gfx::Rect /* caret_rect */)
#endif

#if defined(OS_MACOSX)
IPC_MESSAGE_ROUTED1(PluginHostMsg_FocusChanged,
                    bool /* focused */)

IPC_MESSAGE_ROUTED0(PluginHostMsg_StartIme)

//----------------------------------------------------------------------
// Core Animation plugin implementation rendering via compositor.

// Notifies the renderer process that this plugin will be using the
// accelerated rendering path.
IPC_MESSAGE_ROUTED0(PluginHostMsg_AcceleratedPluginEnabledRendering)

// Notifies the renderer process that the plugin allocated a new
// IOSurface into which it is rendering. The renderer process forwards
// this IOSurface to the GPU process, causing it to be bound to a
// texture from which the compositor can render. Any previous
// IOSurface allocated by this plugin must be implicitly released by
// the receipt of this message.
IPC_MESSAGE_ROUTED3(PluginHostMsg_AcceleratedPluginAllocatedIOSurface,
                    int32 /* width */,
                    int32 /* height */,
                    uint32 /* surface_id */)

// Notifies the renderer process that the plugin produced a new frame
// of content into its IOSurface, and therefore that the compositor
// needs to redraw.
IPC_MESSAGE_ROUTED0(PluginHostMsg_AcceleratedPluginSwappedIOSurface)
#endif


//-----------------------------------------------------------------------------
// NPObject messages
// These are messages used to marshall NPObjects.  They are sent both from the
// plugin to the renderer and from the renderer to the plugin.
IPC_SYNC_MESSAGE_ROUTED0_0(NPObjectMsg_Release)

IPC_SYNC_MESSAGE_ROUTED1_1(NPObjectMsg_HasMethod,
                           content::NPIdentifier_Param /* name */,
                           bool /* result */)

IPC_SYNC_MESSAGE_ROUTED3_2(NPObjectMsg_Invoke,
                           bool /* is_default */,
                           content::NPIdentifier_Param /* method */,
                           std::vector<content::NPVariant_Param> /* args */,
                           content::NPVariant_Param /* result_param */,
                           bool /* result */)

IPC_SYNC_MESSAGE_ROUTED1_1(NPObjectMsg_HasProperty,
                           content::NPIdentifier_Param /* name */,
                           bool /* result */)

IPC_SYNC_MESSAGE_ROUTED1_2(NPObjectMsg_GetProperty,
                           content::NPIdentifier_Param /* name */,
                           content::NPVariant_Param /* property */,
                           bool /* result */)

IPC_SYNC_MESSAGE_ROUTED2_1(NPObjectMsg_SetProperty,
                           content::NPIdentifier_Param /* name */,
                           content::NPVariant_Param /* property */,
                           bool /* result */)

IPC_SYNC_MESSAGE_ROUTED1_1(NPObjectMsg_RemoveProperty,
                           content::NPIdentifier_Param /* name */,
                           bool /* result */)

IPC_SYNC_MESSAGE_ROUTED0_0(NPObjectMsg_Invalidate)

IPC_SYNC_MESSAGE_ROUTED0_2(NPObjectMsg_Enumeration,
                           std::vector<content::NPIdentifier_Param> /* value */,
                           bool /* result */)

IPC_SYNC_MESSAGE_ROUTED1_2(NPObjectMsg_Construct,
                           std::vector<content::NPVariant_Param> /* args */,
                           content::NPVariant_Param /* result_param */,
                           bool /* result */)

IPC_SYNC_MESSAGE_ROUTED2_2(NPObjectMsg_Evaluate,
                           std::string /* script */,
                           bool /* popups_allowed */,
                           content::NPVariant_Param /* result_param */,
                           bool /* result */)
