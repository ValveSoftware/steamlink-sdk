// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/skia_benchmarking_extension.h"

#include "base/base64.h"
#include "base/time/time.h"
#include "base/values.h"
#include "cc/base/math_util.h"
#include "cc/resources/picture.h"
#include "content/public/renderer/v8_value_converter.h"
#include "content/renderer/render_thread_impl.h"
#include "gin/arguments.h"
#include "gin/handle.h"
#include "gin/object_template_builder.h"
#include "skia/ext/benchmarking_canvas.h"
#include "third_party/WebKit/public/platform/WebArrayBuffer.h"
#include "third_party/WebKit/public/web/WebArrayBufferConverter.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebKit.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColorPriv.h"
#include "third_party/skia/include/core/SkGraphics.h"
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/skia/src/utils/debugger/SkDebugCanvas.h"
#include "third_party/skia/src/utils/debugger/SkDrawCommand.h"
#include "ui/gfx/rect_conversions.h"
#include "ui/gfx/skia_util.h"
#include "v8/include/v8.h"


namespace content {

namespace {

scoped_ptr<base::Value> ParsePictureArg(v8::Isolate* isolate,
                                        v8::Handle<v8::Value> arg) {
  scoped_ptr<content::V8ValueConverter> converter(
      content::V8ValueConverter::create());
  return scoped_ptr<base::Value>(
      converter->FromV8Value(arg, isolate->GetCurrentContext()));
}

scoped_refptr<cc::Picture> ParsePictureStr(v8::Isolate* isolate,
                                           v8::Handle<v8::Value> arg) {
  scoped_ptr<base::Value> picture_value = ParsePictureArg(isolate, arg);
  if (!picture_value)
    return NULL;
  return cc::Picture::CreateFromSkpValue(picture_value.get());
}

scoped_refptr<cc::Picture> ParsePictureHash(v8::Isolate* isolate,
                                            v8::Handle<v8::Value> arg) {
  scoped_ptr<base::Value> picture_value = ParsePictureArg(isolate, arg);
  if (!picture_value)
    return NULL;
  return cc::Picture::CreateFromValue(picture_value.get());
}

}  // namespace

gin::WrapperInfo SkiaBenchmarking::kWrapperInfo = {gin::kEmbedderNativeGin};

// static
void SkiaBenchmarking::Install(blink::WebFrame* frame) {
  v8::Isolate* isolate = blink::mainThreadIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Handle<v8::Context> context = frame->mainWorldScriptContext();
  if (context.IsEmpty())
    return;

  v8::Context::Scope context_scope(context);

  gin::Handle<SkiaBenchmarking> controller =
      gin::CreateHandle(isolate, new SkiaBenchmarking());
  if (controller.IsEmpty())
    return;

  v8::Handle<v8::Object> global = context->Global();
  v8::Handle<v8::Object> chrome =
      global->Get(gin::StringToV8(isolate, "chrome"))->ToObject();
  if (chrome.IsEmpty()) {
    chrome = v8::Object::New(isolate);
    global->Set(gin::StringToV8(isolate, "chrome"), chrome);
  }
  chrome->Set(gin::StringToV8(isolate, "skiaBenchmarking"), controller.ToV8());
}

// static
void SkiaBenchmarking::Initialize() {
  DCHECK(RenderThreadImpl::current());
  // FIXME: remove this after Skia updates SkGraphics::Init() to be
  //        thread-safe and idempotent.
  static bool skia_initialized = false;
  if (!skia_initialized) {
    LOG(WARNING) << "Enabling unsafe Skia benchmarking extension.";
    SkGraphics::Init();
    skia_initialized = true;
  }
}

SkiaBenchmarking::SkiaBenchmarking() {
  Initialize();
}

SkiaBenchmarking::~SkiaBenchmarking() {}

gin::ObjectTemplateBuilder SkiaBenchmarking::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return gin::Wrappable<SkiaBenchmarking>::GetObjectTemplateBuilder(isolate)
      .SetMethod("rasterize", &SkiaBenchmarking::Rasterize)
      .SetMethod("getOps", &SkiaBenchmarking::GetOps)
      .SetMethod("getOpTimings", &SkiaBenchmarking::GetOpTimings)
      .SetMethod("getInfo", &SkiaBenchmarking::GetInfo);
}

