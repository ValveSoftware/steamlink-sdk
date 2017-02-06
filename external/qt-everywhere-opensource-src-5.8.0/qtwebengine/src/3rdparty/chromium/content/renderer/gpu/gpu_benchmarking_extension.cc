// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/gpu/gpu_benchmarking_extension.h"

#include <stddef.h>

#include <string>
#include <utility>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "cc/layers/layer.h"
#include "cc/trees/layer_tree_host.h"
#include "content/common/child_process_messages.h"
#include "content/common/input/synthetic_gesture_params.h"
#include "content/common/input/synthetic_pinch_gesture_params.h"
#include "content/common/input/synthetic_smooth_drag_gesture_params.h"
#include "content/common/input/synthetic_smooth_scroll_gesture_params.h"
#include "content/common/input/synthetic_tap_gesture_params.h"
#include "content/public/child/v8_value_converter.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/chrome_object_extensions_utils.h"
#include "content/public/renderer/render_thread.h"
#include "content/renderer/gpu/render_widget_compositor.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/render_view_impl.h"
#include "content/renderer/skia_benchmarking_extension.h"
#include "gin/arguments.h"
#include "gin/handle.h"
#include "gin/object_template_builder.h"
#include "gpu/ipc/common/gpu_messages.h"
#include "third_party/WebKit/public/web/WebImageCache.h"
#include "third_party/WebKit/public/web/WebKit.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkGraphics.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/core/SkPixelRef.h"
#include "third_party/skia/include/core/SkPixelSerializer.h"
#include "third_party/skia/include/core/SkStream.h"
#include "ui/gfx/codec/png_codec.h"
#include "v8/include/v8.h"

using blink::WebCanvas;
using blink::WebLocalFrame;
using blink::WebImageCache;
using blink::WebPrivatePtr;
using blink::WebSize;
using blink::WebView;

namespace content {

namespace {

class EncodingSerializer : public SkPixelSerializer {
 protected:
  bool onUseEncodedData(const void* data, size_t len) override { return true; }

  SkData* onEncode(const SkPixmap& pixmap) override {
    std::vector<uint8_t> vector;

    const base::CommandLine& commandLine =
        *base::CommandLine::ForCurrentProcess();
    if (commandLine.HasSwitch(switches::kSkipReencodingOnSKPCapture)) {
        // In this case, we just want to store some useful information
        // about the image to replace the missing encoded data.

        // First make sure that the data does not accidentally match any
        // image signatures.
        vector.push_back(0xFF);
        vector.push_back(0xFF);
        vector.push_back(0xFF);
        vector.push_back(0xFF);

        // Save the width and height.
        uint32_t width = pixmap.width();
        uint32_t height = pixmap.height();
        vector.push_back(width & 0xFF);
        vector.push_back((width >> 8) & 0xFF);
        vector.push_back((width >> 16) & 0xFF);
        vector.push_back((width >> 24) & 0xFF);
        vector.push_back(height & 0xFF);
        vector.push_back((height >> 8) & 0xFF);
        vector.push_back((height >> 16) & 0xFF);
        vector.push_back((height >> 24) & 0xFF);

        // Save any additional information about the bitmap that may be
        // interesting.
        vector.push_back(pixmap.colorType());
        vector.push_back(pixmap.alphaType());
        return SkData::NewWithCopy(&vector.front(), vector.size());
    } else {
      SkBitmap bm;
      // The const_cast is fine, since we only read from the bitmap.
      if (bm.installPixels(pixmap.info(),
                           const_cast<void*>(pixmap.addr()),
                           pixmap.rowBytes())) {
        if (gfx::PNGCodec::EncodeBGRASkBitmap(bm, false, &vector)) {
          return SkData::NewWithCopy(&vector.front(), vector.size());
        }
      }
    }
    return nullptr;
  }
};

class SkPictureSerializer {
 public:
  explicit SkPictureSerializer(const base::FilePath& dirpath)
      : dirpath_(dirpath),
        layer_id_(0) {
    // Let skia register known effect subclasses. This basically enables
    // reflection on those subclasses required for picture serialization.
    SkiaBenchmarking::Initialize();
  }

