// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/android/delegated_frame_host_android.h"

#include "base/bind.h"
#include "base/logging.h"
#include "cc/layers/solid_color_layer.h"
#include "cc/layers/surface_layer.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/copy_output_result.h"
#include "cc/surfaces/surface.h"
#include "cc/surfaces/surface_id.h"
#include "cc/surfaces/surface_id_allocator.h"
#include "cc/surfaces/surface_manager.h"
#include "ui/android/context_provider_factory.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android_compositor.h"
#include "ui/gfx/android/device_display_info.h"
#include "ui/gfx/geometry/dip_util.h"

namespace ui {

namespace {

void SatisfyCallback(cc::SurfaceManager* manager,
                     const cc::SurfaceSequence& sequence) {
  std::vector<uint32_t> sequences;
  sequences.push_back(sequence.sequence);
  manager->DidSatisfySequences(sequence.frame_sink_id, &sequences);
}

void RequireCallback(cc::SurfaceManager* manager,
                     const cc::SurfaceId& id,
                     const cc::SurfaceSequence& sequence) {
  cc::Surface* surface = manager->GetSurfaceForId(id);
  if (!surface) {
    LOG(ERROR) << "Attempting to require callback on nonexistent surface";
    return;
  }
  surface->AddDestructionDependency(sequence);
}

scoped_refptr<cc::SurfaceLayer> CreateSurfaceLayer(
    cc::SurfaceManager* surface_manager,
    cc::SurfaceId surface_id,
    const gfx::Size surface_size,
    bool surface_opaque) {
  // manager must outlive compositors using it.
  scoped_refptr<cc::SurfaceLayer> layer = cc::SurfaceLayer::Create(
      base::Bind(&SatisfyCallback, base::Unretained(surface_manager)),
      base::Bind(&RequireCallback, base::Unretained(surface_manager)));
  layer->SetSurfaceId(surface_id, 1.f, surface_size);
  layer->SetBounds(surface_size);
  layer->SetIsDrawable(true);
  layer->SetContentsOpaque(surface_opaque);

  return layer;
}

void CopyOutputRequestCallback(
    scoped_refptr<cc::Layer> readback_layer,
    cc::CopyOutputRequest::CopyOutputRequestCallback result_callback,
    std::unique_ptr<cc::CopyOutputResult> copy_output_result) {
  readback_layer->RemoveFromParent();
  result_callback.Run(std::move(copy_output_result));
}

}  // namespace

DelegatedFrameHostAndroid::DelegatedFrameHostAndroid(
    ui::ViewAndroid* view,
    SkColor background_color,
    ReturnResourcesCallback return_resources_callback)
    : frame_sink_id_(
          ui::ContextProviderFactory::GetInstance()->AllocateFrameSinkId()),
      view_(view),
      return_resources_callback_(return_resources_callback),
      background_layer_(cc::SolidColorLayer::Create()) {
  DCHECK(view_);
  DCHECK(!return_resources_callback_.is_null());

  surface_manager_ =
      ui::ContextProviderFactory::GetInstance()->GetSurfaceManager();
  surface_id_allocator_.reset(new cc::SurfaceIdAllocator());
  surface_manager_->RegisterFrameSinkId(frame_sink_id_);
  surface_factory_ = base::WrapUnique(
      new cc::SurfaceFactory(frame_sink_id_, surface_manager_, this));

  background_layer_->SetBackgroundColor(background_color);
  view_->GetLayer()->AddChild(background_layer_);
  UpdateBackgroundLayer();
}

DelegatedFrameHostAndroid::~DelegatedFrameHostAndroid() {
  DestroyDelegatedContent();
  surface_factory_.reset();
  UnregisterFrameSinkHierarchy();
  surface_manager_->InvalidateFrameSinkId(frame_sink_id_);
  background_layer_->RemoveFromParent();
}

DelegatedFrameHostAndroid::FrameData::FrameData() = default;

DelegatedFrameHostAndroid::FrameData::~FrameData() = default;

void DelegatedFrameHostAndroid::SubmitCompositorFrame(
    cc::CompositorFrame frame,
    cc::SurfaceFactory::DrawCallback draw_callback) {
  cc::RenderPass* root_pass =
      frame.delegated_frame_data->render_pass_list.back().get();
  gfx::Size surface_size = root_pass->output_rect.size();

  if (!current_frame_ || surface_size != current_frame_->surface_size ||
      current_frame_->top_controls_height !=
          frame.metadata.top_controls_height ||
      current_frame_->top_controls_shown_ratio !=
          frame.metadata.top_controls_shown_ratio ||
      current_frame_->bottom_controls_height !=
          frame.metadata.bottom_controls_height ||
      current_frame_->bottom_controls_shown_ratio !=
          frame.metadata.bottom_controls_shown_ratio ||
      current_frame_->viewport_selection != frame.metadata.selection ||
      current_frame_->has_transparent_background !=
          root_pass->has_transparent_background) {
    DestroyDelegatedContent();
    DCHECK(!content_layer_);
    DCHECK(!current_frame_);

    current_frame_ = base::MakeUnique<FrameData>();
    current_frame_->local_frame_id = surface_id_allocator_->GenerateId();
    surface_factory_->Create(current_frame_->local_frame_id);

    current_frame_->surface_size = surface_size;
    current_frame_->top_controls_height = frame.metadata.top_controls_height;
    current_frame_->top_controls_shown_ratio =
        frame.metadata.top_controls_shown_ratio;
    current_frame_->bottom_controls_height =
        frame.metadata.bottom_controls_height;
    current_frame_->bottom_controls_shown_ratio =
        frame.metadata.bottom_controls_shown_ratio;
    current_frame_->has_transparent_background =
        root_pass->has_transparent_background;

    current_frame_->viewport_selection = frame.metadata.selection;
    content_layer_ = CreateSurfaceLayer(
        surface_manager_, cc::SurfaceId(surface_factory_->frame_sink_id(),
                                        current_frame_->local_frame_id),
        current_frame_->surface_size,
        !current_frame_->has_transparent_background);
    view_->GetLayer()->AddChild(content_layer_);
    UpdateBackgroundLayer();
  }

  surface_factory_->SubmitCompositorFrame(current_frame_->local_frame_id,
                                          std::move(frame), draw_callback);
}

cc::FrameSinkId DelegatedFrameHostAndroid::GetFrameSinkId() const {
  return frame_sink_id_;
}

void DelegatedFrameHostAndroid::RequestCopyOfSurface(
    WindowAndroidCompositor* compositor,
    const gfx::Rect& src_subrect_in_pixel,
    cc::CopyOutputRequest::CopyOutputRequestCallback result_callback) {
  DCHECK(current_frame_);
  DCHECK(!result_callback.is_null());

  scoped_refptr<cc::Layer> readback_layer = CreateSurfaceLayer(
      surface_manager_, cc::SurfaceId(surface_factory_->frame_sink_id(),
                                      current_frame_->local_frame_id),
      current_frame_->surface_size,
      !current_frame_->has_transparent_background);
  readback_layer->SetHideLayerAndSubtree(true);
  compositor->AttachLayerForReadback(readback_layer);
  std::unique_ptr<cc::CopyOutputRequest> copy_output_request =
      cc::CopyOutputRequest::CreateRequest(base::Bind(
          &CopyOutputRequestCallback, readback_layer, result_callback));

  if (!src_subrect_in_pixel.IsEmpty())
    copy_output_request->set_area(src_subrect_in_pixel);

  surface_factory_->RequestCopyOfSurface(current_frame_->local_frame_id,
                                         std::move(copy_output_request));
}

void DelegatedFrameHostAndroid::DestroyDelegatedContent() {
  if (!current_frame_)
    return;

  DCHECK(content_layer_);

  content_layer_->RemoveFromParent();
  content_layer_ = nullptr;
  surface_factory_->Destroy(current_frame_->local_frame_id);
  current_frame_.reset();

  UpdateBackgroundLayer();
}

bool DelegatedFrameHostAndroid::HasDelegatedContent() const {
  return current_frame_.get() != nullptr;
}

void DelegatedFrameHostAndroid::CompositorFrameSinkChanged() {
  DestroyDelegatedContent();
  surface_factory_->Reset();
}

void DelegatedFrameHostAndroid::UpdateBackgroundColor(SkColor color) {
  background_layer_->SetBackgroundColor(color);
}

void DelegatedFrameHostAndroid::UpdateContainerSizeinDIP(
    const gfx::Size& size_in_dip) {
  container_size_in_dip_ = size_in_dip;
  background_layer_->SetBounds(gfx::ConvertSizeToPixel(
      gfx::DeviceDisplayInfo().GetDIPScale(), container_size_in_dip_));
  UpdateBackgroundLayer();
}

void DelegatedFrameHostAndroid::RegisterFrameSinkHierarchy(
    const cc::FrameSinkId& parent_id) {
  if (registered_parent_frame_sink_id_.is_valid())
    UnregisterFrameSinkHierarchy();
  registered_parent_frame_sink_id_ = parent_id;
  surface_manager_->RegisterSurfaceFactoryClient(frame_sink_id_, this);
  surface_manager_->RegisterFrameSinkHierarchy(parent_id, frame_sink_id_);
}

void DelegatedFrameHostAndroid::UnregisterFrameSinkHierarchy() {
  if (!registered_parent_frame_sink_id_.is_valid())
    return;
  surface_manager_->UnregisterSurfaceFactoryClient(frame_sink_id_);
  surface_manager_->UnregisterFrameSinkHierarchy(
      registered_parent_frame_sink_id_, frame_sink_id_);
  registered_parent_frame_sink_id_ = cc::FrameSinkId();
}

void DelegatedFrameHostAndroid::ReturnResources(
    const cc::ReturnedResourceArray& resources) {
  return_resources_callback_.Run(resources);
}

void DelegatedFrameHostAndroid::SetBeginFrameSource(
    cc::BeginFrameSource* begin_frame_source) {
  // TODO(enne): hook this up instead of making RWHVAndroid a
  // WindowAndroidObserver.
}

void DelegatedFrameHostAndroid::UpdateBackgroundLayer() {
  // The background layer draws in 2 cases:
  // 1) When we don't have any content from the renderer.
  // 2) When the bounds of the content received from the renderer does not match
  // the desired content bounds.
  bool background_is_drawable = false;

  if (current_frame_) {
    float device_scale_factor = gfx::DeviceDisplayInfo().GetDIPScale();
    gfx::Size content_size_in_dip = gfx::ConvertSizeToDIP(
        device_scale_factor, current_frame_->surface_size);
    background_is_drawable =
        content_size_in_dip.width() < container_size_in_dip_.width() ||
        content_size_in_dip.height() < container_size_in_dip_.height();
  } else {
    background_is_drawable = true;
  }

  background_layer_->SetIsDrawable(background_is_drawable);
}

}  // namespace ui
