// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_graphics_2d_host.h"

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "cc/resources/shared_bitmap.h"
#include "cc/resources/texture_mailbox.h"
#include "content/child/child_shared_bitmap_manager.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/renderer_ppapi_host.h"
#include "content/renderer/pepper/common.h"
#include "content/renderer/pepper/gfx_conversion.h"
#include "content/renderer/pepper/pepper_plugin_instance_impl.h"
#include "content/renderer/pepper/ppb_image_data_impl.h"
#include "content/renderer/render_thread_impl.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/ppb_view_shared.h"
#include "ppapi/thunk/enter.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/blit.h"
#include "ui/gfx/point_conversions.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/rect_conversions.h"
#include "ui/gfx/scoped_ns_graphics_context_save_gstate_mac.h"
#include "ui/gfx/size_conversions.h"
#include "ui/gfx/skia_util.h"

#if defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#endif

using ppapi::thunk::EnterResourceNoLock;
using ppapi::thunk::PPB_ImageData_API;

namespace content {

namespace {

const int64 kOffscreenCallbackDelayMs = 1000 / 30;  // 30 fps

// Converts a rect inside an image of the given dimensions. The rect may be
// NULL to indicate it should be the entire image. If the rect is outside of
// the image, this will do nothing and return false.
bool ValidateAndConvertRect(const PP_Rect* rect,
                            int image_width,
                            int image_height,
                            gfx::Rect* dest) {
  if (!rect) {
    // Use the entire image area.
    *dest = gfx::Rect(0, 0, image_width, image_height);
  } else {
    // Validate the passed-in area.
    if (rect->point.x < 0 || rect->point.y < 0 || rect->size.width <= 0 ||
        rect->size.height <= 0)
      return false;

    // Check the max bounds, being careful of overflow.
    if (static_cast<int64>(rect->point.x) +
            static_cast<int64>(rect->size.width) >
        static_cast<int64>(image_width))
      return false;
    if (static_cast<int64>(rect->point.y) +
            static_cast<int64>(rect->size.height) >
        static_cast<int64>(image_height))
      return false;

    *dest = gfx::Rect(
        rect->point.x, rect->point.y, rect->size.width, rect->size.height);
  }
  return true;
}

// Converts BGRA <-> RGBA.
void ConvertBetweenBGRAandRGBA(const uint32_t* input,
                               int pixel_length,
                               uint32_t* output) {
  for (int i = 0; i < pixel_length; i++) {
    const unsigned char* pixel_in =
        reinterpret_cast<const unsigned char*>(&input[i]);
    unsigned char* pixel_out = reinterpret_cast<unsigned char*>(&output[i]);
    pixel_out[0] = pixel_in[2];
    pixel_out[1] = pixel_in[1];
    pixel_out[2] = pixel_in[0];
    pixel_out[3] = pixel_in[3];
  }
}

// Converts ImageData from PP_IMAGEDATAFORMAT_BGRA_PREMUL to
// PP_IMAGEDATAFORMAT_RGBA_PREMUL, or reverse. It's assumed that the
// destination image is always mapped (so will have non-NULL data).
void ConvertImageData(PPB_ImageData_Impl* src_image,
                      const SkIRect& src_rect,
                      PPB_ImageData_Impl* dest_image,
                      const SkRect& dest_rect) {
  ImageDataAutoMapper auto_mapper(src_image);

  DCHECK(src_image->format() != dest_image->format());
  DCHECK(PPB_ImageData_Impl::IsImageDataFormatSupported(src_image->format()));
  DCHECK(PPB_ImageData_Impl::IsImageDataFormatSupported(dest_image->format()));

  const SkBitmap* src_bitmap = src_image->GetMappedBitmap();
  const SkBitmap* dest_bitmap = dest_image->GetMappedBitmap();
  if (src_rect.width() == src_image->width() &&
      dest_rect.width() == dest_image->width()) {
    // Fast path if the full line needs to be converted.
    ConvertBetweenBGRAandRGBA(
        src_bitmap->getAddr32(static_cast<int>(src_rect.fLeft),
                              static_cast<int>(src_rect.fTop)),
        src_rect.width() * src_rect.height(),
        dest_bitmap->getAddr32(static_cast<int>(dest_rect.fLeft),
                               static_cast<int>(dest_rect.fTop)));
  } else {
    // Slow path where we convert line by line.
    for (int y = 0; y < src_rect.height(); y++) {
      ConvertBetweenBGRAandRGBA(
          src_bitmap->getAddr32(static_cast<int>(src_rect.fLeft),
                                static_cast<int>(src_rect.fTop + y)),
          src_rect.width(),
          dest_bitmap->getAddr32(static_cast<int>(dest_rect.fLeft),
                                 static_cast<int>(dest_rect.fTop + y)));
    }
  }
}

}  // namespace

struct PepperGraphics2DHost::QueuedOperation {
  enum Type { PAINT, SCROLL, REPLACE, };