  // Recursively serializes the layer tree.
  // Each layer in the tree is serialized into a separate skp file
  // in the given directory.
  void Serialize(const cc::Layer* root_layer) {
    for (auto* layer : *root_layer->layer_tree_host()) {
      sk_sp<SkPicture> picture = layer->GetPicture();
      if (!picture)
        continue;

      // Serialize picture to file.
      // TODO(alokp): Note that for this to work Chrome needs to be launched
      // with
      // --no-sandbox command-line flag. Get rid of this limitation.
      // CRBUG: 139640.
      std::string filename = "layer_" + base::IntToString(layer_id_++) + ".skp";
      std::string filepath = dirpath_.AppendASCII(filename).MaybeAsASCII();
      DCHECK(!filepath.empty());
      SkFILEWStream file(filepath.c_str());
      DCHECK(file.isValid());

      EncodingSerializer serializer;
      picture->serialize(&file, &serializer);
      file.fsync();
    }
  }

 private:
  base::FilePath dirpath_;
  int layer_id_;
};

template <typename T>
bool GetArg(gin::Arguments* args, T* value) {
  if (!args->GetNext(value)) {
    args->ThrowError();
    return false;
  }
  return true;
}

template <>
bool GetArg(gin::Arguments* args, int* value) {
  float number;
  bool ret = GetArg(args, &number);
  *value = number;
  return ret;
}

template <typename T>
bool GetOptionalArg(gin::Arguments* args, T* value) {
  if (args->PeekNext().IsEmpty())
    return true;
  if (args->PeekNext()->IsUndefined()) {
    args->Skip();
    return true;
  }
  return GetArg(args, value);
}

class CallbackAndContext : public base::RefCounted<CallbackAndContext> {
 public:
  CallbackAndContext(v8::Isolate* isolate,
                     v8::Local<v8::Function> callback,
                     v8::Local<v8::Context> context)
      : isolate_(isolate) {
    callback_.Reset(isolate_, callback);
    context_.Reset(isolate_, context);
  }

  v8::Isolate* isolate() {
    return isolate_;
  }

  v8::Local<v8::Function> GetCallback() {
    return v8::Local<v8::Function>::New(isolate_, callback_);
  }

  v8::Local<v8::Context> GetContext() {
    return v8::Local<v8::Context>::New(isolate_, context_);
  }

 private:
  friend class base::RefCounted<CallbackAndContext>;

  virtual ~CallbackAndContext() {
    callback_.Reset();
    context_.Reset();
  }

  v8::Isolate* isolate_;
  v8::Persistent<v8::Function> callback_;
  v8::Persistent<v8::Context> context_;
  DISALLOW_COPY_AND_ASSIGN(CallbackAndContext);
};

class GpuBenchmarkingContext {
 public:
  GpuBenchmarkingContext()
      : web_frame_(NULL),
        web_view_(NULL),
        render_view_impl_(NULL),
        compositor_(NULL) {}

  bool Init(bool init_compositor) {
    web_frame_ = WebLocalFrame::frameForCurrentContext();
    if (!web_frame_)
      return false;

    web_view_ = web_frame_->view();
    if (!web_view_) {
      web_frame_ = NULL;
      return false;
    }

    render_view_impl_ = RenderViewImpl::FromWebView(web_view_);
    if (!render_view_impl_) {
      web_frame_ = NULL;
      web_view_ = NULL;
      return false;
    }

    if (!init_compositor)
      return true;

    compositor_ = render_view_impl_->GetWidget()->compositor();
    if (!compositor_) {
      web_frame_ = NULL;
      web_view_ = NULL;
      render_view_impl_ = NULL;
      return false;
    }

    return true;
  }

  WebLocalFrame* web_frame() const {
    DCHECK(web_frame_ != NULL);
    return web_frame_;
  }
  WebView* web_view() const {
    DCHECK(web_view_ != NULL);
    return web_view_;
  }
  RenderViewImpl* render_view_impl() const {
    DCHECK(render_view_impl_ != NULL);
    return render_view_impl_;
  }
  RenderWidgetCompositor* compositor() const {
    DCHECK(compositor_ != NULL);
    return compositor_;
  }

 private:
  WebLocalFrame* web_frame_;
  WebView* web_view_;
  RenderViewImpl* render_view_impl_;
  RenderWidgetCompositor* compositor_;