void SkiaBenchmarking::Rasterize(gin::Arguments* args) {
  v8::Isolate* isolate = args->isolate();
  if (args->PeekNext().IsEmpty())
    return;
  v8::Handle<v8::Value> picture_handle;
  args->GetNext(&picture_handle);
  scoped_refptr<cc::Picture> picture =
      ParsePictureHash(isolate, picture_handle);
  if (!picture.get())
    return;

  double scale = 1.0;
  gfx::Rect clip_rect(picture->LayerRect());
  int stop_index = -1;
  bool overdraw = false;

  v8::Handle<v8::Context> context = isolate->GetCurrentContext();
  if (!args->PeekNext().IsEmpty()) {
    v8::Handle<v8::Value> params;
    args->GetNext(&params);
    scoped_ptr<content::V8ValueConverter> converter(
        content::V8ValueConverter::create());
    scoped_ptr<base::Value> params_value(
        converter->FromV8Value(params, context));

    const base::DictionaryValue* params_dict = NULL;
    if (params_value.get() && params_value->GetAsDictionary(&params_dict)) {
      params_dict->GetDouble("scale", &scale);
      params_dict->GetInteger("stop", &stop_index);
      params_dict->GetBoolean("overdraw", &overdraw);

      const base::Value* clip_value = NULL;
      if (params_dict->Get("clip", &clip_value))
        cc::MathUtil::FromValue(clip_value, &clip_rect);
    }
  }

  gfx::RectF clip(clip_rect);
  clip.Intersect(picture->LayerRect());
  clip.Scale(scale);
  gfx::Rect snapped_clip = gfx::ToEnclosingRect(clip);

  SkBitmap bitmap;
  if (!bitmap.setConfig(SkBitmap::kARGB_8888_Config, snapped_clip.width(),
                        snapped_clip.height()))
    return;
  if (!bitmap.allocPixels())
    return;
  bitmap.eraseARGB(0, 0, 0, 0);

  SkCanvas canvas(bitmap);
  canvas.translate(SkFloatToScalar(-clip.x()), SkFloatToScalar(-clip.y()));
  canvas.clipRect(gfx::RectToSkRect(snapped_clip));
  canvas.scale(scale, scale);
  canvas.translate(picture->LayerRect().x(), picture->LayerRect().y());

  // First, build a debug canvas for the given picture.
  SkDebugCanvas debug_canvas(picture->LayerRect().width(),
                             picture->LayerRect().height());
  picture->Replay(&debug_canvas);

  // Raster the requested command subset into the bitmap-backed canvas.
  int last_index = debug_canvas.getSize() - 1;
  if (last_index >= 0) {
    debug_canvas.setOverdrawViz(overdraw);
    debug_canvas.drawTo(
        &canvas,
        stop_index < 0 ? last_index : std::min(last_index, stop_index));
  }

  blink::WebArrayBuffer buffer =
      blink::WebArrayBuffer::create(bitmap.getSize(), 1);
  uint32* packed_pixels = reinterpret_cast<uint32*>(bitmap.getPixels());
  uint8* buffer_pixels = reinterpret_cast<uint8*>(buffer.data());
  // Swizzle from native Skia format to RGBA as we copy out.
  for (size_t i = 0; i < bitmap.getSize(); i += 4) {
    uint32 c = packed_pixels[i >> 2];
    buffer_pixels[i] = SkGetPackedR32(c);
    buffer_pixels[i + 1] = SkGetPackedG32(c);
    buffer_pixels[i + 2] = SkGetPackedB32(c);
    buffer_pixels[i + 3] = SkGetPackedA32(c);
  }

  v8::Handle<v8::Object> result = v8::Object::New(isolate);
  result->Set(v8::String::NewFromUtf8(isolate, "width"),
              v8::Number::New(isolate, snapped_clip.width()));
  result->Set(v8::String::NewFromUtf8(isolate, "height"),
              v8::Number::New(isolate, snapped_clip.height()));
  result->Set(v8::String::NewFromUtf8(isolate, "data"),
              blink::WebArrayBufferConverter::toV8Value(
                  &buffer, context->Global(), isolate));

  args->Return(result);
}

