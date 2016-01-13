// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/examples/pepper_container_app/plugin_instance.h"

#include "base/logging.h"
#include "mojo/examples/pepper_container_app/graphics_3d_resource.h"
#include "mojo/examples/pepper_container_app/mojo_ppapi_globals.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/c/ppp_graphics_3d.h"
#include "ppapi/c/ppp_instance.h"
#include "ppapi/shared_impl/ppb_view_shared.h"
#include "ppapi/shared_impl/proxy_lock.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_graphics_3d_api.h"

namespace mojo {
namespace examples {

PluginInstance::PluginInstance(scoped_refptr<PluginModule> plugin_module)
    : pp_instance_(0),
      plugin_module_(plugin_module) {
  pp_instance_ = MojoPpapiGlobals::Get()->AddInstance(this);
}

PluginInstance::~PluginInstance() {
  MojoPpapiGlobals::Get()->InstanceDeleted(pp_instance_);
}

bool PluginInstance::DidCreate() {
  ppapi::ProxyAutoUnlock unlock;
  const PPP_Instance_1_1* instance_interface =
      static_cast<const PPP_Instance_1_1*>(plugin_module_->GetPluginInterface(
          PPP_INSTANCE_INTERFACE_1_1));
  return !!instance_interface->DidCreate(pp_instance(), 0, NULL, NULL);
}

void PluginInstance::DidDestroy() {
  ppapi::ProxyAutoUnlock unlock;
  const PPP_Instance_1_1* instance_interface =
      static_cast<const PPP_Instance_1_1*>(plugin_module_->GetPluginInterface(
          PPP_INSTANCE_INTERFACE_1_1));
  instance_interface->DidDestroy(pp_instance());
}

void PluginInstance::DidChangeView(const PP_Rect& bounds) {
  ppapi::ViewData view_data;
  view_data.rect = bounds;
  view_data.is_fullscreen = false;
  view_data.is_page_visible = true;
  view_data.clip_rect = bounds;
  view_data.device_scale = 1.0f;
  view_data.css_scale = 1.0f;

  ppapi::ScopedPPResource resource(ppapi::ScopedPPResource::PassRef(),
      (new ppapi::PPB_View_Shared(
          ppapi::OBJECT_IS_IMPL, pp_instance(), view_data))->GetReference());
  {
    ppapi::ProxyAutoUnlock unlock;
    const PPP_Instance_1_1* instance_interface =
        static_cast<const PPP_Instance_1_1*>(plugin_module_->GetPluginInterface(
            PPP_INSTANCE_INTERFACE_1_1));
    instance_interface->DidChangeView(pp_instance(), resource);
  }
}

void PluginInstance::Graphics3DContextLost() {
  ppapi::ProxyAutoUnlock unlock;
  const PPP_Graphics3D_1_0* graphic_3d_interface =
      static_cast<const PPP_Graphics3D_1_0*>(plugin_module_->GetPluginInterface(
          PPP_GRAPHICS_3D_INTERFACE_1_0));
  // TODO(yzshen): Maybe we only need to notify for the bound graphics context?
  graphic_3d_interface->Graphics3DContextLost(pp_instance());
}

bool PluginInstance::IsBoundGraphics(PP_Resource device) const {
  return device != 0 && device == bound_graphics_.get();
}

PP_Bool PluginInstance::BindGraphics(PP_Instance instance, PP_Resource device) {
  if (bound_graphics_.get() == device)
    return PP_TRUE;

  ppapi::thunk::EnterResourceNoLock<ppapi::thunk::PPB_Graphics3D_API>
      enter(device, false);
  if (enter.failed())
    return PP_FALSE;

  bound_graphics_ = device;
  static_cast<Graphics3DResource*>(enter.object())->BindGraphics();

  return PP_TRUE;
}

PP_Bool PluginInstance::IsFullFrame(PP_Instance instance) {
  NOTIMPLEMENTED();
  return PP_FALSE;
}

const ppapi::ViewData* PluginInstance::GetViewData(PP_Instance instance) {
  NOTIMPLEMENTED();
  return NULL;
}

PP_Bool PluginInstance::FlashIsFullscreen(PP_Instance instance) {
  NOTIMPLEMENTED();
  return PP_FALSE;
}

PP_Var PluginInstance::GetWindowObject(PP_Instance instance) {
  NOTIMPLEMENTED();
  return PP_MakeUndefined();
}

PP_Var PluginInstance::GetOwnerElementObject(PP_Instance instance) {
  NOTIMPLEMENTED();
  return PP_MakeUndefined();
}

PP_Var PluginInstance::ExecuteScript(PP_Instance instance,
                                     PP_Var script,
                                     PP_Var* exception) {
  NOTIMPLEMENTED();
  return PP_MakeUndefined();
}

uint32_t PluginInstance::GetAudioHardwareOutputSampleRate(
    PP_Instance instance) {
  NOTIMPLEMENTED();
  return 0;
}

uint32_t PluginInstance::GetAudioHardwareOutputBufferSize(
    PP_Instance instance) {
  NOTIMPLEMENTED();
  return 0;
}

PP_Var PluginInstance::GetDefaultCharSet(PP_Instance instance) {
  NOTIMPLEMENTED();
  return PP_MakeUndefined();
}

void PluginInstance::Log(PP_Instance instance,
                         PP_LogLevel log_level,
                         PP_Var value) {
  NOTIMPLEMENTED();
}

void PluginInstance::LogWithSource(PP_Instance instance,
                                   PP_LogLevel log_level,
                                   PP_Var source,
                                   PP_Var value) {
  NOTIMPLEMENTED();
}

void PluginInstance::SetPluginToHandleFindRequests(PP_Instance instance) {
  NOTIMPLEMENTED();
}

void PluginInstance::NumberOfFindResultsChanged(PP_Instance instance,
                                                int32_t total,
                                                PP_Bool final_result) {
  NOTIMPLEMENTED();
}

void PluginInstance::SelectedFindResultChanged(PP_Instance instance,
                                               int32_t index) {
  NOTIMPLEMENTED();
}

void PluginInstance::SetTickmarks(PP_Instance instance,
                                  const PP_Rect* tickmarks,
                                  uint32_t count) {
  NOTIMPLEMENTED();
}

PP_Bool PluginInstance::IsFullscreen(PP_Instance instance) {
  NOTIMPLEMENTED();
  return PP_FALSE;
}

PP_Bool PluginInstance::SetFullscreen(PP_Instance instance,
                                      PP_Bool fullscreen) {
  NOTIMPLEMENTED();
  return PP_FALSE;
}

PP_Bool PluginInstance::GetScreenSize(PP_Instance instance, PP_Size* size) {
  NOTIMPLEMENTED();
  return PP_FALSE;
}

ppapi::Resource* PluginInstance::GetSingletonResource(
    PP_Instance instance,
    ppapi::SingletonResourceID id) {
  NOTIMPLEMENTED();
  return NULL;
}

int32_t PluginInstance::RequestInputEvents(PP_Instance instance,
                                           uint32_t event_classes) {
  NOTIMPLEMENTED();
  return PP_ERROR_FAILED;
}

int32_t PluginInstance::RequestFilteringInputEvents(PP_Instance instance,
                                                    uint32_t event_classes) {
  NOTIMPLEMENTED();
  return PP_ERROR_FAILED;
}

void PluginInstance::ClearInputEventRequest(PP_Instance instance,
                                            uint32_t event_classes) {
  NOTIMPLEMENTED();
}

void PluginInstance::StartTrackingLatency(PP_Instance instance) {
  NOTIMPLEMENTED();
}

void PluginInstance::PostMessage(PP_Instance instance, PP_Var message) {
  NOTIMPLEMENTED();
}

int32_t PluginInstance::RegisterMessageHandler(
    PP_Instance instance,
    void* user_data,
    const PPP_MessageHandler_0_1* handler,
    PP_Resource message_loop) {
  NOTIMPLEMENTED();
  return PP_ERROR_FAILED;
}

void PluginInstance::UnregisterMessageHandler(PP_Instance instance) {
  NOTIMPLEMENTED();
}

PP_Bool PluginInstance::SetCursor(PP_Instance instance,
                                  PP_MouseCursor_Type type,
                                  PP_Resource image,
                                  const PP_Point* hot_spot) {
  NOTIMPLEMENTED();
  return PP_FALSE;
}

int32_t PluginInstance::LockMouse(
    PP_Instance instance,
    scoped_refptr<ppapi::TrackedCallback> callback) {
  NOTIMPLEMENTED();
  return PP_ERROR_FAILED;
}

void PluginInstance::UnlockMouse(PP_Instance instance) {
  NOTIMPLEMENTED();
}

void PluginInstance::SetTextInputType(PP_Instance instance,
                                      PP_TextInput_Type type) {
  NOTIMPLEMENTED();
}

void PluginInstance::UpdateCaretPosition(PP_Instance instance,
                                         const PP_Rect& caret,
                                         const PP_Rect& bounding_box) {
  NOTIMPLEMENTED();
}

void PluginInstance::CancelCompositionText(PP_Instance instance) {
  NOTIMPLEMENTED();
}

void PluginInstance::SelectionChanged(PP_Instance instance) {
  NOTIMPLEMENTED();
}

void PluginInstance::UpdateSurroundingText(PP_Instance instance,
                                           const char* text,
                                           uint32_t caret,
                                           uint32_t anchor) {
  NOTIMPLEMENTED();
}

void PluginInstance::ZoomChanged(PP_Instance instance, double factor) {
  NOTIMPLEMENTED();
}

void PluginInstance::ZoomLimitsChanged(PP_Instance instance,
                                       double minimum_factor,
                                       double maximum_factor) {
  NOTIMPLEMENTED();
}

PP_Var PluginInstance::GetDocumentURL(PP_Instance instance,
                                      PP_URLComponents_Dev* components) {
  NOTIMPLEMENTED();
  return PP_MakeUndefined();
}

void PluginInstance::PromiseResolved(PP_Instance instance, uint32 promise_id) {
  NOTIMPLEMENTED();
}

void PluginInstance::PromiseResolvedWithSession(PP_Instance instance,
                                                uint32 promise_id,
                                                PP_Var web_session_id_var) {
  NOTIMPLEMENTED();
}

void PluginInstance::PromiseRejected(PP_Instance instance,
                                     uint32 promise_id,
                                     PP_CdmExceptionCode exception_code,
                                     uint32 system_code,
                                     PP_Var error_description_var) {
  NOTIMPLEMENTED();
}

void PluginInstance::SessionMessage(PP_Instance instance,
                                    PP_Var web_session_id_var,
                                    PP_Var message_var,
                                    PP_Var destination_url_var) {
  NOTIMPLEMENTED();
}

void PluginInstance::SessionReady(PP_Instance instance,
                                  PP_Var web_session_id_var) {
  NOTIMPLEMENTED();
}

void PluginInstance::SessionClosed(PP_Instance instance,
                                   PP_Var web_session_id_var) {
  NOTIMPLEMENTED();
}

void PluginInstance::SessionError(PP_Instance instance,
                                  PP_Var web_session_id_var,
                                  PP_CdmExceptionCode exception_code,
                                  uint32 system_code,
                                  PP_Var error_description_var) {
  NOTIMPLEMENTED();
}

void PluginInstance::DeliverBlock(PP_Instance instance,
                                  PP_Resource decrypted_block,
                                  const PP_DecryptedBlockInfo* block_info) {
  NOTIMPLEMENTED();
}

void PluginInstance::DecoderInitializeDone(PP_Instance instance,
                                           PP_DecryptorStreamType decoder_type,
                                           uint32_t request_id,
                                           PP_Bool success) {
  NOTIMPLEMENTED();
}

void PluginInstance::DecoderDeinitializeDone(
    PP_Instance instance,
    PP_DecryptorStreamType decoder_type,
    uint32_t request_id) {
  NOTIMPLEMENTED();
}

void PluginInstance::DecoderResetDone(PP_Instance instance,
                                      PP_DecryptorStreamType decoder_type,
                                      uint32_t request_id) {
  NOTIMPLEMENTED();
}

void PluginInstance::DeliverFrame(PP_Instance instance,
                                  PP_Resource decrypted_frame,
                                  const PP_DecryptedFrameInfo* frame_info) {
  NOTIMPLEMENTED();
}

void PluginInstance::DeliverSamples(PP_Instance instance,
                                    PP_Resource audio_frames,
                                    const PP_DecryptedSampleInfo* sample_info) {
  NOTIMPLEMENTED();
}

PP_Var PluginInstance::ResolveRelativeToDocument(
    PP_Instance instance,
    PP_Var relative,
    PP_URLComponents_Dev* components) {
  NOTIMPLEMENTED();
  return PP_MakeUndefined();
}

PP_Bool PluginInstance::DocumentCanRequest(PP_Instance instance, PP_Var url) {
  NOTIMPLEMENTED();
  return PP_FALSE;
}

PP_Bool PluginInstance::DocumentCanAccessDocument(PP_Instance instance,
                                                  PP_Instance target) {
  NOTIMPLEMENTED();
  return PP_FALSE;
}

PP_Var PluginInstance::GetPluginInstanceURL(PP_Instance instance,
                                            PP_URLComponents_Dev* components) {
  NOTIMPLEMENTED();
  return PP_MakeUndefined();
}

PP_Var PluginInstance::GetPluginReferrerURL(PP_Instance instance,
                                            PP_URLComponents_Dev* components) {
  NOTIMPLEMENTED();
  return PP_MakeUndefined();
}

}  // namespace examples
}  // namespace mojo