  DISALLOW_COPY_AND_ASSIGN(GpuBenchmarkingContext);
};

void OnMicroBenchmarkCompleted(CallbackAndContext* callback_and_context,
                               std::unique_ptr<base::Value> result) {
  v8::Isolate* isolate = callback_and_context->isolate();
  v8::HandleScope scope(isolate);
  v8::Local<v8::Context> context = callback_and_context->GetContext();
  v8::Context::Scope context_scope(context);
  WebLocalFrame* frame = WebLocalFrame::frameForContext(context);
  if (frame) {
    std::unique_ptr<V8ValueConverter> converter =
        base::WrapUnique(V8ValueConverter::create());
    v8::Local<v8::Value> value = converter->ToV8Value(result.get(), context);
    v8::Local<v8::Value> argv[] = { value };

    frame->callFunctionEvenIfScriptDisabled(
        callback_and_context->GetCallback(),
        v8::Object::New(isolate),
        1,
        argv);
  }
}

void OnSyntheticGestureCompleted(CallbackAndContext* callback_and_context) {
  v8::Isolate* isolate = callback_and_context->isolate();
  v8::HandleScope scope(isolate);
  v8::Local<v8::Context> context = callback_and_context->GetContext();
  v8::Context::Scope context_scope(context);
  WebLocalFrame* frame = WebLocalFrame::frameForContext(context);
  if (frame) {
    frame->callFunctionEvenIfScriptDisabled(
        callback_and_context->GetCallback(), v8::Object::New(isolate), 0, NULL);
  }
}

bool BeginSmoothScroll(v8::Isolate* isolate,
                       float pixels_to_scroll,
                       v8::Local<v8::Function> callback,
                       int gesture_source_type,
                       const std::string& direction,
                       float speed_in_pixels_s,
                       bool prevent_fling,
                       float start_x,
                       float start_y) {
  GpuBenchmarkingContext context;
  if (!context.Init(false))
    return false;

  // Convert coordinates from CSS pixels to density independent pixels (DIPs).
  float page_scale_factor = context.web_view()->pageScaleFactor();

  if (gesture_source_type == SyntheticGestureParams::MOUSE_INPUT) {
    // Ensure the mouse is centered and visible, in case it will
    // trigger any hover or mousemove effects.
    context.web_view()->setIsActive(true);
    blink::WebRect contentRect =
        context.web_view()->mainFrame()->visibleContentRect();
    blink::WebMouseEvent mouseMove;
    mouseMove.type = blink::WebInputEvent::MouseMove;
    mouseMove.x = (contentRect.x + contentRect.width / 2) * page_scale_factor;
    mouseMove.y = (contentRect.y + contentRect.height / 2) * page_scale_factor;
    context.web_view()->handleInputEvent(mouseMove);
    context.web_view()->setCursorVisibilityState(true);
  }

  scoped_refptr<CallbackAndContext> callback_and_context =
      new CallbackAndContext(
          isolate, callback, context.web_frame()->mainWorldScriptContext());

  std::unique_ptr<SyntheticSmoothScrollGestureParams> gesture_params(
      new SyntheticSmoothScrollGestureParams);

  if (gesture_source_type < 0 ||
      gesture_source_type > SyntheticGestureParams::GESTURE_SOURCE_TYPE_MAX) {
    return false;
  }
  gesture_params->gesture_source_type =
      static_cast<SyntheticGestureParams::GestureSourceType>(
          gesture_source_type);

  gesture_params->speed_in_pixels_s = speed_in_pixels_s;
  gesture_params->prevent_fling = prevent_fling;

  gesture_params->anchor.SetPoint(start_x * page_scale_factor,
                                  start_y * page_scale_factor);

  float distance_length = pixels_to_scroll * page_scale_factor;
  gfx::Vector2dF distance;
  if (direction == "down")
    distance.set_y(-distance_length);
  else if (direction == "up")
    distance.set_y(distance_length);
  else if (direction == "right")
    distance.set_x(-distance_length);
  else if (direction == "left")
    distance.set_x(distance_length);
  else if (direction == "upleft") {
    distance.set_y(distance_length);
    distance.set_x(distance_length);
  } else if (direction == "upright") {
    distance.set_y(distance_length);
    distance.set_x(-distance_length);
  } else if (direction == "downleft") {
    distance.set_y(-distance_length);
    distance.set_x(distance_length);
  } else if (direction == "downright") {
    distance.set_y(-distance_length);
    distance.set_x(-distance_length);
  } else {
    return false;
  }
  gesture_params->distances.push_back(distance);

  // TODO(nduca): If the render_view_impl is destroyed while the gesture is in
  // progress, we will leak the callback and context. This needs to be fixed,
  // somehow.
  context.render_view_impl()->GetWidget()->QueueSyntheticGesture(
      std::move(gesture_params),
      base::Bind(&OnSyntheticGestureCompleted,
                 base::RetainedRef(callback_and_context)));

  return true;
}

bool BeginSmoothDrag(v8::Isolate* isolate,
                     float start_x,
                     float start_y,
                     float end_x,
                     float end_y,
                     v8::Local<v8::Function> callback,
                     int gesture_source_type,
                     float speed_in_pixels_s) {
  GpuBenchmarkingContext context;
  if (!context.Init(false))
    return false;
  scoped_refptr<CallbackAndContext> callback_and_context =
      new CallbackAndContext(isolate, callback,
                             context.web_frame()->mainWorldScriptContext());

  std::unique_ptr<SyntheticSmoothDragGestureParams> gesture_params(
      new SyntheticSmoothDragGestureParams);

  // Convert coordinates from CSS pixels to density independent pixels (DIPs).
  float page_scale_factor = context.web_view()->pageScaleFactor();

  gesture_params->start_point.SetPoint(start_x * page_scale_factor,
                                       start_y * page_scale_factor);
  gfx::PointF end_point(end_x * page_scale_factor,
                        end_y * page_scale_factor);
  gfx::Vector2dF distance = end_point - gesture_params->start_point;
  gesture_params->distances.push_back(distance);
  gesture_params->speed_in_pixels_s = speed_in_pixels_s * page_scale_factor;
  gesture_params->gesture_source_type =
      static_cast<SyntheticGestureParams::GestureSourceType>(
          gesture_source_type);

  // TODO(nduca): If the render_view_impl is destroyed while the gesture is in
  // progress, we will leak the callback and context. This needs to be fixed,
  // somehow.
  context.render_view_impl()->GetWidget()->QueueSyntheticGesture(
      std::move(gesture_params),
      base::Bind(&OnSyntheticGestureCompleted,
                 base::RetainedRef(callback_and_context)));

  return true;
}

}  // namespace

gin::WrapperInfo GpuBenchmarking::kWrapperInfo = {gin::kEmbedderNativeGin};

// static
void GpuBenchmarking::Install(blink::WebFrame* frame) {
  v8::Isolate* isolate = blink::mainThreadIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = frame->mainWorldScriptContext();
  if (context.IsEmpty())
    return;

  v8::Context::Scope context_scope(context);

  gin::Handle<GpuBenchmarking> controller =
      gin::CreateHandle(isolate, new GpuBenchmarking());
  if (controller.IsEmpty())
    return;

  v8::Local<v8::Object> chrome = GetOrCreateChromeObject(isolate,
                                                          context->Global());
  chrome->Set(gin::StringToV8(isolate, "gpuBenchmarking"), controller.ToV8());
}

GpuBenchmarking::GpuBenchmarking() {
}

GpuBenchmarking::~GpuBenchmarking() {
}

gin::ObjectTemplateBuilder GpuBenchmarking::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return gin::Wrappable<GpuBenchmarking>::GetObjectTemplateBuilder(isolate)
      .SetMethod("setNeedsDisplayOnAllLayers",
                 &GpuBenchmarking::SetNeedsDisplayOnAllLayers)
      .SetMethod("setRasterizeOnlyVisibleContent",
                 &GpuBenchmarking::SetRasterizeOnlyVisibleContent)
      .SetMethod("printToSkPicture", &GpuBenchmarking::PrintToSkPicture)
      .SetValue("DEFAULT_INPUT", 0)
      .SetValue("TOUCH_INPUT", 1)
      .SetValue("MOUSE_INPUT", 2)
      .SetMethod("gestureSourceTypeSupported",
                 &GpuBenchmarking::GestureSourceTypeSupported)
      .SetMethod("smoothScrollBy", &GpuBenchmarking::SmoothScrollBy)
      .SetMethod("smoothDrag", &GpuBenchmarking::SmoothDrag)
      .SetMethod("swipe", &GpuBenchmarking::Swipe)
      .SetMethod("scrollBounce", &GpuBenchmarking::ScrollBounce)
      .SetMethod("pinchBy", &GpuBenchmarking::PinchBy)
      .SetMethod("pageScaleFactor", &GpuBenchmarking::PageScaleFactor)
      .SetMethod("visualViewportX", &GpuBenchmarking::VisualViewportX)
      .SetMethod("visualViewportY", &GpuBenchmarking::VisualViewportY)
      .SetMethod("visualViewportHeight", &GpuBenchmarking::VisualViewportHeight)
      .SetMethod("visualViewportWidth", &GpuBenchmarking::VisualViewportWidth)
      .SetMethod("tap", &GpuBenchmarking::Tap)
      .SetMethod("clearImageCache", &GpuBenchmarking::ClearImageCache)
      .SetMethod("runMicroBenchmark", &GpuBenchmarking::RunMicroBenchmark)
      .SetMethod("sendMessageToMicroBenchmark",
                 &GpuBenchmarking::SendMessageToMicroBenchmark)
      .SetMethod("hasGpuChannel", &GpuBenchmarking::HasGpuChannel)
      .SetMethod("hasGpuProcess", &GpuBenchmarking::HasGpuProcess)
      .SetMethod("getGpuDriverBugWorkarounds",
                 &GpuBenchmarking::GetGpuDriverBugWorkarounds);
}