void SkiaBenchmarking::GetOps(gin::Arguments* args) {
  v8::Isolate* isolate = args->isolate();
  if (args->PeekNext().IsEmpty())
    return;
  v8::Handle<v8::Value> picture_handle;
  args->GetNext(&picture_handle);
  scoped_refptr<cc::Picture> picture =
      ParsePictureHash(isolate, picture_handle);
  if (!picture.get())
    return;

  gfx::Rect bounds = picture->LayerRect();
  SkDebugCanvas canvas(bounds.width(), bounds.height());
  picture->Replay(&canvas);

  v8::Handle<v8::Array> result = v8::Array::New(isolate, canvas.getSize());
  for (int i = 0; i < canvas.getSize(); ++i) {
    DrawType cmd_type = canvas.getDrawCommandAt(i)->getType();
    v8::Handle<v8::Object> cmd = v8::Object::New(isolate);
    cmd->Set(v8::String::NewFromUtf8(isolate, "cmd_type"),
             v8::Integer::New(isolate, cmd_type));
    cmd->Set(v8::String::NewFromUtf8(isolate, "cmd_string"),
             v8::String::NewFromUtf8(
                 isolate, SkDrawCommand::GetCommandString(cmd_type)));

    SkTDArray<SkString*>* info = canvas.getCommandInfo(i);
    DCHECK(info);

    v8::Local<v8::Array> v8_info = v8::Array::New(isolate, info->count());
    for (int j = 0; j < info->count(); ++j) {
      const SkString* info_str = (*info)[j];
      DCHECK(info_str);
      v8_info->Set(j, v8::String::NewFromUtf8(isolate, info_str->c_str()));
    }

    cmd->Set(v8::String::NewFromUtf8(isolate, "info"), v8_info);

    result->Set(i, cmd);
  }

  args->Return(result.As<v8::Object>());
}

void SkiaBenchmarking::GetOpTimings(gin::Arguments* args) {
  v8::Isolate* isolate = args->isolate();
  if (args->PeekNext().IsEmpty())
    return;
  v8::Handle<v8::Value> picture_handle;
  args->GetNext(&picture_handle);
  scoped_refptr<cc::Picture> picture =
      ParsePictureHash(isolate, picture_handle);
  if (!picture.get())
    return;

  gfx::Rect bounds = picture->LayerRect();

  // Measure the total time by drawing straight into a bitmap-backed canvas.
  SkBitmap bitmap;
  bitmap.allocN32Pixels(bounds.width(), bounds.height());
  SkCanvas bitmap_canvas(bitmap);
  bitmap_canvas.clear(SK_ColorTRANSPARENT);
  base::TimeTicks t0 = base::TimeTicks::HighResNow();
  picture->Replay(&bitmap_canvas);
  base::TimeDelta total_time = base::TimeTicks::HighResNow() - t0;

  // Gather per-op timing info by drawing into a BenchmarkingCanvas.
  skia::BenchmarkingCanvas benchmarking_canvas(bounds.width(), bounds.height());
  picture->Replay(&benchmarking_canvas);

  v8::Local<v8::Array> op_times =
      v8::Array::New(isolate, benchmarking_canvas.CommandCount());
  for (size_t i = 0; i < benchmarking_canvas.CommandCount(); ++i)
    op_times->Set(i, v8::Number::New(isolate, benchmarking_canvas.GetTime(i)));

  v8::Handle<v8::Object> result = v8::Object::New(isolate);
  result->Set(v8::String::NewFromUtf8(isolate, "total_time"),
              v8::Number::New(isolate, total_time.InMillisecondsF()));
  result->Set(v8::String::NewFromUtf8(isolate, "cmd_times"), op_times);

  args->Return(result);
}

void SkiaBenchmarking::GetInfo(gin::Arguments* args) {
  v8::Isolate* isolate = args->isolate();
  if (args->PeekNext().IsEmpty())
    return;
  v8::Handle<v8::Value> picture_handle;
  args->GetNext(&picture_handle);
  scoped_refptr<cc::Picture> picture =
      ParsePictureStr(isolate, picture_handle);
  if (!picture.get())
    return;

  v8::Handle<v8::Object> result = v8::Object::New(isolate);
  result->Set(v8::String::NewFromUtf8(isolate, "width"),
              v8::Number::New(isolate, picture->LayerRect().width()));
  result->Set(v8::String::NewFromUtf8(isolate, "height"),
              v8::Number::New(isolate, picture->LayerRect().height()));

  args->Return(result);
}

} // namespace content
