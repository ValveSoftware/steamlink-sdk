// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/overscroll_glow.h"

#include "base/debug/trace_event.h"
#include "base/lazy_instance.h"
#include "base/threading/worker_pool.h"
#include "cc/layers/image_layer.h"
#include "content/browser/android/edge_effect.h"
#include "skia/ext/image_operations.h"
#include "ui/gfx/android/java_bitmap.h"

using std::max;
using std::min;

namespace content {

namespace {

const float kEpsilon = 1e-3f;
const int kScaledEdgeHeight = 12;
const int kScaledGlowHeight = 64;
const float kEdgeHeightAtMdpi = 12.f;
const float kGlowHeightAtMdpi = 128.f;

SkBitmap CreateSkBitmapFromAndroidResource(const char* name, gfx::Size size) {
  base::android::ScopedJavaLocalRef<jobject> jobj =
      gfx::CreateJavaBitmapFromAndroidResource(name, size);
  if (jobj.is_null())
    return SkBitmap();

  SkBitmap bitmap = CreateSkBitmapFromJavaBitmap(gfx::JavaBitmap(jobj.obj()));
  if (bitmap.isNull())
    return bitmap;

  return skia::ImageOperations::Resize(
      bitmap, skia::ImageOperations::RESIZE_BOX, size.width(), size.height());
}

class OverscrollResources {
 public:
  OverscrollResources() {
    TRACE_EVENT0("browser", "OverscrollResources::Create");
    edge_bitmap_ =
        CreateSkBitmapFromAndroidResource("android:drawable/overscroll_edge",
                                          gfx::Size(128, kScaledEdgeHeight));
    glow_bitmap_ =
        CreateSkBitmapFromAndroidResource("android:drawable/overscroll_glow",
                                          gfx::Size(128, kScaledGlowHeight));
  }

  const SkBitmap& edge_bitmap() const { return edge_bitmap_; }
  const SkBitmap& glow_bitmap() const { return glow_bitmap_; }

 private:
  SkBitmap edge_bitmap_;
  SkBitmap glow_bitmap_;