  QueuedOperation(Type t)
      : type(t), paint_x(0), paint_y(0), scroll_dx(0), scroll_dy(0) {}

  Type type;

  // Valid when type == PAINT.
  scoped_refptr<PPB_ImageData_Impl> paint_image;
  int paint_x, paint_y;
  gfx::Rect paint_src_rect;

  // Valid when type == SCROLL.
  gfx::Rect scroll_clip_rect;
  int scroll_dx, scroll_dy;

  // Valid when type == REPLACE.
  scoped_refptr<PPB_ImageData_Impl> replace_image;
};

// static
PepperGraphics2DHost* PepperGraphics2DHost::Create(
    RendererPpapiHost* host,
    PP_Instance instance,
    PP_Resource resource,
    const PP_Size& size,
    PP_Bool is_always_opaque,
    scoped_refptr<PPB_ImageData_Impl> backing_store) {
  PepperGraphics2DHost* resource_host =
      new PepperGraphics2DHost(host, instance, resource);
  if (!resource_host->Init(size.width,
                           size.height,
                           PP_ToBool(is_always_opaque),
                           backing_store)) {
    delete resource_host;
    return NULL;
  }
  return resource_host;
}

PepperGraphics2DHost::PepperGraphics2DHost(RendererPpapiHost* host,
                                           PP_Instance instance,
                                           PP_Resource resource)
    : ResourceHost(host->GetPpapiHost(), instance, resource),
      renderer_ppapi_host_(host),
      bound_instance_(NULL),
      need_flush_ack_(false),
      offscreen_flush_pending_(false),
      is_always_opaque_(false),
      scale_(1.0f),
      is_running_in_process_(host->IsRunningInProcess()),
      texture_mailbox_modified_(true) {}

PepperGraphics2DHost::~PepperGraphics2DHost() {
  // Unbind from the instance when destroyed if we're still bound.
  if (bound_instance_)
    bound_instance_->BindGraphics(bound_instance_->pp_instance(), 0);
}

bool PepperGraphics2DHost::Init(
    int width,
    int height,
    bool is_always_opaque,
    scoped_refptr<PPB_ImageData_Impl> backing_store) {
  // The underlying PPB_ImageData_Impl will validate the dimensions.
  image_data_ = backing_store;
  if (!image_data_->Init(PPB_ImageData_Impl::GetNativeImageDataFormat(),
                         width,
                         height,
                         true) ||
      !image_data_->Map()) {
    image_data_ = NULL;
    return false;
  }
  is_always_opaque_ = is_always_opaque;
  scale_ = 1.0f;
  return true;
}

int32_t PepperGraphics2DHost::OnResourceMessageReceived(
    const IPC::Message& msg,
    ppapi::host::HostMessageContext* context) {
  PPAPI_BEGIN_MESSAGE_MAP(PepperGraphics2DHost, msg)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_Graphics2D_PaintImageData,
                                      OnHostMsgPaintImageData)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_Graphics2D_Scroll,
                                      OnHostMsgScroll)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_Graphics2D_ReplaceContents,
                                      OnHostMsgReplaceContents)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_Graphics2D_Flush,
                                      OnHostMsgFlush)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_Graphics2D_SetScale,
                                      OnHostMsgSetScale)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_Graphics2D_ReadImageData,
                                      OnHostMsgReadImageData)
  PPAPI_END_MESSAGE_MAP()
  return PP_ERROR_FAILED;
}

bool PepperGraphics2DHost::IsGraphics2DHost() { return true; }