void GpuBenchmarking::SetNeedsDisplayOnAllLayers() {
  GpuBenchmarkingContext context;
  if (!context.Init(true))
    return;

  context.compositor()->SetNeedsDisplayOnAllLayers();
}

void GpuBenchmarking::SetRasterizeOnlyVisibleContent() {
  GpuBenchmarkingContext context;
  if (!context.Init(true))
    return;

  context.compositor()->SetRasterizeOnlyVisibleContent();
}

void GpuBenchmarking::PrintToSkPicture(v8::Isolate* isolate,
                                       const std::string& dirname) {
  GpuBenchmarkingContext context;
  if (!context.Init(true))
    return;

  const cc::Layer* root_layer = context.compositor()->GetRootLayer();
  if (!root_layer)
    return;

  base::FilePath dirpath = base::FilePath::FromUTF8Unsafe(dirname);
  if (!base::CreateDirectory(dirpath) ||
      !base::PathIsWritable(dirpath)) {
    std::string msg("Path is not writable: ");
    msg.append(dirpath.MaybeAsASCII());
    isolate->ThrowException(v8::Exception::Error(v8::String::NewFromUtf8(
        isolate, msg.c_str(), v8::String::kNormalString, msg.length())));
    return;
  }

  SkPictureSerializer serializer(dirpath);
  serializer.Serialize(root_layer);
}

