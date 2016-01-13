// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/examples/pepper_container_app/resource_creation_impl.h"

#include "base/logging.h"
#include "mojo/examples/pepper_container_app/graphics_3d_resource.h"

namespace mojo {
namespace examples {

ResourceCreationImpl::ResourceCreationImpl() {}

ResourceCreationImpl::~ResourceCreationImpl() {}

PP_Resource ResourceCreationImpl::CreateFileIO(PP_Instance instance) {
  NOTIMPLEMENTED();
  return 0;
}

PP_Resource ResourceCreationImpl::CreateFileRef(
    PP_Instance instance,
    const ppapi::FileRefCreateInfo& create_info) {
  NOTIMPLEMENTED();
  return 0;
}

PP_Resource ResourceCreationImpl::CreateFileSystem(
    PP_Instance instance,
    PP_FileSystemType type) {
  NOTIMPLEMENTED();
  return 0;
}

PP_Resource ResourceCreationImpl::CreateIMEInputEvent(
    PP_Instance instance,
    PP_InputEvent_Type type,
    PP_TimeTicks time_stamp,
    struct PP_Var text,
    uint32_t segment_number,
    const uint32_t* segment_offsets,
    int32_t target_segment,
    uint32_t selection_start,
    uint32_t selection_end) {
  NOTIMPLEMENTED();
  return 0;
}

PP_Resource ResourceCreationImpl::CreateKeyboardInputEvent_1_0(
    PP_Instance instance,
    PP_InputEvent_Type type,
    PP_TimeTicks time_stamp,
    uint32_t modifiers,
    uint32_t key_code,
    struct PP_Var character_text) {
  NOTIMPLEMENTED();
  return 0;
}

PP_Resource ResourceCreationImpl::CreateKeyboardInputEvent_1_2(
    PP_Instance instance,
    PP_InputEvent_Type type,
    PP_TimeTicks time_stamp,
    uint32_t modifiers,
    uint32_t key_code,
    struct PP_Var character_text,
    struct PP_Var code) {
  NOTIMPLEMENTED();
  return 0;
}

PP_Resource ResourceCreationImpl::CreateMouseInputEvent(
    PP_Instance instance,
    PP_InputEvent_Type type,
    PP_TimeTicks time_stamp,
    uint32_t modifiers,
    PP_InputEvent_MouseButton mouse_button,
    const PP_Point* mouse_position,
    int32_t click_count,
    const PP_Point* mouse_movement) {
  NOTIMPLEMENTED();
  return 0;
}

PP_Resource ResourceCreationImpl::CreateTouchInputEvent(
    PP_Instance instance,
    PP_InputEvent_Type type,
    PP_TimeTicks time_stamp,
    uint32_t modifiers) {
  NOTIMPLEMENTED();
  return 0;
}

PP_Resource ResourceCreationImpl::CreateTrueTypeFont(
    PP_Instance instance,
    const PP_TrueTypeFontDesc_Dev* desc) {
  NOTIMPLEMENTED();
  return 0;
}

PP_Resource ResourceCreationImpl::CreateURLLoader(PP_Instance instance) {
  NOTIMPLEMENTED();
  return 0;
}

PP_Resource ResourceCreationImpl::CreateURLRequestInfo(
    PP_Instance instance) {
  NOTIMPLEMENTED();
  return 0;
}

PP_Resource ResourceCreationImpl::CreateWheelInputEvent(
    PP_Instance instance,
    PP_TimeTicks time_stamp,
    uint32_t modifiers,
    const PP_FloatPoint* wheel_delta,
    const PP_FloatPoint* wheel_ticks,
    PP_Bool scroll_by_page) {
  NOTIMPLEMENTED();
  return 0;
}

PP_Resource ResourceCreationImpl::CreateAudio1_0(
    PP_Instance instance,
    PP_Resource config_id,
    PPB_Audio_Callback_1_0 audio_callback,
    void* user_data) {
  NOTIMPLEMENTED();
  return 0;
}

PP_Resource ResourceCreationImpl::CreateAudio(
    PP_Instance instance,
    PP_Resource config_id,
    PPB_Audio_Callback audio_callback,
    void* user_data) {
  NOTIMPLEMENTED();
  return 0;
}

PP_Resource ResourceCreationImpl::CreateAudioTrusted(PP_Instance instance) {
  NOTIMPLEMENTED();
  return 0;
}

PP_Resource ResourceCreationImpl::CreateAudioConfig(
    PP_Instance instance,
    PP_AudioSampleRate sample_rate,
    uint32_t sample_frame_count) {
  NOTIMPLEMENTED();
  return 0;
}

PP_Resource ResourceCreationImpl::CreateCompositor(PP_Instance instance) {
  NOTIMPLEMENTED();
  return 0;
}

PP_Resource ResourceCreationImpl::CreateFileChooser(
    PP_Instance instance,
    PP_FileChooserMode_Dev mode,
    const PP_Var& accept_types) {
  NOTIMPLEMENTED();
  return 0;
}

PP_Resource ResourceCreationImpl::CreateGraphics2D(PP_Instance instance,
                                                   const PP_Size* size,
                                                   PP_Bool is_always_opaque) {
  NOTIMPLEMENTED();
  return 0;
}

PP_Resource ResourceCreationImpl::CreateGraphics3D(
    PP_Instance instance,
    PP_Resource share_context,
    const int32_t* attrib_list) {
  return (new Graphics3DResource(instance))->GetReference();
}

PP_Resource ResourceCreationImpl::CreateGraphics3DRaw(
    PP_Instance instance,
    PP_Resource share_context,
    const int32_t* attrib_list) {
  NOTIMPLEMENTED();
  return 0;
}

PP_Resource ResourceCreationImpl::CreateHostResolver(PP_Instance instance) {
  NOTIMPLEMENTED();
  return 0;
}

PP_Resource ResourceCreationImpl::CreateHostResolverPrivate(
    PP_Instance instance) {
  NOTIMPLEMENTED();
  return 0;
}

PP_Resource ResourceCreationImpl::CreateImageData(
    PP_Instance instance,
    PP_ImageDataFormat format,
    const PP_Size* size,
    PP_Bool init_to_zero) {
  NOTIMPLEMENTED();
  return 0;
}

PP_Resource ResourceCreationImpl::CreateImageDataSimple(
    PP_Instance instance,
    PP_ImageDataFormat format,
    const PP_Size* size,
    PP_Bool init_to_zero) {
  NOTIMPLEMENTED();
  return 0;
}

PP_Resource ResourceCreationImpl::CreateMediaStreamVideoTrack(
    PP_Instance instance) {
  NOTIMPLEMENTED();
  return 0;
}

PP_Resource ResourceCreationImpl::CreateNetAddressFromIPv4Address(
    PP_Instance instance,
    const PP_NetAddress_IPv4* ipv4_addr) {
  NOTIMPLEMENTED();
  return 0;
}

PP_Resource ResourceCreationImpl::CreateNetAddressFromIPv6Address(
    PP_Instance instance,
    const PP_NetAddress_IPv6* ipv6_addr) {
  NOTIMPLEMENTED();
  return 0;
}

PP_Resource ResourceCreationImpl::CreateNetAddressFromNetAddressPrivate(
    PP_Instance instance,
    const PP_NetAddress_Private& private_addr) {
  NOTIMPLEMENTED();
  return 0;
}

PP_Resource ResourceCreationImpl::CreateNetworkMonitor(
    PP_Instance instance) {
  NOTIMPLEMENTED();
  return 0;
}

PP_Resource ResourceCreationImpl::CreateOutputProtectionPrivate(
    PP_Instance instance) {
  NOTIMPLEMENTED();
  return 0;
}

PP_Resource ResourceCreationImpl::CreatePrinting(PP_Instance instance) {
  NOTIMPLEMENTED();
  return 0;
}

PP_Resource ResourceCreationImpl::CreateTCPServerSocketPrivate(
    PP_Instance instance) {
  NOTIMPLEMENTED();
  return 0;
}

PP_Resource ResourceCreationImpl::CreateTCPSocket1_0(
    PP_Instance instance) {
  NOTIMPLEMENTED();
  return 0;
}

PP_Resource ResourceCreationImpl::CreateTCPSocket(
    PP_Instance instance) {
  NOTIMPLEMENTED();
  return 0;
}

PP_Resource ResourceCreationImpl::CreateTCPSocketPrivate(
    PP_Instance instance) {
  NOTIMPLEMENTED();
  return 0;
}

PP_Resource ResourceCreationImpl::CreateUDPSocket(PP_Instance instance) {
  NOTIMPLEMENTED();
  return 0;
}

PP_Resource ResourceCreationImpl::CreateUDPSocketPrivate(
    PP_Instance instance) {
  NOTIMPLEMENTED();
  return 0;
}

PP_Resource ResourceCreationImpl::CreateVideoDecoder(PP_Instance instance) {
  NOTIMPLEMENTED();
  return 0;
}

PP_Resource ResourceCreationImpl::CreateVideoDestination(
    PP_Instance instance) {
  NOTIMPLEMENTED();
  return 0;
}

PP_Resource ResourceCreationImpl::CreateVideoSource(
    PP_Instance instance) {
  NOTIMPLEMENTED();
  return 0;
}

PP_Resource ResourceCreationImpl::CreateWebSocket(PP_Instance instance) {
  NOTIMPLEMENTED();
  return 0;
}

PP_Resource ResourceCreationImpl::CreateX509CertificatePrivate(
    PP_Instance instance) {
  NOTIMPLEMENTED();
  return 0;
}

#if !defined(OS_NACL)
PP_Resource ResourceCreationImpl::CreateAudioInput(
    PP_Instance instance) {
  NOTIMPLEMENTED();
  return 0;
}

PP_Resource ResourceCreationImpl::CreateBroker(PP_Instance instance) {
  NOTIMPLEMENTED();
  return 0;
}

PP_Resource ResourceCreationImpl::CreateBrowserFont(
    PP_Instance instance,
    const PP_BrowserFont_Trusted_Description* description) {
  NOTIMPLEMENTED();
  return 0;
}

PP_Resource ResourceCreationImpl::CreateBuffer(PP_Instance instance,
                                                uint32_t size) {
  NOTIMPLEMENTED();
  return 0;
}

PP_Resource ResourceCreationImpl::CreateFlashDRM(PP_Instance instance) {
  NOTIMPLEMENTED();
  return 0;
}

PP_Resource ResourceCreationImpl::CreateFlashFontFile(
    PP_Instance instance,
    const PP_BrowserFont_Trusted_Description* description,
    PP_PrivateFontCharset charset) {
  NOTIMPLEMENTED();
  return 0;
}

PP_Resource ResourceCreationImpl::CreateFlashMenu(
    PP_Instance instance,
    const PP_Flash_Menu* menu_data) {
  NOTIMPLEMENTED();
  return 0;
}

PP_Resource ResourceCreationImpl::CreateFlashMessageLoop(
    PP_Instance instance) {
  NOTIMPLEMENTED();
  return 0;
}

PP_Resource ResourceCreationImpl::CreatePlatformVerificationPrivate(
    PP_Instance instance) {
  NOTIMPLEMENTED();
  return 0;
}

PP_Resource ResourceCreationImpl::CreateScrollbar(PP_Instance instance,
                                                   PP_Bool vertical) {
  NOTIMPLEMENTED();
  return 0;
}

PP_Resource ResourceCreationImpl::CreateTalk(PP_Instance instance) {
  NOTIMPLEMENTED();
  return 0;
}

PP_Resource ResourceCreationImpl::CreateVideoCapture(PP_Instance instance) {
  NOTIMPLEMENTED();
  return 0;
}

PP_Resource ResourceCreationImpl::CreateVideoDecoderDev(
    PP_Instance instance,
    PP_Resource context3d_id,
    PP_VideoDecoder_Profile profile) {
  NOTIMPLEMENTED();
  return 0;
}
#endif  // !defined(OS_NACL)

}  // namespace examples
}  // namespace mojo