bool PepperGraphics2DHost::ReadImageData(PP_Resource image,
                                         const PP_Point* top_left) {
  // Get and validate the image object to paint into.
  EnterResourceNoLock<PPB_ImageData_API> enter(image, true);
  if (enter.failed())
    return false;
  PPB_ImageData_Impl* image_resource =
      static_cast<PPB_ImageData_Impl*>(enter.object());
  if (!PPB_ImageData_Impl::IsImageDataFormatSupported(image_resource->format()))
    return false;  // Must be in the right format.

  // Validate the bitmap position.
  int x = top_left->x;
  if (x < 0 ||
      static_cast<int64>(x) + static_cast<int64>(image_resource->width()) >
          image_data_->width())
    return false;
  int y = top_left->y;
  if (y < 0 ||
      static_cast<int64>(y) + static_cast<int64>(image_resource->height()) >
          image_data_->height())
    return false;

  ImageDataAutoMapper auto_mapper(image_resource);
  if (!auto_mapper.is_valid())
    return false;

  SkIRect src_irect = {x, y, x + image_resource->width(),
                       y + image_resource->height()};
  SkRect dest_rect = {SkIntToScalar(0), SkIntToScalar(0),
                      SkIntToScalar(image_resource->width()),
                      SkIntToScalar(image_resource->height())};

  if (image_resource->format() != image_data_->format()) {
    // Convert the image data if the format does not match.
    ConvertImageData(image_data_.get(), src_irect, image_resource, dest_rect);
  } else {
    SkCanvas* dest_canvas = image_resource->GetCanvas();

    // We want to replace the contents of the bitmap rather than blend.
    SkPaint paint;
    paint.setXfermodeMode(SkXfermode::kSrc_Mode);
    dest_canvas->drawBitmapRect(
        *image_data_->GetMappedBitmap(), &src_irect, dest_rect, &paint);
  }
  return true;
}

bool PepperGraphics2DHost::BindToInstance(
    PepperPluginInstanceImpl* new_instance) {
  if (new_instance && new_instance->pp_instance() != pp_instance())
    return false;  // Can't bind other instance's contexts.
  if (bound_instance_ == new_instance)
    return true;  // Rebinding the same device, nothing to do.
  if (bound_instance_ && new_instance)
    return false;  // Can't change a bound device.

  if (!new_instance) {
    // When the device is detached, we'll not get any more paint callbacks so
    // we need to clear the list, but we still want to issue any pending
    // callbacks to the plugin.
    if (need_flush_ack_)
      ScheduleOffscreenFlushAck();
  } else {
    // Devices being replaced, redraw the plugin.
    new_instance->InvalidateRect(gfx::Rect());
  }

  texture_mailbox_modified_ = true;

  bound_instance_ = new_instance;
  return true;
}

// The |backing_bitmap| must be clipped to the |plugin_rect| to avoid painting
// outside the plugin area. This can happen if the plugin has been resized since
// PaintImageData verified the image is within the plugin size.
void PepperGraphics2DHost::Paint(blink::WebCanvas* canvas,
                                 const gfx::Rect& plugin_rect,
                                 const gfx::Rect& paint_rect) {
  TRACE_EVENT0("pepper", "PepperGraphics2DHost::Paint");
  ImageDataAutoMapper auto_mapper(image_data_.get());
  const SkBitmap& backing_bitmap = *image_data_->GetMappedBitmap();

  gfx::Rect invalidate_rect = plugin_rect;
  invalidate_rect.Intersect(paint_rect);
  SkRect sk_invalidate_rect = gfx::RectToSkRect(invalidate_rect);
  SkAutoCanvasRestore auto_restore(canvas, true);
  canvas->clipRect(sk_invalidate_rect);
  gfx::Size pixel_image_size(image_data_->width(), image_data_->height());
  gfx::Size image_size =
      gfx::ToFlooredSize(gfx::ScaleSize(pixel_image_size, scale_));

  PepperPluginInstance* plugin_instance =
      renderer_ppapi_host_->GetPluginInstance(pp_instance());
  if (!plugin_instance)
    return;
  if (plugin_instance->IsFullPagePlugin()) {
    // When we're resizing a window with a full-frame plugin, the plugin may
    // not yet have bound a new device, which will leave parts of the
    // background exposed if the window is getting larger. We want this to
    // show white (typically less jarring) rather than black or uninitialized.
    // We don't do this for non-full-frame plugins since we specifically want
    // the page background to show through.
    SkAutoCanvasRestore auto_restore(canvas, true);
    SkRect image_data_rect =
        gfx::RectToSkRect(gfx::Rect(plugin_rect.origin(), image_size));
    canvas->clipRect(image_data_rect, SkRegion::kDifference_Op);

    SkPaint paint;
    paint.setXfermodeMode(SkXfermode::kSrc_Mode);
    paint.setColor(SK_ColorWHITE);
    canvas->drawRect(sk_invalidate_rect, paint);
  }

  SkBitmap image;
  // Copy to device independent bitmap when target canvas doesn't support
  // platform paint.
  if (!skia::SupportsPlatformPaint(canvas))
    backing_bitmap.copyTo(&image, kPMColor_SkColorType);
  else
    image = backing_bitmap;

  SkPaint paint;
  if (is_always_opaque_) {
    // When we know the device is opaque, we can disable blending for slightly
    // more optimized painting.
    paint.setXfermodeMode(SkXfermode::kSrc_Mode);
  }

  SkPoint origin;
  origin.set(SkIntToScalar(plugin_rect.x()), SkIntToScalar(plugin_rect.y()));

  SkPoint pixel_origin = origin;

  if (scale_ != 1.0f && scale_ > 0.0f) {
    canvas->scale(scale_, scale_);
    pixel_origin.set(pixel_origin.x() * (1.0f / scale_),
                     pixel_origin.y() * (1.0f / scale_));
  }
  canvas->drawBitmap(image, pixel_origin.x(), pixel_origin.y(), &paint);
}