bool GpuBenchmarking::GestureSourceTypeSupported(int gesture_source_type) {
  if (gesture_source_type < 0 ||
      gesture_source_type > SyntheticGestureParams::GESTURE_SOURCE_TYPE_MAX) {
    return false;
  }

  return SyntheticGestureParams::IsGestureSourceTypeSupported(
      static_cast<SyntheticGestureParams::GestureSourceType>(
          gesture_source_type));
}

bool GpuBenchmarking::SmoothScrollBy(gin::Arguments* args) {
  GpuBenchmarkingContext context;
  if (!context.Init(true))
    return false;

  float page_scale_factor = context.web_view()->pageScaleFactor();
  blink::WebRect rect = context.render_view_impl()->GetWidget()->windowRect();

  float pixels_to_scroll = 0;
  v8::Local<v8::Function> callback;
  float start_x = rect.width / (page_scale_factor * 2);
  float start_y = rect.height / (page_scale_factor * 2);
  int gesture_source_type = SyntheticGestureParams::DEFAULT_INPUT;
  std::string direction = "down";
  float speed_in_pixels_s = 800;

  if (!GetOptionalArg(args, &pixels_to_scroll) ||
      !GetOptionalArg(args, &callback) ||
      !GetOptionalArg(args, &start_x) ||
      !GetOptionalArg(args, &start_y) ||
      !GetOptionalArg(args, &gesture_source_type) ||
      !GetOptionalArg(args, &direction) ||
      !GetOptionalArg(args, &speed_in_pixels_s)) {
    return false;
  }

  return BeginSmoothScroll(args->isolate(),
                           pixels_to_scroll,
                           callback,
                           gesture_source_type,
                           direction,
                           speed_in_pixels_s,
                           true,
                           start_x,
                           start_y);
}

