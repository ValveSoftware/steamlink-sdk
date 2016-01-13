// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_INSTANCE_API_H_
#define PPAPI_THUNK_INSTANCE_API_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "ppapi/c/dev/ppb_url_util_dev.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_size.h"
#include "ppapi/c/ppb_audio_config.h"
#include "ppapi/c/ppb_console.h"
#include "ppapi/c/ppb_gamepad.h"
#include "ppapi/c/ppb_instance.h"
#include "ppapi/c/ppb_mouse_cursor.h"
#include "ppapi/c/ppb_text_input_controller.h"
#include "ppapi/c/private/pp_content_decryptor.h"
#include "ppapi/c/private/ppb_instance_private.h"
#include "ppapi/shared_impl/api_id.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/shared_impl/singleton_resource_id.h"

// Windows headers interfere with this file.
#ifdef PostMessage
#undef PostMessage
#endif

struct PP_DecryptedBlockInfo;
struct PP_DecryptedFrameInfo;
struct PPP_MessageHandler_0_1;

namespace ppapi {

class Resource;
class TrackedCallback;
struct ViewData;

namespace thunk {

class PPB_Flash_API;

class PPB_Instance_API {
 public:
  virtual ~PPB_Instance_API() {}

  virtual PP_Bool BindGraphics(PP_Instance instance, PP_Resource device) = 0;
  virtual PP_Bool IsFullFrame(PP_Instance instance) = 0;

  // Unexposed PPAPI functions for proxying.
  // Returns the internal view data struct.
  virtual const ViewData* GetViewData(PP_Instance instance) = 0;
  // Returns the flash fullscreen status.
  virtual PP_Bool FlashIsFullscreen(PP_Instance instance) = 0;

  // InstancePrivate.
  virtual PP_Var GetWindowObject(PP_Instance instance) = 0;
  virtual PP_Var GetOwnerElementObject(PP_Instance instance) = 0;
  virtual PP_Var ExecuteScript(PP_Instance instance,
                               PP_Var script,
                               PP_Var* exception) = 0;

  // Audio.
  virtual uint32_t GetAudioHardwareOutputSampleRate(PP_Instance instance) = 0;
  virtual uint32_t GetAudioHardwareOutputBufferSize(PP_Instance instance) = 0;

  // CharSet.
  virtual PP_Var GetDefaultCharSet(PP_Instance instance) = 0;

  // Console.
  virtual void Log(PP_Instance instance,
                   PP_LogLevel log_level,
                   PP_Var value) = 0;
  virtual void LogWithSource(PP_Instance instance,
                             PP_LogLevel log_level,
                             PP_Var source,
                             PP_Var value) = 0;

  // Find.
  virtual void SetPluginToHandleFindRequests(PP_Instance instance) = 0;
  virtual void NumberOfFindResultsChanged(PP_Instance instance,
                                          int32_t total,
                                          PP_Bool final_result) = 0;
  virtual void SelectedFindResultChanged(PP_Instance instance,
                                         int32_t index) = 0;
  virtual void SetTickmarks(PP_Instance instance,
                            const PP_Rect* tickmarks,
                            uint32_t count) = 0;

  // Fullscreen.
  virtual PP_Bool IsFullscreen(PP_Instance instance) = 0;
  virtual PP_Bool SetFullscreen(PP_Instance instance,
                                PP_Bool fullscreen) = 0;
  virtual PP_Bool GetScreenSize(PP_Instance instance, PP_Size* size) = 0;

  // This is an implementation-only function which grabs an instance of a
  // "singleton resource". These are used to implement instance interfaces
  // (functions which are associated with the instance itself as opposed to a
  // resource).
  virtual Resource* GetSingletonResource(
      PP_Instance instance, SingletonResourceID id) = 0;

  // InputEvent.
  virtual int32_t RequestInputEvents(PP_Instance instance,
                                     uint32_t event_classes) = 0;
  virtual int32_t RequestFilteringInputEvents(PP_Instance instance,
                                              uint32_t event_classes) = 0;
  virtual void ClearInputEventRequest(PP_Instance instance,
                                      uint32_t event_classes) = 0;

  // InputEventPrivate.
  virtual void StartTrackingLatency(PP_Instance instance) = 0;

  // Messaging.
  virtual void PostMessage(PP_Instance instance, PP_Var message) = 0;
  virtual int32_t RegisterMessageHandler(PP_Instance instance,
                                         void* user_data,
                                         const PPP_MessageHandler_0_1* handler,
                                         PP_Resource message_loop) = 0;
  virtual void UnregisterMessageHandler(PP_Instance instance) = 0;