void PepperGraphics2DHost::ViewInitiatedPaint() {}

void PepperGraphics2DHost::ViewFlushedPaint() {
  TRACE_EVENT0("pepper", "PepperGraphics2DHost::ViewFlushedPaint");
  if (need_flush_ack_) {
    SendFlushAck();
    need_flush_ack_ = false;
  }
}

void PepperGraphics2DHost::SetScale(float scale) { scale_ = scale; }

float PepperGraphics2DHost::GetScale() const { return scale_; }

bool PepperGraphics2DHost::IsAlwaysOpaque() const { return is_always_opaque_; }

PPB_ImageData_Impl* PepperGraphics2DHost::ImageData() {
  return image_data_.get();
}

gfx::Size PepperGraphics2DHost::Size() const {
  if (!image_data_)
    return gfx::Size();
  return gfx::Size(image_data_->width(), image_data_->height());
}

int32_t PepperGraphics2DHost::OnHostMsgPaintImageData(
    ppapi::host::HostMessageContext* context,
    const ppapi::HostResource& image_data,
    const PP_Point& top_left,
    bool src_rect_specified,
    const PP_Rect& src_rect) {
  EnterResourceNoLock<PPB_ImageData_API> enter(image_data.host_resource(),
                                               true);
  if (enter.failed())
    return PP_ERROR_BADRESOURCE;
  PPB_ImageData_Impl* image_resource =
      static_cast<PPB_ImageData_Impl*>(enter.object());

  QueuedOperation operation(QueuedOperation::PAINT);
  operation.paint_image = image_resource;
  if (!ValidateAndConvertRect(src_rect_specified ? &src_rect : NULL,
                              image_resource->width(),
                              image_resource->height(),
                              &operation.paint_src_rect))
    return PP_ERROR_BADARGUMENT;

  // Validate the bitmap position using the previously-validated rect, there
  // should be no painted area outside of the image.
  int64 x64 = static_cast<int64>(top_left.x);
  int64 y64 = static_cast<int64>(top_left.y);
  if (x64 + static_cast<int64>(operation.paint_src_rect.x()) < 0 ||
      x64 + static_cast<int64>(operation.paint_src_rect.right()) >
          image_data_->width())
    return PP_ERROR_BADARGUMENT;
  if (y64 + static_cast<int64>(operation.paint_src_rect.y()) < 0 ||
      y64 + static_cast<int64>(operation.paint_src_rect.bottom()) >
          image_data_->height())
    return PP_ERROR_BADARGUMENT;
  operation.paint_x = top_left.x;
  operation.paint_y = top_left.y;

  queued_operations_.push_back(operation);
  return PP_OK;
}

int32_t PepperGraphics2DHost::OnHostMsgScroll(
    ppapi::host::HostMessageContext* context,
    bool clip_specified,
    const PP_Rect& clip,
    const PP_Point& amount) {
  QueuedOperation operation(QueuedOperation::SCROLL);
  if (!ValidateAndConvertRect(clip_specified ? &clip : NULL,
                              image_data_->width(),
                              image_data_->height(),
                              &operation.scroll_clip_rect))
    return PP_ERROR_BADARGUMENT;

  // If we're being asked to scroll by more than the clip rect size, just
  // ignore this scroll command and say it worked.
  int32 dx = amount.x;
  int32 dy = amount.y;
  if (dx <= -image_data_->width() || dx >= image_data_->width() ||
      dy <= -image_data_->height() || dy >= image_data_->height())
    return PP_ERROR_BADARGUMENT;

  operation.scroll_dx = dx;
  operation.scroll_dy = dy;

  queued_operations_.push_back(operation);
  return PP_OK;
}