bool GpuBenchmarking::SmoothDrag(gin::Arguments* args) {
  GpuBenchmarkingContext context;
  if (!context.Init(true))
    return false;

  float start_x;
  float start_y;
  float end_x;
  float end_y;
  v8::Local<v8::Function> callback;
  int gesture_source_type = SyntheticGestureParams::DEFAULT_INPUT;
  float speed_in_pixels_s = 800;

  if (!GetArg(args, &start_x) ||
      !GetArg(args, &start_y) ||
      !GetArg(args, &end_x) ||
      !GetArg(args, &end_y) ||
      !GetOptionalArg(args, &callback) ||
      !GetOptionalArg(args, &gesture_source_type) ||
      !GetOptionalArg(args, &speed_in_pixels_s)) {
    return false;
  }

  return BeginSmoothDrag(args->isolate(),
                         start_x,
                         start_y,
                         end_x,
                         end_y,
                         callback,
                         gesture_source_type,
                         speed_in_pixels_s);
}

bool GpuBenchmarking::Swipe(gin::Arguments* args) {
  GpuBenchmarkingContext context;
  if (!context.Init(true))
    return false;

  float page_scale_factor = context.web_view()->pageScaleFactor();
  blink::WebRect rect = context.render_view_impl()->GetWidget()->windowRect();

  std::string direction = "up";
  float pixels_to_scroll = 0;
  v8::Local<v8::Function> callback;
  float start_x = rect.width / (page_scale_factor * 2);
  float start_y = rect.height / (page_scale_factor * 2);
  float speed_in_pixels_s = 800;

  if (!GetOptionalArg(args, &direction) ||
      !GetOptionalArg(args, &pixels_to_scroll) ||
      !GetOptionalArg(args, &callback) ||
      !GetOptionalArg(args, &start_x) ||
      !GetOptionalArg(args, &start_y) ||
      !GetOptionalArg(args, &speed_in_pixels_s)) {
    return false;
  }

  return BeginSmoothScroll(args->isolate(),
                           -pixels_to_scroll,
                           callback,
                           1,  // TOUCH_INPUT
                           direction,
                           speed_in_pixels_s,
                           false,
                           start_x,
                           start_y);
}

bool GpuBenchmarking::ScrollBounce(gin::Arguments* args) {
  GpuBenchmarkingContext context;
  if (!context.Init(false))
    return false;

  float page_scale_factor = context.web_view()->pageScaleFactor();
  blink::WebRect rect = context.render_view_impl()->GetWidget()->windowRect();

  std::string direction = "down";
  float distance_length = 0;
  float overscroll_length = 0;
  int repeat_count = 1;
  v8::Local<v8::Function> callback;
  float start_x = rect.width / (page_scale_factor * 2);
  float start_y = rect.height / (page_scale_factor * 2);
  float speed_in_pixels_s = 800;

  if (!GetOptionalArg(args, &direction) ||
      !GetOptionalArg(args, &distance_length) ||
      !GetOptionalArg(args, &overscroll_length) ||
      !GetOptionalArg(args, &repeat_count) ||
      !GetOptionalArg(args, &callback) ||
      !GetOptionalArg(args, &start_x) ||
      !GetOptionalArg(args, &start_y) ||
      !GetOptionalArg(args, &speed_in_pixels_s)) {
    return false;
  }

  scoped_refptr<CallbackAndContext> callback_and_context =
      new CallbackAndContext(args->isolate(),
                             callback,
                             context.web_frame()->mainWorldScriptContext());

  std::unique_ptr<SyntheticSmoothScrollGestureParams> gesture_params(
      new SyntheticSmoothScrollGestureParams);

  gesture_params->speed_in_pixels_s = speed_in_pixels_s;

  gesture_params->anchor.SetPoint(start_x * page_scale_factor,
                                  start_y * page_scale_factor);

  distance_length *= page_scale_factor;
  overscroll_length *= page_scale_factor;
  gfx::Vector2dF distance;
  gfx::Vector2dF overscroll;
  if (direction == "down") {
    distance.set_y(-distance_length);
    overscroll.set_y(overscroll_length);
  } else if (direction == "up") {
    distance.set_y(distance_length);
    overscroll.set_y(-overscroll_length);
  } else if (direction == "right") {
    distance.set_x(-distance_length);
    overscroll.set_x(overscroll_length);
  } else if (direction == "left") {
    distance.set_x(distance_length);
    overscroll.set_x(-overscroll_length);
  } else {
    return false;
  }

  for (int i = 0; i < repeat_count; i++) {
    gesture_params->distances.push_back(distance);
    gesture_params->distances.push_back(-distance + overscroll);
  }

  // TODO(nduca): If the render_view_impl is destroyed while the gesture is in
  // progress, we will leak the callback and context. This needs to be fixed,
  // somehow.
  context.render_view_impl()->GetWidget()->QueueSyntheticGesture(
      std::move(gesture_params),
      base::Bind(&OnSyntheticGestureCompleted,
                 base::RetainedRef(callback_and_context)));

  return true;
}

