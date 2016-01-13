// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EXAMPLES_PEPPER_CONTAINER_APP_PLUGIN_INSTANCE_H_
#define MOJO_EXAMPLES_PEPPER_CONTAINER_APP_PLUGIN_INSTANCE_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "mojo/examples/pepper_container_app/plugin_module.h"
#include "mojo/examples/pepper_container_app/resource_creation_impl.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/shared_impl/scoped_pp_resource.h"
#include "ppapi/thunk/ppb_instance_api.h"

namespace mojo {
namespace examples {

class PluginInstance : public ppapi::thunk::PPB_Instance_API {
 public:
  explicit PluginInstance(scoped_refptr<PluginModule> plugin_module);
  virtual ~PluginInstance();

  // Notifies the plugin that a new instance has been created.
  bool DidCreate();
  // Notifies the plugin that the instance has been destroyed.
  void DidDestroy();
  // Notifies the plugin that the position or size of the instance has changed.
  void DidChangeView(const PP_Rect& bounds);
  // Notifies the plugin that the Graphics 3D context has been invalidated.
  void Graphics3DContextLost();

  // Returns true if |device| has been bound as the current display surface.
  bool IsBoundGraphics(PP_Resource device) const;

  PP_Instance pp_instance() const { return pp_instance_; }
  ResourceCreationImpl* resource_creation() { return &resource_creation_; }
  PluginModule* plugin_module() { return plugin_module_.get(); }

  // ppapi::thunk::PPB_Instance_API implementation.
  virtual PP_Bool BindGraphics(PP_Instance instance,
                               PP_Resource device) OVERRIDE;
  virtual PP_Bool IsFullFrame(PP_Instance instance) OVERRIDE;
  virtual const ppapi::ViewData* GetViewData(PP_Instance instance) OVERRIDE;
  virtual PP_Bool FlashIsFullscreen(PP_Instance instance) OVERRIDE;
  virtual PP_Var GetWindowObject(PP_Instance instance) OVERRIDE;
  virtual PP_Var GetOwnerElementObject(PP_Instance instance) OVERRIDE;
  virtual PP_Var ExecuteScript(PP_Instance instance,
                               PP_Var script,
                               PP_Var* exception) OVERRIDE;
  virtual uint32_t GetAudioHardwareOutputSampleRate(
      PP_Instance instance) OVERRIDE;
  virtual uint32_t GetAudioHardwareOutputBufferSize(
      PP_Instance instance) OVERRIDE;
  virtual PP_Var GetDefaultCharSet(PP_Instance instance) OVERRIDE;
  virtual void Log(PP_Instance instance,
                   PP_LogLevel log_level,
                   PP_Var value) OVERRIDE;
  virtual void LogWithSource(PP_Instance instance,
                             PP_LogLevel log_level,
                             PP_Var source,
                             PP_Var value) OVERRIDE;
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
  virtual PP_Bool GetScreenSize(PP_Instance instance, PP_Size* size) OVERRIDE;
  virtual ppapi::Resource* GetSingletonResource(
      PP_Instance instance, ppapi::SingletonResourceID id) OVERRIDE;
  virtual int32_t RequestInputEvents(PP_Instance instance,
                                     uint32_t event_classes) OVERRIDE;
  virtual int32_t RequestFilteringInputEvents(PP_Instance instance,
                                              uint32_t event_classes) OVERRIDE;
  virtual void ClearInputEventRequest(PP_Instance instance,
                                      uint32_t event_classes) OVERRIDE;
  virtual void StartTrackingLatency(PP_Instance instance) OVERRIDE;
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
  virtual int32_t LockMouse(
      PP_Instance instance,
      scoped_refptr<ppapi::TrackedCallback> callback) OVERRIDE;
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
  virtual void ZoomChanged(PP_Instance instance, double factor) OVERRIDE;
  virtual void ZoomLimitsChanged(PP_Instance instance,
                                 double minimum_factor,
                                 double maximum_factor) OVERRIDE;
  virtual PP_Var GetDocumentURL(PP_Instance instance,
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

 private:
  PP_Instance pp_instance_;
  ResourceCreationImpl resource_creation_;
  scoped_refptr<PluginModule> plugin_module_;
  ppapi::ScopedPPResource bound_graphics_;

  DISALLOW_COPY_AND_ASSIGN(PluginInstance);
};

}  // namespace examples
}  // namespace mojo

#endif  // MOJO_EXAMPLES_PEPPER_CONTAINER_APP_PLUGIN_INSTANCE_H_