int32_t PepperGraphics2DHost::OnHostMsgReplaceContents(
    ppapi::host::HostMessageContext* context,
    const ppapi::HostResource& image_data) {
  EnterResourceNoLock<PPB_ImageData_API> enter(image_data.host_resource(),
                                               true);
  if (enter.failed())
    return PP_ERROR_BADRESOURCE;
  PPB_ImageData_Impl* image_resource =
      static_cast<PPB_ImageData_Impl*>(enter.object());

  if (!PPB_ImageData_Impl::IsImageDataFormatSupported(image_resource->format()))
    return PP_ERROR_BADARGUMENT;

  if (image_resource->width() != image_data_->width() ||
      image_resource->height() != image_data_->height())
    return PP_ERROR_BADARGUMENT;

  QueuedOperation operation(QueuedOperation::REPLACE);
  operation.replace_image = image_resource;
  queued_operations_.push_back(operation);
  return PP_OK;
}

int32_t PepperGraphics2DHost::OnHostMsgFlush(
    ppapi::host::HostMessageContext* context,
    const std::vector<ui::LatencyInfo>& latency_info) {
  // Don't allow more than one pending flush at a time.
  if (HasPendingFlush())
    return PP_ERROR_INPROGRESS;

  if (bound_instance_)
    bound_instance_->AddLatencyInfo(latency_info);

  PP_Resource old_image_data = 0;
  flush_reply_context_ = context->MakeReplyMessageContext();
  if (is_running_in_process_)
    return Flush(NULL);

  // Reuse image data when running out of process.
  int32_t result = Flush(&old_image_data);

  if (old_image_data) {
    // If the Graphics2D has an old image data it's not using any more, send
    // it back to the plugin for possible re-use. See ppb_image_data_proxy.cc
    // for a description how this process works.
    ppapi::HostResource old_image_data_host_resource;
    old_image_data_host_resource.SetHostResource(pp_instance(), old_image_data);
    host()->Send(new PpapiMsg_PPBImageData_NotifyUnusedImageData(
        ppapi::API_ID_PPB_IMAGE_DATA, old_image_data_host_resource));
  }

  return result;
}

int32_t PepperGraphics2DHost::OnHostMsgSetScale(
    ppapi::host::HostMessageContext* context,
    float scale) {
  if (scale > 0.0f) {
    scale_ = scale;
    return PP_OK;
  }
  return PP_ERROR_BADARGUMENT;
}

int32_t PepperGraphics2DHost::OnHostMsgReadImageData(
    ppapi::host::HostMessageContext* context,
    PP_Resource image,
    const PP_Point& top_left) {
  context->reply_msg = PpapiPluginMsg_Graphics2D_ReadImageDataAck();
  return ReadImageData(image, &top_left) ? PP_OK : PP_ERROR_FAILED;
}

void ReleaseCallback(scoped_ptr<cc::SharedBitmap> bitmap,
                     uint32 sync_point,
                     bool lost_resource) {}

bool PepperGraphics2DHost::PrepareTextureMailbox(
    cc::TextureMailbox* mailbox,
    scoped_ptr<cc::SingleReleaseCallback>* release_callback) {
  if (!texture_mailbox_modified_)
    return false;
  // TODO(jbauman): Send image_data_ through mailbox to avoid copy.
  gfx::Size pixel_image_size(image_data_->width(), image_data_->height());
  scoped_ptr<cc::SharedBitmap> shared_bitmap =
      RenderThreadImpl::current()
          ->shared_bitmap_manager()
          ->AllocateSharedBitmap(pixel_image_size);
  if (!shared_bitmap)
    return false;
  void* src = image_data_->Map();
  memcpy(shared_bitmap->pixels(),
         src,
         cc::SharedBitmap::CheckedSizeInBytes(pixel_image_size));
  image_data_->Unmap();

  *mailbox = cc::TextureMailbox(shared_bitmap->memory(), pixel_image_size);
  *release_callback = cc::SingleReleaseCallback::Create(
      base::Bind(&ReleaseCallback, base::Passed(&shared_bitmap)));
  texture_mailbox_modified_ = false;
  return true;
}