bool GpuBenchmarking::PinchBy(gin::Arguments* args) {
  GpuBenchmarkingContext context;
  if (!context.Init(false))
    return false;

  float scale_factor;
  float anchor_x;
  float anchor_y;
  v8::Local<v8::Function> callback;
  float relative_pointer_speed_in_pixels_s = 800;


  if (!GetArg(args, &scale_factor) ||
      !GetArg(args, &anchor_x) ||
      !GetArg(args, &anchor_y) ||
      !GetOptionalArg(args, &callback) ||
      !GetOptionalArg(args, &relative_pointer_speed_in_pixels_s)) {
    return false;
  }

  std::unique_ptr<SyntheticPinchGestureParams> gesture_params(
      new SyntheticPinchGestureParams);

  // TODO(bokan): Remove page scale here when change land in Catapult.
  // Convert coordinates from CSS pixels to density independent pixels (DIPs).
  float page_scale_factor = context.web_view()->pageScaleFactor();

  gesture_params->scale_factor = scale_factor;
  gesture_params->anchor.SetPoint(anchor_x * page_scale_factor,
                                  anchor_y * page_scale_factor);
  gesture_params->relative_pointer_speed_in_pixels_s =
      relative_pointer_speed_in_pixels_s;

  scoped_refptr<CallbackAndContext> callback_and_context =
      new CallbackAndContext(args->isolate(),
                             callback,
                             context.web_frame()->mainWorldScriptContext());


  // TODO(nduca): If the render_view_impl is destroyed while the gesture is in
  // progress, we will leak the callback and context. This needs to be fixed,
  // somehow.
  context.render_view_impl()->GetWidget()->QueueSyntheticGesture(
      std::move(gesture_params),
      base::Bind(&OnSyntheticGestureCompleted,
                 base::RetainedRef(callback_and_context)));

  return true;
}

float GpuBenchmarking::PageScaleFactor() {
  GpuBenchmarkingContext context;
  if (!context.Init(false))
    return 0.0;
  return context.web_view()->pageScaleFactor();
}

float GpuBenchmarking::VisualViewportY() {
  GpuBenchmarkingContext context;
  if (!context.Init(false))
    return 0.0;
  return context.web_view()->visualViewportOffset().y;
}

float GpuBenchmarking::VisualViewportX() {
  GpuBenchmarkingContext context;
  if (!context.Init(false))
    return 0.0;
  return context.web_view()->visualViewportOffset().x;
}

float GpuBenchmarking::VisualViewportHeight() {
  GpuBenchmarkingContext context;
  if (!context.Init(false))
    return 0.0;
  return context.web_view()->visualViewportSize().height;
}

float GpuBenchmarking::VisualViewportWidth() {
  GpuBenchmarkingContext context;
  if (!context.Init(false))
    return 0.0;
  return context.web_view()->visualViewportSize().width;
}

