// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PPB_INSTANCE_PROXY_H_
#define PPAPI_PROXY_PPB_INSTANCE_PROXY_H_

#include <string>

#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/proxy/interface_proxy.h"
#include "ppapi/proxy/proxy_completion_callback_factory.h"
#include "ppapi/shared_impl/host_resource.h"
#include "ppapi/shared_impl/ppb_instance_shared.h"
#include "ppapi/shared_impl/singleton_resource_id.h"
#include "ppapi/thunk/ppb_instance_api.h"
#include "ppapi/utility/completion_callback_factory.h"

// Windows headers interfere with this file.
#ifdef PostMessage
#undef PostMessage
#endif

struct PP_DecryptedBlockInfo;
struct PP_DecryptedFrameInfo;

namespace ppapi {
namespace proxy {

class SerializedVarReceiveInput;
class SerializedVarOutParam;
class SerializedVarReturnValue;

class PPB_Instance_Proxy : public InterfaceProxy,
                           public PPB_Instance_Shared {
 public:
  PPB_Instance_Proxy(Dispatcher* dispatcher);
  virtual ~PPB_Instance_Proxy();

  // InterfaceProxy implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg);

  // PPB_Instance_API implementation.
  virtual PP_Bool BindGraphics(PP_Instance instance,
                               PP_Resource device) OVERRIDE;
  virtual PP_Bool IsFullFrame(PP_Instance instance) OVERRIDE;
  virtual const ViewData* GetViewData(PP_Instance instance) OVERRIDE;
  virtual PP_Bool FlashIsFullscreen(PP_Instance instance) OVERRIDE;
  virtual PP_Var GetWindowObject(PP_Instance instance) OVERRIDE;
  virtual PP_Var GetOwnerElementObject(PP_Instance instance) OVERRIDE;
  virtual PP_Var ExecuteScript(PP_Instance instance,
                               PP_Var script,
                               PP_Var* exception) OVERRIDE;
  virtual uint32_t GetAudioHardwareOutputSampleRate(PP_Instance instance)
      OVERRIDE;
  virtual uint32_t GetAudioHardwareOutputBufferSize(PP_Instance instance)
      OVERRIDE;
  virtual PP_Var GetDefaultCharSet(PP_Instance instance) OVERRIDE;
  virtual void SetPluginToHandleFindRequests(PP_Instance instance) OVERRIDE;
  virtual void NumberOfFindResultsChanged(PP_Instance instance,
                                          int32_t total,
                                          PP_Bool final_result) OVERRIDE;
  virtual void SelectedFindResultChanged(PP_Instance instance,
                                         int32_t index) OVERRIDE;
  virtual void SetTickmarks(PP_Instance instance,
                            const PP_Rect* tickmarks,
                            uint32_t count) OVERRIDE;
  virtual PP_Bool IsFullscreen(PP_Instance instance) OVERRIDE;
  virtual PP_Bool SetFullscreen(PP_Instance instance,
                                PP_Bool fullscreen) OVERRIDE;
  virtual PP_Bool GetScreenSize(PP_Instance instance,
                                PP_Size* size) OVERRIDE;
  virtual Resource* GetSingletonResource(PP_Instance instance,
                                         SingletonResourceID id) OVERRIDE;
  virtual int32_t RequestInputEvents(PP_Instance instance,
                                     uint32_t event_classes) OVERRIDE;
  virtual int32_t RequestFilteringInputEvents(PP_Instance instance,
                                              uint32_t event_classes) OVERRIDE;
  virtual void StartTrackingLatency(PP_Instance instance) OVERRIDE;
  virtual void ClearInputEventRequest(PP_Instance instance,
                                      uint32_t event_classes) OVERRIDE;
  virtual void ZoomChanged(PP_Instance instance, double factor) OVERRIDE;
  virtual void ZoomLimitsChanged(PP_Instance instance,
                                 double minimum_factor,
                                 double maximium_factor) OVERRIDE;
  virtual void PostMessage(PP_Instance instance, PP_Var message) OVERRIDE;
  virtual int32_t RegisterMessageHandler(PP_Instance instance,
                                         void* user_data,
                                         const PPP_MessageHandler_0_1* handler,
                                         PP_Resource message_loop) OVERRIDE;
  virtual void UnregisterMessageHandler(PP_Instance instance) OVERRIDE;
  virtual PP_Bool SetCursor(PP_Instance instance,
                            PP_MouseCursor_Type type,
                            PP_Resource image,
                            const PP_Point* hot_spot) OVERRIDE;
  virtual int32_t LockMouse(PP_Instance instance,
                            scoped_refptr<TrackedCallback> callback) OVERRIDE;
  virtual void UnlockMouse(PP_Instance instance) OVERRIDE;
  virtual void SetTextInputType(PP_Instance instance,
                                PP_TextInput_Type type) OVERRIDE;
  virtual void UpdateCaretPosition(PP_Instance instance,
                                   const PP_Rect& caret,
                                   const PP_Rect& bounding_box) OVERRIDE;
  virtual void CancelCompositionText(PP_Instance instance) OVERRIDE;
  virtual void SelectionChanged(PP_Instance instance) OVERRIDE;
  virtual void UpdateSurroundingText(PP_Instance instance,
                                     const char* text,
                                     uint32_t caret,
                                     uint32_t anchor) OVERRIDE;
  virtual PP_Var GetDocumentURL(PP_Instance instance,
                                PP_URLComponents_Dev* components) OVERRIDE;
#if !defined(OS_NACL)
  virtual PP_Var ResolveRelativeToDocument(
      PP_Instance instance,
      PP_Var relative,
      PP_URLComponents_Dev* components) OVERRIDE;
  virtual PP_Bool DocumentCanRequest(PP_Instance instance, PP_Var url) OVERRIDE;
  virtual PP_Bool DocumentCanAccessDocument(PP_Instance instance,
                                            PP_Instance target) OVERRIDE;
  virtual PP_Var GetPluginInstanceURL(
      PP_Instance instance,
      PP_URLComponents_Dev* components) OVERRIDE;
  virtual PP_Var GetPluginReferrerURL(
      PP_Instance instance,
      PP_URLComponents_Dev* components) OVERRIDE;
  virtual void PromiseResolved(PP_Instance instance,
                               uint32 promise_id) OVERRIDE;
  virtual void PromiseResolvedWithSession(PP_Instance instance,
                                          uint32 promise_id,
                                          PP_Var web_session_id_var) OVERRIDE;
  virtual void PromiseRejected(PP_Instance instance,
                               uint32 promise_id,
                               PP_CdmExceptionCode exception_code,
                               uint32 system_code,
                               PP_Var error_description_var) OVERRIDE;
  virtual void SessionMessage(PP_Instance instance,
                              PP_Var web_session_id_var,
                              PP_Var message_var,
                              PP_Var destination_url_var) OVERRIDE;
  virtual void SessionReady(PP_Instance instance,
                            PP_Var web_session_id_var) OVERRIDE;
  virtual void SessionClosed(PP_Instance instance,
                             PP_Var web_session_id_var) OVERRIDE;
  virtual void SessionError(PP_Instance instance,
                            PP_Var web_session_id_var,
                            PP_CdmExceptionCode exception_code,
                            uint32 system_code,
                            PP_Var error_description_var) OVERRIDE;
  virtual void DeliverBlock(PP_Instance instance,
                            PP_Resource decrypted_block,
                            const PP_DecryptedBlockInfo* block_info) OVERRIDE;
  virtual void DecoderInitializeDone(PP_Instance instance,
                                     PP_DecryptorStreamType decoder_type,
                                     uint32_t request_id,
                                     PP_Bool success) OVERRIDE;
  virtual void DecoderDeinitializeDone(PP_Instance instance,
                                       PP_DecryptorStreamType decoder_type,
                                       uint32_t request_id) OVERRIDE;
  virtual void DecoderResetDone(PP_Instance instance,
                                PP_DecryptorStreamType decoder_type,
                                uint32_t request_id) OVERRIDE;
  virtual void DeliverFrame(PP_Instance instance,
                            PP_Resource decrypted_frame,
                            const PP_DecryptedFrameInfo* frame_info) OVERRIDE;
  virtual void DeliverSamples(
      PP_Instance instance,
      PP_Resource audio_frames,
      const PP_DecryptedSampleInfo* sample_info) OVERRIDE;
#endif  // !defined(OS_NACL)

  static const ApiID kApiID = API_ID_PPB_INSTANCE;

 private:
  // Plugin -> Host message handlers.
  void OnHostMsgGetWindowObject(PP_Instance instance,
                                SerializedVarReturnValue result);
  void OnHostMsgGetOwnerElementObject(PP_Instance instance,
                                      SerializedVarReturnValue result);
  void OnHostMsgBindGraphics(PP_Instance instance,
                             PP_Resource device);
  void OnHostMsgIsFullFrame(PP_Instance instance, PP_Bool* result);
  void OnHostMsgExecuteScript(PP_Instance instance,
                              SerializedVarReceiveInput script,
                              SerializedVarOutParam out_exception,
                              SerializedVarReturnValue result);
  void OnHostMsgGetAudioHardwareOutputSampleRate(PP_Instance instance,
                                                 uint32_t *result);
  void OnHostMsgGetAudioHardwareOutputBufferSize(PP_Instance instance,
                                                 uint32_t *result);
  void OnHostMsgGetDefaultCharSet(PP_Instance instance,
                                  SerializedVarReturnValue result);
  void OnHostMsgSetPluginToHandleFindRequests(PP_Instance instance);
  void OnHostMsgNumberOfFindResultsChanged(PP_Instance instance,
                                           int32_t total,
                                           PP_Bool final_result);
  void OnHostMsgSelectFindResultChanged(PP_Instance instance,
                                        int32_t index);
  void OnHostMsgSetTickmarks(PP_Instance instance,
                             const std::vector<PP_Rect>& tickmarks);
  void OnHostMsgSetFullscreen(PP_Instance instance,
                              PP_Bool fullscreen,
                              PP_Bool* result);
  void OnHostMsgGetScreenSize(PP_Instance instance,
                              PP_Bool* result,
                              PP_Size* size);
  void OnHostMsgRequestInputEvents(PP_Instance instance,
                                   bool is_filtering,
                                   uint32_t event_classes);
  void OnHostMsgStartTrackingLatency(PP_Instance instance);
  void OnHostMsgClearInputEvents(PP_Instance instance,
                                 uint32_t event_classes);
  void OnHostMsgPostMessage(PP_Instance instance,
                            SerializedVarReceiveInput message);
  void OnHostMsgLockMouse(PP_Instance instance);
  void OnHostMsgUnlockMouse(PP_Instance instance);
  void OnHostMsgSetCursor(PP_Instance instance,
                          int32_t type,
                          const ppapi::HostResource& custom_image,
                          const PP_Point& hot_spot);
  void OnHostMsgSetTextInputType(PP_Instance instance, PP_TextInput_Type type);
  void OnHostMsgUpdateCaretPosition(PP_Instance instance,
                                    const PP_Rect& caret,
                                    const PP_Rect& bounding_box);
  void OnHostMsgCancelCompositionText(PP_Instance instance);
  void OnHostMsgUpdateSurroundingText(
      PP_Instance instance,
      const std::string& text,
      uint32_t caret,
      uint32_t anchor);
  void OnHostMsgGetDocumentURL(PP_Instance instance,
                               PP_URLComponents_Dev* components,
                               SerializedVarReturnValue result);

#if !defined(OS_NACL)
  void OnHostMsgResolveRelativeToDocument(PP_Instance instance,
                                          SerializedVarReceiveInput relative,
                                          SerializedVarReturnValue result);
  void OnHostMsgDocumentCanRequest(PP_Instance instance,
                                   SerializedVarReceiveInput url,
                                   PP_Bool* result);
  void OnHostMsgDocumentCanAccessDocument(PP_Instance active,
                                          PP_Instance target,
                                          PP_Bool* result);
  void OnHostMsgGetPluginInstanceURL(PP_Instance instance,
                                     SerializedVarReturnValue result);
  void OnHostMsgGetPluginReferrerURL(PP_Instance instance,
                                     SerializedVarReturnValue result);

  virtual void OnHostMsgPromiseResolved(PP_Instance instance,
                                        uint32_t promise_id);
  virtual void OnHostMsgPromiseResolvedWithSession(
      PP_Instance instance,
      uint32_t promise_id,
      SerializedVarReceiveInput web_session_id);
  virtual void OnHostMsgPromiseRejected(
      PP_Instance instance,
      uint32_t promise_id,
      PP_CdmExceptionCode exception_code,
      uint32_t system_code,
      SerializedVarReceiveInput error_description);
  virtual void OnHostMsgSessionMessage(
      PP_Instance instance,
      SerializedVarReceiveInput web_session_id,
      SerializedVarReceiveInput message,
      SerializedVarReceiveInput destination_url);
  virtual void OnHostMsgSessionReady(PP_Instance instance,
                                     SerializedVarReceiveInput web_session_id);
  virtual void OnHostMsgSessionClosed(PP_Instance instance,
                                      SerializedVarReceiveInput web_session_id);
  virtual void OnHostMsgSessionError(
      PP_Instance instance,
      SerializedVarReceiveInput web_session_id,
      PP_CdmExceptionCode exception_code,
      uint32_t system_code,
      SerializedVarReceiveInput error_description);
  virtual void OnHostMsgDecoderInitializeDone(
      PP_Instance instance,
      PP_DecryptorStreamType decoder_type,
      uint32_t request_id,
      PP_Bool success);
  virtual void OnHostMsgDecoderDeinitializeDone(
      PP_Instance instance,
      PP_DecryptorStreamType decoder_type,
      uint32_t request_id);
  virtual void OnHostMsgDecoderResetDone(PP_Instance instance,
                                         PP_DecryptorStreamType decoder_type,
                                         uint32_t request_id);
  virtual void OnHostMsgDeliverBlock(PP_Instance instance,
                                     PP_Resource decrypted_block,
                                     const std::string& serialized_block_info);
  virtual void OnHostMsgDeliverFrame(PP_Instance instance,
                                     PP_Resource decrypted_frame,
                                     const std::string& serialized_block_info);
  virtual void OnHostMsgDeliverSamples(
      PP_Instance instance,
      PP_Resource audio_frames,
      const std::string& serialized_sample_info);
#endif  // !defined(OS_NACL)

  // Host -> Plugin message handlers.
  void OnPluginMsgMouseLockComplete(PP_Instance instance, int32_t result);

  void MouseLockCompleteInHost(int32_t result, PP_Instance instance);

  // Other helpers.
  void CancelAnyPendingRequestSurroundingText(PP_Instance instance);

  ProxyCompletionCallbackFactory<PPB_Instance_Proxy> callback_factory_;
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PPB_INSTANCE_PROXY_H_