void PepperGraphics2DHost::AttachedToNewLayer() {
  texture_mailbox_modified_ = true;
}

int32_t PepperGraphics2DHost::Flush(PP_Resource* old_image_data) {
  bool done_replace_contents = false;
  bool no_update_visible = true;
  bool is_plugin_visible = true;
  for (size_t i = 0; i < queued_operations_.size(); i++) {
    QueuedOperation& operation = queued_operations_[i];
    gfx::Rect op_rect;
    switch (operation.type) {
      case QueuedOperation::PAINT:
        ExecutePaintImageData(operation.paint_image.get(),
                              operation.paint_x,
                              operation.paint_y,
                              operation.paint_src_rect,
                              &op_rect);
        break;
      case QueuedOperation::SCROLL:
        ExecuteScroll(operation.scroll_clip_rect,
                      operation.scroll_dx,
                      operation.scroll_dy,
                      &op_rect);
        break;
      case QueuedOperation::REPLACE:
        // Since the out parameter |old_image_data| takes ownership of the
        // reference, if there are more than one ReplaceContents calls queued
        // the first |old_image_data| will get overwritten and leaked. So we
        // only supply this for the first call.
        ExecuteReplaceContents(operation.replace_image.get(),
                               &op_rect,
                               done_replace_contents ? NULL : old_image_data);
        done_replace_contents = true;
        break;
    }

    // For correctness with accelerated compositing, we must issue an invalidate
    // on the full op_rect even if it is partially or completely off-screen.
    // However, if we issue an invalidate for a clipped-out region, WebKit will
    // do nothing and we won't get any ViewFlushedPaint calls, leaving our
    // callback stranded. So we still need to check whether the repainted area
    // is visible to determine how to deal with the callback.
    if (bound_instance_ && !op_rect.IsEmpty()) {
      gfx::Point scroll_delta(operation.scroll_dx, operation.scroll_dy);
      if (!ConvertToLogicalPixels(scale_,
                                  &op_rect,
                                  operation.type == QueuedOperation::SCROLL
                                      ? &scroll_delta
                                      : NULL)) {
        // Conversion requires falling back to InvalidateRect.
        operation.type = QueuedOperation::PAINT;
      }

      gfx::Rect clip = PP_ToGfxRect(bound_instance_->view_data().clip_rect);
      is_plugin_visible = !clip.IsEmpty();

      // Set |no_update_visible| to false if the change overlaps the visible
      // area.
      if (!gfx::IntersectRects(clip, op_rect).IsEmpty()) {
        no_update_visible = false;
      }

      // Notify the plugin of the entire change (op_rect), even if it is
      // partially or completely off-screen.
      if (operation.type == QueuedOperation::SCROLL) {
        bound_instance_->ScrollRect(
            scroll_delta.x(), scroll_delta.y(), op_rect);
      } else {
        if (!op_rect.IsEmpty())
          bound_instance_->InvalidateRect(op_rect);
      }
      texture_mailbox_modified_ = true;
    }
  }
  queued_operations_.clear();

  if (!bound_instance_) {
    // As promised in the API, we always schedule callback when unbound.
    ScheduleOffscreenFlushAck();
  } else if (no_update_visible && is_plugin_visible &&
             bound_instance_->view_data().is_page_visible) {
    // There's nothing visible to invalidate so just schedule the callback to
    // execute in the next round of the message loop.
    ScheduleOffscreenFlushAck();
  } else {
    need_flush_ack_ = true;
  }

  return PP_OK_COMPLETIONPENDING;
}