bool GpuBenchmarking::Tap(gin::Arguments* args) {
  GpuBenchmarkingContext context;
  if (!context.Init(false))
    return false;

  float position_x;
  float position_y;
  v8::Local<v8::Function> callback;
  int duration_ms = 50;
  int gesture_source_type = SyntheticGestureParams::DEFAULT_INPUT;

  if (!GetArg(args, &position_x) ||
      !GetArg(args, &position_y) ||
      !GetOptionalArg(args, &callback) ||
      !GetOptionalArg(args, &duration_ms) ||
      !GetOptionalArg(args, &gesture_source_type)) {
    return false;
  }

  std::unique_ptr<SyntheticTapGestureParams> gesture_params(
      new SyntheticTapGestureParams);

  // Convert coordinates from CSS pixels to density independent pixels (DIPs).
  float page_scale_factor = context.web_view()->pageScaleFactor();

  gesture_params->position.SetPoint(position_x * page_scale_factor,
                                    position_y * page_scale_factor);
  gesture_params->duration_ms = duration_ms;

  if (gesture_source_type < 0 ||
      gesture_source_type > SyntheticGestureParams::GESTURE_SOURCE_TYPE_MAX) {
    return false;
  }
  gesture_params->gesture_source_type =
      static_cast<SyntheticGestureParams::GestureSourceType>(
          gesture_source_type);

  scoped_refptr<CallbackAndContext> callback_and_context =
      new CallbackAndContext(args->isolate(),
                             callback,
                             context.web_frame()->mainWorldScriptContext());

  // TODO(nduca): If the render_view_impl is destroyed while the gesture is in
  // progress, we will leak the callback and context. This needs to be fixed,
  // somehow.
  context.render_view_impl()->GetWidget()->QueueSyntheticGesture(
      std::move(gesture_params),
      base::Bind(&OnSyntheticGestureCompleted,
                 base::RetainedRef(callback_and_context)));

  return true;
}

void GpuBenchmarking::ClearImageCache() {
  WebImageCache::clear();
}

int GpuBenchmarking::RunMicroBenchmark(gin::Arguments* args) {
  GpuBenchmarkingContext context;
  if (!context.Init(true))
    return 0;

  std::string name;
  v8::Local<v8::Function> callback;
  v8::Local<v8::Object> arguments;

  if (!GetArg(args, &name) || !GetArg(args, &callback) ||
      !GetOptionalArg(args, &arguments)) {
    return 0;
  }

  scoped_refptr<CallbackAndContext> callback_and_context =
      new CallbackAndContext(args->isolate(),
                             callback,
                             context.web_frame()->mainWorldScriptContext());

  std::unique_ptr<V8ValueConverter> converter =
      base::WrapUnique(V8ValueConverter::create());
  v8::Local<v8::Context> v8_context = callback_and_context->GetContext();
  std::unique_ptr<base::Value> value =
      converter->FromV8Value(arguments, v8_context);

  return context.compositor()->ScheduleMicroBenchmark(
      name, std::move(value),
      base::Bind(&OnMicroBenchmarkCompleted,
                 base::RetainedRef(callback_and_context)));
}

bool GpuBenchmarking::SendMessageToMicroBenchmark(
    int id,
    v8::Local<v8::Object> message) {
  GpuBenchmarkingContext context;
  if (!context.Init(true))
    return false;

  std::unique_ptr<V8ValueConverter> converter =
      base::WrapUnique(V8ValueConverter::create());
  v8::Local<v8::Context> v8_context =
      context.web_frame()->mainWorldScriptContext();
  std::unique_ptr<base::Value> value =
      converter->FromV8Value(message, v8_context);

  return context.compositor()->SendMessageToMicroBenchmark(id,
                                                           std::move(value));
}

bool GpuBenchmarking::HasGpuChannel() {
  gpu::GpuChannelHost* gpu_channel =
      RenderThreadImpl::current()->GetGpuChannel();
  return !!gpu_channel;
}

bool GpuBenchmarking::HasGpuProcess() {
  bool has_gpu_process = false;
  if (!RenderThreadImpl::current()->Send(
          new ChildProcessHostMsg_HasGpuProcess(&has_gpu_process)))
    return false;

  return has_gpu_process;
}

void GpuBenchmarking::GetGpuDriverBugWorkarounds(gin::Arguments* args) {
  std::vector<std::string> gpu_driver_bug_workarounds;
  gpu::GpuChannelHost* gpu_channel =
      RenderThreadImpl::current()->GetGpuChannel();
  if (!gpu_channel->Send(new GpuChannelMsg_GetDriverBugWorkArounds(
          &gpu_driver_bug_workarounds))) {
    return;
  }

  v8::Local<v8::Value> result;
  if (gin::TryConvertToV8(args->isolate(), gpu_driver_bug_workarounds, &result))
    args->Return(result);
}

}  // namespace content