  // Mouse cursor.
  virtual PP_Bool SetCursor(PP_Instance instance,
                            PP_MouseCursor_Type type,
                            PP_Resource image,
                            const PP_Point* hot_spot) = 0;

  // MouseLock.
  virtual int32_t LockMouse(PP_Instance instance,
                            scoped_refptr<TrackedCallback> callback) = 0;
  virtual void UnlockMouse(PP_Instance instance) = 0;

  // TextInput.
  virtual void SetTextInputType(PP_Instance instance,
                                PP_TextInput_Type type) = 0;
  virtual void UpdateCaretPosition(PP_Instance instance,
                                   const PP_Rect& caret,
                                   const PP_Rect& bounding_box) = 0;
  virtual void CancelCompositionText(PP_Instance instance) = 0;
  virtual void SelectionChanged(PP_Instance instance) = 0;
  virtual void UpdateSurroundingText(PP_Instance instance,
                                     const char* text,
                                     uint32_t caret,
                                     uint32_t anchor) = 0;

  // Zoom.
  virtual void ZoomChanged(PP_Instance instance, double factor) = 0;
  virtual void ZoomLimitsChanged(PP_Instance instance,
                                 double minimum_factor,
                                 double maximum_factor) = 0;
  // Testing and URLUtil.
  virtual PP_Var GetDocumentURL(PP_Instance instance,
                                PP_URLComponents_Dev* components) = 0;
#if !defined(OS_NACL)
  // Content Decryptor.
  virtual void PromiseResolved(PP_Instance instance, uint32 promise_id) = 0;
  virtual void PromiseResolvedWithSession(PP_Instance instance,
                                          uint32 promise_id,
                                          PP_Var web_session_id_var) = 0;
  virtual void PromiseRejected(PP_Instance instance,
                               uint32 promise_id,
                               PP_CdmExceptionCode exception_code,
                               uint32 system_code,
                               PP_Var error_description_var) = 0;
  virtual void SessionMessage(PP_Instance instance,
                              PP_Var web_session_id_var,
                              PP_Var message_var,
                              PP_Var destination_url_var) = 0;
  virtual void SessionReady(PP_Instance instance,
                            PP_Var web_session_id_var) = 0;
  virtual void SessionClosed(PP_Instance instance,
                             PP_Var web_session_id_var) = 0;
  virtual void SessionError(PP_Instance instance,
                            PP_Var web_session_id_var,
                            PP_CdmExceptionCode exception_code,
                            uint32 system_code,
                            PP_Var error_description_var) = 0;
  virtual void DeliverBlock(PP_Instance instance,
                            PP_Resource decrypted_block,
                            const PP_DecryptedBlockInfo* block_info) = 0;
  virtual void DecoderInitializeDone(PP_Instance instance,
                                     PP_DecryptorStreamType decoder_type,
                                     uint32_t request_id,
                                     PP_Bool success) = 0;
  virtual void DecoderDeinitializeDone(PP_Instance instance,
                                       PP_DecryptorStreamType decoder_type,
                                       uint32_t request_id) = 0;
  virtual void DecoderResetDone(PP_Instance instance,
                                PP_DecryptorStreamType decoder_type,
                                uint32_t request_id) = 0;
  virtual void DeliverFrame(PP_Instance instance,
                            PP_Resource decrypted_frame,
                            const PP_DecryptedFrameInfo* frame_info) = 0;
  virtual void DeliverSamples(PP_Instance instance,
                              PP_Resource audio_frames,
                              const PP_DecryptedSampleInfo* sample_info) = 0;

  // URLUtil.
  virtual PP_Var ResolveRelativeToDocument(
      PP_Instance instance,
      PP_Var relative,
      PP_URLComponents_Dev* components) = 0;
  virtual PP_Bool DocumentCanRequest(PP_Instance instance, PP_Var url) = 0;
  virtual PP_Bool DocumentCanAccessDocument(PP_Instance instance,
                                            PP_Instance target) = 0;
  virtual PP_Var GetPluginInstanceURL(PP_Instance instance,
                                      PP_URLComponents_Dev* components) = 0;
  virtual PP_Var GetPluginReferrerURL(PP_Instance instance,
                                      PP_URLComponents_Dev* components) = 0;
#endif  // !defined(OS_NACL)

  static const ApiID kApiID = API_ID_PPB_INSTANCE;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_INSTANCE_API_H_