void PepperGraphics2DHost::ExecutePaintImageData(PPB_ImageData_Impl* image,
                                                 int x,
                                                 int y,
                                                 const gfx::Rect& src_rect,
                                                 gfx::Rect* invalidated_rect) {
  // Ensure the source image is mapped to read from it.
  ImageDataAutoMapper auto_mapper(image);
  if (!auto_mapper.is_valid())
    return;

  // Portion within the source image to cut out.
  SkIRect src_irect = {src_rect.x(), src_rect.y(), src_rect.right(),
                       src_rect.bottom()};

  // Location within the backing store to copy to.
  *invalidated_rect = src_rect;
  invalidated_rect->Offset(x, y);
  SkRect dest_rect = {SkIntToScalar(invalidated_rect->x()),
                      SkIntToScalar(invalidated_rect->y()),
                      SkIntToScalar(invalidated_rect->right()),
                      SkIntToScalar(invalidated_rect->bottom())};

  if (image->format() != image_data_->format()) {
    // Convert the image data if the format does not match.
    ConvertImageData(image, src_irect, image_data_.get(), dest_rect);
  } else {
    // We're guaranteed to have a mapped canvas since we mapped it in Init().
    SkCanvas* backing_canvas = image_data_->GetCanvas();

    // We want to replace the contents of the bitmap rather than blend.
    SkPaint paint;
    paint.setXfermodeMode(SkXfermode::kSrc_Mode);
    backing_canvas->drawBitmapRect(
        *image->GetMappedBitmap(), &src_irect, dest_rect, &paint);
  }
}

void PepperGraphics2DHost::ExecuteScroll(const gfx::Rect& clip,
                                         int dx,
                                         int dy,
                                         gfx::Rect* invalidated_rect) {
  gfx::ScrollCanvas(image_data_->GetCanvas(), clip, gfx::Vector2d(dx, dy));
  *invalidated_rect = clip;
}

void PepperGraphics2DHost::ExecuteReplaceContents(PPB_ImageData_Impl* image,
                                                  gfx::Rect* invalidated_rect,
                                                  PP_Resource* old_image_data) {
  if (image->format() != image_data_->format()) {
    DCHECK(image->width() == image_data_->width() &&
           image->height() == image_data_->height());
    // Convert the image data if the format does not match.
    SkIRect src_irect = {0, 0, image->width(), image->height()};
    SkRect dest_rect = {SkIntToScalar(0), SkIntToScalar(0),
                        SkIntToScalar(image_data_->width()),
                        SkIntToScalar(image_data_->height())};
    ConvertImageData(image, src_irect, image_data_.get(), dest_rect);
  } else {
    // The passed-in image may not be mapped in our process, and we need to
    // guarantee that the current backing store is always mapped.
    if (!image->Map())
      return;

    if (old_image_data)
      *old_image_data = image_data_->GetReference();
    image_data_ = image;
  }
  *invalidated_rect =
      gfx::Rect(0, 0, image_data_->width(), image_data_->height());
}

void PepperGraphics2DHost::SendFlushAck() {
  host()->SendReply(flush_reply_context_, PpapiPluginMsg_Graphics2D_FlushAck());
}

void PepperGraphics2DHost::SendOffscreenFlushAck() {
  DCHECK(offscreen_flush_pending_);

  // We must clear this flag before issuing the callback. It will be
  // common for the plugin to issue another invalidate in response to a flush
  // callback, and we don't want to think that a callback is already pending.
  offscreen_flush_pending_ = false;
  SendFlushAck();
}

void PepperGraphics2DHost::ScheduleOffscreenFlushAck() {
  offscreen_flush_pending_ = true;
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&PepperGraphics2DHost::SendOffscreenFlushAck, AsWeakPtr()),
      base::TimeDelta::FromMilliseconds(kOffscreenCallbackDelayMs));
}

bool PepperGraphics2DHost::HasPendingFlush() const {
  return need_flush_ack_ || offscreen_flush_pending_;
}

// static
bool PepperGraphics2DHost::ConvertToLogicalPixels(float scale,
                                                  gfx::Rect* op_rect,
                                                  gfx::Point* delta) {
  if (scale == 1.0f || scale <= 0.0f)
    return true;

  gfx::Rect original_rect = *op_rect;
  // Take the enclosing rectangle after scaling so a rectangle scaled down then
  // scaled back up by the inverse scale would fully contain the entire area
  // affected by the original rectangle.
  *op_rect = gfx::ToEnclosingRect(gfx::ScaleRect(*op_rect, scale));
  if (delta) {
    gfx::Point original_delta = *delta;
    float inverse_scale = 1.0f / scale;
    *delta = gfx::ToFlooredPoint(gfx::ScalePoint(*delta, scale));

    gfx::Rect inverse_scaled_rect =
        gfx::ToEnclosingRect(gfx::ScaleRect(*op_rect, inverse_scale));
    if (original_rect != inverse_scaled_rect)
      return false;
    gfx::Point inverse_scaled_point =
        gfx::ToFlooredPoint(gfx::ScalePoint(*delta, inverse_scale));
    if (original_delta != inverse_scaled_point)
      return false;
  }

  return true;
}

}  // namespace content