  DISALLOW_COPY_AND_ASSIGN(OverscrollResources);
};

// Leaky to allow access from a worker thread.
base::LazyInstance<OverscrollResources>::Leaky g_overscroll_resources =
    LAZY_INSTANCE_INITIALIZER;

scoped_refptr<cc::Layer> CreateImageLayer(const SkBitmap& bitmap) {
  scoped_refptr<cc::ImageLayer> layer = cc::ImageLayer::Create();
  layer->SetBitmap(bitmap);
  return layer;
}

bool IsApproxZero(float value) {
  return std::abs(value) < kEpsilon;
}

gfx::Vector2dF ZeroSmallComponents(gfx::Vector2dF vector) {
  if (IsApproxZero(vector.x()))
    vector.set_x(0);
  if (IsApproxZero(vector.y()))
    vector.set_y(0);
  return vector;
}

// Force loading of any necessary resources.  This function is thread-safe.
void EnsureResources() {
  g_overscroll_resources.Get();
}

} // namespace

scoped_ptr<OverscrollGlow> OverscrollGlow::Create(bool enabled) {
  // Don't block the main thread with effect resource loading during creation.
  // Effect instantiation is deferred until the effect overscrolls, in which
  // case the main thread may block until the resource has loaded.
  if (enabled && g_overscroll_resources == NULL)
    base::WorkerPool::PostTask(FROM_HERE, base::Bind(EnsureResources), true);

  return make_scoped_ptr(new OverscrollGlow(enabled));
}

OverscrollGlow::OverscrollGlow(bool enabled)
    : enabled_(enabled), initialized_(false) {}

OverscrollGlow::~OverscrollGlow() {
  Detach();
}

void OverscrollGlow::Enable() {
  enabled_ = true;
}

void OverscrollGlow::Disable() {
  if (!enabled_)
    return;
  enabled_ = false;
  if (!enabled_ && initialized_) {
    Detach();
    for (size_t i = 0; i < EdgeEffect::EDGE_COUNT; ++i)
      edge_effects_[i]->Finish();
  }
}

bool OverscrollGlow::OnOverscrolled(cc::Layer* overscrolling_layer,
                                    base::TimeTicks current_time,
                                    gfx::Vector2dF accumulated_overscroll,
                                    gfx::Vector2dF overscroll_delta,
                                    gfx::Vector2dF velocity) {
  DCHECK(overscrolling_layer);

  if (!enabled_)
    return false;

  // The size of the glow determines the relative effect of the inputs; an
  // empty-sized effect is effectively disabled.
  if (display_params_.size.IsEmpty())
    return false;

  // Ignore sufficiently small values that won't meaningfuly affect animation.
  overscroll_delta = ZeroSmallComponents(overscroll_delta);
  if (overscroll_delta.IsZero()) {
    if (initialized_) {
      Release(current_time);
      UpdateLayerAttachment(overscrolling_layer);
    }
    return NeedsAnimate();
  }

  if (!InitializeIfNecessary())
    return false;

  gfx::Vector2dF old_overscroll = accumulated_overscroll - overscroll_delta;
  bool x_overscroll_started =
      !IsApproxZero(overscroll_delta.x()) && IsApproxZero(old_overscroll.x());
  bool y_overscroll_started =
      !IsApproxZero(overscroll_delta.y()) && IsApproxZero(old_overscroll.y());

  if (x_overscroll_started)
    ReleaseAxis(AXIS_X, current_time);
  if (y_overscroll_started)
    ReleaseAxis(AXIS_Y, current_time);

  velocity = ZeroSmallComponents(velocity);
  if (!velocity.IsZero())
    Absorb(current_time, velocity, x_overscroll_started, y_overscroll_started);
  else
    Pull(current_time, overscroll_delta);

  UpdateLayerAttachment(overscrolling_layer);
  return NeedsAnimate();
}

bool OverscrollGlow::Animate(base::TimeTicks current_time) {
  if (!NeedsAnimate()) {
    Detach();
    return false;
  }

  for (size_t i = 0; i < EdgeEffect::EDGE_COUNT; ++i) {
    if (edge_effects_[i]->Update(current_time)) {
      edge_effects_[i]->ApplyToLayers(
          display_params_.size,
          static_cast<EdgeEffect::Edge>(i),
          kEdgeHeightAtMdpi * display_params_.device_scale_factor,
          kGlowHeightAtMdpi * display_params_.device_scale_factor,
          display_params_.edge_offsets[i]);
    }
  }

  if (!NeedsAnimate()) {
    Detach();
    return false;
  }

  return true;
}

void OverscrollGlow::UpdateDisplayParameters(const DisplayParameters& params) {
  display_params_ = params;
}

bool OverscrollGlow::NeedsAnimate() const {
  if (!enabled_ || !initialized_)
    return false;
  for (size_t i = 0; i < EdgeEffect::EDGE_COUNT; ++i) {
    if (!edge_effects_[i]->IsFinished())
      return true;
  }
  return false;
}

void OverscrollGlow::UpdateLayerAttachment(cc::Layer* parent) {
  DCHECK(parent);
  if (!root_layer_)
    return;

  if (!NeedsAnimate()) {
    Detach();
    return;
  }

  if (root_layer_->parent() != parent)
    parent->AddChild(root_layer_);
}

void OverscrollGlow::Detach() {
  if (root_layer_)
    root_layer_->RemoveFromParent();
}

bool OverscrollGlow::InitializeIfNecessary() {
  DCHECK(enabled_);
  if (initialized_)
    return true;

  const SkBitmap& edge = g_overscroll_resources.Get().edge_bitmap();
  const SkBitmap& glow = g_overscroll_resources.Get().glow_bitmap();
  if (edge.isNull() || glow.isNull()) {
    Disable();
    return false;
  }

  DCHECK(!root_layer_);
  root_layer_ = cc::Layer::Create();
  for (size_t i = 0; i < EdgeEffect::EDGE_COUNT; ++i) {
    scoped_refptr<cc::Layer> edge_layer = CreateImageLayer(edge);
    scoped_refptr<cc::Layer> glow_layer = CreateImageLayer(glow);
    root_layer_->AddChild(edge_layer);
    root_layer_->AddChild(glow_layer);
    edge_effects_[i] = make_scoped_ptr(new EdgeEffect(edge_layer, glow_layer));
  }

  initialized_ = true;
  return true;
}

void OverscrollGlow::Pull(base::TimeTicks current_time,
                          gfx::Vector2dF overscroll_delta) {
  DCHECK(enabled_ && initialized_);
  overscroll_delta = ZeroSmallComponents(overscroll_delta);
  if (overscroll_delta.IsZero())
    return;

  gfx::Vector2dF overscroll_pull =
      gfx::ScaleVector2d(overscroll_delta,
                         1.f / display_params_.size.width(),
                         1.f / display_params_.size.height());
  float edge_overscroll_pull[EdgeEffect::EDGE_COUNT] = {
      min(overscroll_pull.y(), 0.f),  // Top
      min(overscroll_pull.x(), 0.f),  // Left
      max(overscroll_pull.y(), 0.f),  // Bottom
      max(overscroll_pull.x(), 0.f)   // Right
  };

  for (size_t i = 0; i < EdgeEffect::EDGE_COUNT; ++i) {
    if (!edge_overscroll_pull[i])
      continue;

    edge_effects_[i]->Pull(current_time, std::abs(edge_overscroll_pull[i]));
    GetOppositeEdge(i)->Release(current_time);
  }
}

void OverscrollGlow::Absorb(base::TimeTicks current_time,
                            gfx::Vector2dF velocity,
                            bool x_overscroll_started,
                            bool y_overscroll_started) {
  DCHECK(enabled_ && initialized_);
  if (velocity.IsZero())
    return;

  // Only trigger on initial overscroll at a non-zero velocity
  const float overscroll_velocities[EdgeEffect::EDGE_COUNT] = {
      y_overscroll_started ? min(velocity.y(), 0.f) : 0,  // Top
      x_overscroll_started ? min(velocity.x(), 0.f) : 0,  // Left
      y_overscroll_started ? max(velocity.y(), 0.f) : 0,  // Bottom
      x_overscroll_started ? max(velocity.x(), 0.f) : 0   // Right
  };

  for (size_t i = 0; i < EdgeEffect::EDGE_COUNT; ++i) {
    if (!overscroll_velocities[i])
      continue;

    edge_effects_[i]->Absorb(current_time, std::abs(overscroll_velocities[i]));
    GetOppositeEdge(i)->Release(current_time);
  }
}

void OverscrollGlow::Release(base::TimeTicks current_time) {
  DCHECK(initialized_);
  for (size_t i = 0; i < EdgeEffect::EDGE_COUNT; ++i)
    edge_effects_[i]->Release(current_time);
}

void OverscrollGlow::ReleaseAxis(Axis axis, base::TimeTicks current_time) {
  DCHECK(initialized_);
  switch (axis) {
    case AXIS_X:
      edge_effects_[EdgeEffect::EDGE_LEFT]->Release(current_time);
      edge_effects_[EdgeEffect::EDGE_RIGHT]->Release(current_time);
      break;
    case AXIS_Y:
      edge_effects_[EdgeEffect::EDGE_TOP]->Release(current_time);
      edge_effects_[EdgeEffect::EDGE_BOTTOM]->Release(current_time);
      break;
  };
}

EdgeEffect* OverscrollGlow::GetOppositeEdge(int edge_index) {
  DCHECK(initialized_);
  return edge_effects_[(edge_index + 2) % EdgeEffect::EDGE_COUNT].get();
}

OverscrollGlow::DisplayParameters::DisplayParameters()
    : device_scale_factor(1) {
  edge_offsets[0] = edge_offsets[1] = edge_offsets[2] = edge_offsets[3] = 0.f;
}

}  // namespace content
