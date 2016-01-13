// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/content_renderer_pepper_host_factory.h"

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "content/public/common/content_client.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/renderer/pepper/pepper_audio_input_host.h"
#include "content/renderer/pepper/pepper_compositor_host.h"
#include "content/renderer/pepper/pepper_file_chooser_host.h"
#include "content/renderer/pepper/pepper_file_ref_renderer_host.h"
#include "content/renderer/pepper/pepper_file_system_host.h"
#include "content/renderer/pepper/pepper_graphics_2d_host.h"
#include "content/renderer/pepper/pepper_media_stream_video_track_host.h"
#include "content/renderer/pepper/pepper_plugin_instance_impl.h"
#include "content/renderer/pepper/pepper_truetype_font_host.h"
#include "content/renderer/pepper/pepper_url_loader_host.h"
#include "content/renderer/pepper/pepper_video_capture_host.h"
#include "content/renderer/pepper/pepper_video_decoder_host.h"
#include "content/renderer/pepper/pepper_video_destination_host.h"
#include "content/renderer/pepper/pepper_video_source_host.h"
#include "content/renderer/pepper/pepper_websocket_host.h"
#include "content/renderer/pepper/ppb_image_data_impl.h"
#include "content/renderer/pepper/renderer_ppapi_host_impl.h"
#include "ppapi/host/resource_host.h"
#include "ppapi/proxy/ppapi_message_utils.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/serialized_structs.h"
#include "ppapi/shared_impl/ppb_image_data_shared.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebPluginContainer.h"

using ppapi::host::ResourceHost;
using ppapi::proxy::SerializedTrueTypeFontDesc;
using ppapi::UnpackMessage;

namespace content {

#if defined(ENABLE_WEBRTC)
namespace {

bool CanUseMediaStreamAPI(const RendererPpapiHost* host, PP_Instance instance) {
  blink::WebPluginContainer* container =
      host->GetContainerForInstance(instance);
  if (!container)
    return false;

  GURL document_url = container->element().document().url();
  ContentRendererClient* content_renderer_client =
      GetContentClient()->renderer();
  return content_renderer_client->AllowPepperMediaStreamAPI(document_url);
}

}  // namespace
#endif  // defined(ENABLE_WEBRTC)

ContentRendererPepperHostFactory::ContentRendererPepperHostFactory(
    RendererPpapiHostImpl* host)
    : host_(host) {}

ContentRendererPepperHostFactory::~ContentRendererPepperHostFactory() {}

scoped_ptr<ResourceHost> ContentRendererPepperHostFactory::CreateResourceHost(
    ppapi::host::PpapiHost* host,
    const ppapi::proxy::ResourceMessageCallParams& params,
    PP_Instance instance,
    const IPC::Message& message) {
  DCHECK(host == host_->GetPpapiHost());

  // Make sure the plugin is giving us a valid instance for this resource.
  if (!host_->IsValidInstance(instance))
    return scoped_ptr<ResourceHost>();

  PepperPluginInstanceImpl* instance_impl =
      host_->GetPluginInstanceImpl(instance);
  if (!instance_impl->render_frame())
    return scoped_ptr<ResourceHost>();

  // Public interfaces.
  switch (message.type()) {
    case PpapiHostMsg_Compositor_Create::ID: {
        return scoped_ptr<ResourceHost>(
            new PepperCompositorHost(host_, instance, params.pp_resource()));
      }
    case PpapiHostMsg_FileRef_CreateForFileAPI::ID: {
      PP_Resource file_system;
      std::string internal_path;
      if (!UnpackMessage<PpapiHostMsg_FileRef_CreateForFileAPI>(
              message, &file_system, &internal_path)) {
        NOTREACHED();
        return scoped_ptr<ResourceHost>();
      }
      return scoped_ptr<ResourceHost>(new PepperFileRefRendererHost(
          host_, instance, params.pp_resource(), file_system, internal_path));
    }
    case PpapiHostMsg_FileSystem_Create::ID: {
      PP_FileSystemType file_system_type;
      if (!UnpackMessage<PpapiHostMsg_FileSystem_Create>(message,
                                                         &file_system_type)) {
        NOTREACHED();
        return scoped_ptr<ResourceHost>();
      }
      return scoped_ptr<ResourceHost>(new PepperFileSystemHost(
          host_, instance, params.pp_resource(), file_system_type));
    }
    case PpapiHostMsg_Graphics2D_Create::ID: {
      PP_Size size;
      PP_Bool is_always_opaque;
      if (!UnpackMessage<PpapiHostMsg_Graphics2D_Create>(
              message, &size, &is_always_opaque)) {
        NOTREACHED();
        return scoped_ptr<ResourceHost>();
      }
      scoped_refptr<PPB_ImageData_Impl> image_data(new PPB_ImageData_Impl(
          instance, ppapi::PPB_ImageData_Shared::PLATFORM));
      return scoped_ptr<ResourceHost>(
          PepperGraphics2DHost::Create(host_,
                                       instance,
                                       params.pp_resource(),
                                       size,
                                       is_always_opaque,
                                       image_data));
    }
    case PpapiHostMsg_URLLoader_Create::ID:
      return scoped_ptr<ResourceHost>(new PepperURLLoaderHost(
          host_, false, instance, params.pp_resource()));
    case PpapiHostMsg_VideoDecoder_Create::ID:
      return scoped_ptr<ResourceHost>(
          new PepperVideoDecoderHost(host_, instance, params.pp_resource()));
    case PpapiHostMsg_WebSocket_Create::ID:
      return scoped_ptr<ResourceHost>(
          new PepperWebSocketHost(host_, instance, params.pp_resource()));
#if defined(ENABLE_WEBRTC)
    case PpapiHostMsg_MediaStreamVideoTrack_Create::ID:
      return scoped_ptr<ResourceHost>(new PepperMediaStreamVideoTrackHost(
          host_, instance, params.pp_resource()));
    // These private MediaStream interfaces are exposed as if they were public
    // so they can be used by NaCl plugins. However, they are available only
    // for whitelisted apps.
    case PpapiHostMsg_VideoDestination_Create::ID:
      if (CanUseMediaStreamAPI(host_, instance))
        return scoped_ptr<ResourceHost>(new PepperVideoDestinationHost(
            host_, instance, params.pp_resource()));
    case PpapiHostMsg_VideoSource_Create::ID:
      if (CanUseMediaStreamAPI(host_, instance))
        return scoped_ptr<ResourceHost>(
            new PepperVideoSourceHost(host_, instance, params.pp_resource()));
#endif  // defined(ENABLE_WEBRTC)
  }

  // Dev interfaces.
  if (GetPermissions().HasPermission(ppapi::PERMISSION_DEV)) {
    switch (message.type()) {
      case PpapiHostMsg_AudioInput_Create::ID:
        return scoped_ptr<ResourceHost>(
            new PepperAudioInputHost(host_, instance, params.pp_resource()));
      case PpapiHostMsg_FileChooser_Create::ID:
        return scoped_ptr<ResourceHost>(
            new PepperFileChooserHost(host_, instance, params.pp_resource()));
      case PpapiHostMsg_TrueTypeFont_Create::ID: {
        SerializedTrueTypeFontDesc desc;
        if (!UnpackMessage<PpapiHostMsg_TrueTypeFont_Create>(message, &desc)) {
          NOTREACHED();
          return scoped_ptr<ResourceHost>();
        }
        // Check that the family name is valid UTF-8 before passing it to the
        // host OS.
        if (base::IsStringUTF8(desc.family)) {
          return scoped_ptr<ResourceHost>(new PepperTrueTypeFontHost(
              host_, instance, params.pp_resource(), desc));
        }
        break;  // Drop through and return null host.
      }
      case PpapiHostMsg_VideoCapture_Create::ID: {
        PepperVideoCaptureHost* host =
            new PepperVideoCaptureHost(host_, instance, params.pp_resource());
        if (!host->Init()) {
          delete host;
          return scoped_ptr<ResourceHost>();
        }
        return scoped_ptr<ResourceHost>(host);
      }
    }
  }

  return scoped_ptr<ResourceHost>();
}

const ppapi::PpapiPermissions&
ContentRendererPepperHostFactory::GetPermissions() const {
  return host_->GetPpapiHost()->permissions();
}

}  // namespace content
