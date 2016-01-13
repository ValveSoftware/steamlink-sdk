// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/layer.h"

#include <algorithm>

#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "cc/base/scoped_ptr_algorithm.h"
#include "cc/layers/content_layer.h"
#include "cc/layers/delegated_renderer_layer.h"
#include "cc/layers/picture_layer.h"
#include "cc/layers/solid_color_layer.h"
#include "cc/layers/texture_layer.h"
#include "cc/output/copy_output_request.h"
#include "cc/output/delegated_frame_data.h"
#include "cc/output/filter_operation.h"
#include "cc/output/filter_operations.h"
#include "cc/resources/transferable_resource.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/compositor/dip_util.h"
#include "ui/compositor/layer_animator.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/display.h"
#include "ui/gfx/interpolated_transform.h"
#include "ui/gfx/point3_f.h"
#include "ui/gfx/point_conversions.h"
#include "ui/gfx/size_conversions.h"

namespace {

const ui::Layer* GetRoot(const ui::Layer* layer) {
  while (layer->parent())
    layer = layer->parent();
  return layer;
}

struct UIImplSidePaintingStatus {
  UIImplSidePaintingStatus()
      : enabled(ui::IsUIImplSidePaintingEnabled()) {
  }
  bool enabled;
};
base::LazyInstance<UIImplSidePaintingStatus> g_ui_impl_side_painting_status =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

namespace ui {

Layer::Layer()
    : type_(LAYER_TEXTURED),
      compositor_(NULL),
      parent_(NULL),
      visible_(true),
      force_render_surface_(false),
      fills_bounds_opaquely_(true),
      fills_bounds_completely_(false),
      background_blur_radius_(0),
      layer_saturation_(0.0f),
      layer_brightness_(0.0f),
      layer_grayscale_(0.0f),
      layer_inverted_(false),
      layer_mask_(NULL),
      layer_mask_back_link_(NULL),
      zoom_(1),
      zoom_inset_(0),
      delegate_(NULL),
      owner_(NULL),
      cc_layer_(NULL),
      device_scale_factor_(1.0f) {
  CreateWebLayer();
}

Layer::Layer(LayerType type)
    : type_(type),
      compositor_(NULL),
      parent_(NULL),
      visible_(true),
      force_render_surface_(false),
      fills_bounds_opaquely_(true),
      fills_bounds_completely_(false),
      background_blur_radius_(0),
      layer_saturation_(0.0f),
      layer_brightness_(0.0f),
      layer_grayscale_(0.0f),
      layer_inverted_(false),
      layer_mask_(NULL),
      layer_mask_back_link_(NULL),
      zoom_(1),
      zoom_inset_(0),
      delegate_(NULL),
      owner_(NULL),
      cc_layer_(NULL),
      device_scale_factor_(1.0f) {
  CreateWebLayer();
}

Layer::~Layer() {
  // Destroying the animator may cause observers to use the layer (and
  // indirectly the WebLayer). Destroy the animator first so that the WebLayer
  // is still around.
  if (animator_.get())
    animator_->SetDelegate(NULL);
  animator_ = NULL;
  if (compositor_)
    compositor_->SetRootLayer(NULL);
  if (parent_)
    parent_->Remove(this);
  if (layer_mask_)
    SetMaskLayer(NULL);
  if (layer_mask_back_link_)
    layer_mask_back_link_->SetMaskLayer(NULL);
  for (size_t i = 0; i < children_.size(); ++i)
    children_[i]->parent_ = NULL;
  cc_layer_->RemoveLayerAnimationEventObserver(this);
  cc_layer_->RemoveFromParent();
}

// static
bool Layer::UsingPictureLayer() {
  return g_ui_impl_side_painting_status.Get().enabled;
}

Compositor* Layer::GetCompositor() {
  return GetRoot(this)->compositor_;
}

float Layer::opacity() const {
  return cc_layer_->opacity();
}

void Layer::SetCompositor(Compositor* compositor) {
  // This function must only be called to set the compositor on the root layer,
  // or to reset it.
  DCHECK(!compositor || !compositor_);
  DCHECK(!compositor || compositor->root_layer() == this);
  DCHECK(!parent_);
  if (compositor_) {
    RemoveAnimatorsInTreeFromCollection(
        compositor_->layer_animator_collection());
  }
  compositor_ = compositor;
  if (compositor) {
    OnDeviceScaleFactorChanged(compositor->device_scale_factor());
    SendPendingThreadedAnimations();
    AddAnimatorsInTreeToCollection(compositor_->layer_animator_collection());
  }
}

void Layer::Add(Layer* child) {
  DCHECK(!child->compositor_);
  if (child->parent_)
    child->parent_->Remove(child);
  child->parent_ = this;
  children_.push_back(child);
  cc_layer_->AddChild(child->cc_layer_);
  child->OnDeviceScaleFactorChanged(device_scale_factor_);
  if (GetCompositor())
    child->SendPendingThreadedAnimations();
  LayerAnimatorCollection* collection = GetLayerAnimatorCollection();
  if (collection)
    child->AddAnimatorsInTreeToCollection(collection);
}

void Layer::Remove(Layer* child) {
  // Current bounds are used to calculate offsets when layers are reparented.
  // Stop (and complete) an ongoing animation to update the bounds immediately.
  LayerAnimator* child_animator = child->animator_;
  if (child_animator)
    child_animator->StopAnimatingProperty(ui::LayerAnimationElement::BOUNDS);
  LayerAnimatorCollection* collection = GetLayerAnimatorCollection();
  if (collection)
    child->RemoveAnimatorsInTreeFromCollection(collection);

  std::vector<Layer*>::iterator i =
      std::find(children_.begin(), children_.end(), child);
  DCHECK(i != children_.end());
  children_.erase(i);
  child->parent_ = NULL;
  child->cc_layer_->RemoveFromParent();
}

void Layer::StackAtTop(Layer* child) {
  if (children_.size() <= 1 || child == children_.back())
    return;  // Already in front.
  StackAbove(child, children_.back());
}

void Layer::StackAbove(Layer* child, Layer* other) {
  StackRelativeTo(child, other, true);
}

void Layer::StackAtBottom(Layer* child) {
  if (children_.size() <= 1 || child == children_.front())
    return;  // Already on bottom.
  StackBelow(child, children_.front());
}

void Layer::StackBelow(Layer* child, Layer* other) {
  StackRelativeTo(child, other, false);
}

bool Layer::Contains(const Layer* other) const {
  for (const Layer* parent = other; parent; parent = parent->parent()) {
    if (parent == this)
      return true;
  }
  return false;
}

void Layer::SetAnimator(LayerAnimator* animator) {
  if (animator)
    animator->SetDelegate(this);
  animator_ = animator;
}

LayerAnimator* Layer::GetAnimator() {
  if (!animator_.get())
    SetAnimator(LayerAnimator::CreateDefaultAnimator());
  return animator_.get();
}

void Layer::SetTransform(const gfx::Transform& transform) {
  GetAnimator()->SetTransform(transform);
}

gfx::Transform Layer::GetTargetTransform() const {
  if (animator_.get() && animator_->IsAnimatingProperty(
      LayerAnimationElement::TRANSFORM)) {
    return animator_->GetTargetTransform();
  }
  return transform();
}

void Layer::SetBounds(const gfx::Rect& bounds) {
  GetAnimator()->SetBounds(bounds);
}

void Layer::SetSubpixelPositionOffset(const gfx::Vector2dF offset) {
  subpixel_position_offset_ = offset;
  RecomputePosition();
}

gfx::Rect Layer::GetTargetBounds() const {
  if (animator_.get() && animator_->IsAnimatingProperty(
      LayerAnimationElement::BOUNDS)) {
    return animator_->GetTargetBounds();
  }
  return bounds_;
}

void Layer::SetMasksToBounds(bool masks_to_bounds) {
  cc_layer_->SetMasksToBounds(masks_to_bounds);
}

bool Layer::GetMasksToBounds() const {
  return cc_layer_->masks_to_bounds();
}

void Layer::SetOpacity(float opacity) {
  GetAnimator()->SetOpacity(opacity);
}

float Layer::GetCombinedOpacity() const {
  float opacity = this->opacity();
  Layer* current = this->parent_;
  while (current) {
    opacity *= current->opacity();
    current = current->parent_;
  }
  return opacity;
}

void Layer::SetBackgroundBlur(int blur_radius) {
  background_blur_radius_ = blur_radius;

  SetLayerBackgroundFilters();
}

void Layer::SetLayerSaturation(float saturation) {
  layer_saturation_ = saturation;
  SetLayerFilters();
}

void Layer::SetLayerBrightness(float brightness) {
  GetAnimator()->SetBrightness(brightness);
}

float Layer::GetTargetBrightness() const {
  if (animator_.get() && animator_->IsAnimatingProperty(
      LayerAnimationElement::BRIGHTNESS)) {
    return animator_->GetTargetBrightness();
  }
  return layer_brightness();
}

void Layer::SetLayerGrayscale(float grayscale) {
  GetAnimator()->SetGrayscale(grayscale);
}

float Layer::GetTargetGrayscale() const {
  if (animator_.get() && animator_->IsAnimatingProperty(
      LayerAnimationElement::GRAYSCALE)) {
    return animator_->GetTargetGrayscale();
  }
  return layer_grayscale();
}

void Layer::SetLayerInverted(bool inverted) {
  layer_inverted_ = inverted;
  SetLayerFilters();
}

void Layer::SetMaskLayer(Layer* layer_mask) {
  // The provided mask should not have a layer mask itself.
  DCHECK(!layer_mask ||
         (!layer_mask->layer_mask_layer() &&
          layer_mask->children().empty() &&
          !layer_mask->layer_mask_back_link_));
  DCHECK(!layer_mask_back_link_);
  if (layer_mask_ == layer_mask)
    return;
  // We need to de-reference the currently linked object so that no problem
  // arises if the mask layer gets deleted before this object.
  if (layer_mask_)
    layer_mask_->layer_mask_back_link_ = NULL;
  layer_mask_ = layer_mask;
  cc_layer_->SetMaskLayer(
      layer_mask ? layer_mask->cc_layer() : NULL);
  // We need to reference the linked object so that it can properly break the
  // link to us when it gets deleted.
  if (layer_mask) {
    layer_mask->layer_mask_back_link_ = this;
    layer_mask->OnDeviceScaleFactorChanged(device_scale_factor_);
  }
}

void Layer::SetBackgroundZoom(float zoom, int inset) {
  zoom_ = zoom;
  zoom_inset_ = inset;

  SetLayerBackgroundFilters();
}

void Layer::SetAlphaShape(scoped_ptr<SkRegion> region) {
  alpha_shape_ = region.Pass();

  SetLayerFilters();
}

void Layer::SetLayerFilters() {
  cc::FilterOperations filters;
  if (layer_saturation_) {
    filters.Append(cc::FilterOperation::CreateSaturateFilter(
        layer_saturation_));
  }
  if (layer_grayscale_) {
    filters.Append(cc::FilterOperation::CreateGrayscaleFilter(
        layer_grayscale_));
  }
  if (layer_inverted_)
    filters.Append(cc::FilterOperation::CreateInvertFilter(1.0));
  // Brightness goes last, because the resulting colors neeed clamping, which
  // cause further color matrix filters to be applied separately. In this order,
  // they all can be combined in a single pass.
  if (layer_brightness_) {
    filters.Append(cc::FilterOperation::CreateSaturatingBrightnessFilter(
        layer_brightness_));
  }
  if (alpha_shape_) {
    filters.Append(cc::FilterOperation::CreateAlphaThresholdFilter(
            *alpha_shape_, 1.f, 0.f));
  }

  cc_layer_->SetFilters(filters);
}

void Layer::SetLayerBackgroundFilters() {
  cc::FilterOperations filters;
  if (zoom_ != 1)
    filters.Append(cc::FilterOperation::CreateZoomFilter(zoom_, zoom_inset_));

  if (background_blur_radius_) {
    filters.Append(cc::FilterOperation::CreateBlurFilter(
        background_blur_radius_));
  }

  cc_layer_->SetBackgroundFilters(filters);
}

float Layer::GetTargetOpacity() const {
  if (animator_.get() && animator_->IsAnimatingProperty(
      LayerAnimationElement::OPACITY))
    return animator_->GetTargetOpacity();
  return opacity();
}

void Layer::SetVisible(bool visible) {
  GetAnimator()->SetVisibility(visible);
}

bool Layer::GetTargetVisibility() const {
  if (animator_.get() && animator_->IsAnimatingProperty(
      LayerAnimationElement::VISIBILITY))
    return animator_->GetTargetVisibility();
  return visible_;
}

bool Layer::IsDrawn() const {
  const Layer* layer = this;
  while (layer && layer->visible_)
    layer = layer->parent_;
  return layer == NULL;
}

bool Layer::ShouldDraw() const {
  return type_ != LAYER_NOT_DRAWN && GetCombinedOpacity() > 0.0f;
}

// static
void Layer::ConvertPointToLayer(const Layer* source,
                                const Layer* target,
                                gfx::Point* point) {
  if (source == target)
    return;

  const Layer* root_layer = GetRoot(source);
  CHECK_EQ(root_layer, GetRoot(target));

  if (source != root_layer)
    source->ConvertPointForAncestor(root_layer, point);
  if (target != root_layer)
    target->ConvertPointFromAncestor(root_layer, point);
}

bool Layer::GetTargetTransformRelativeTo(const Layer* ancestor,
                                         gfx::Transform* transform) const {
  const Layer* p = this;
  for (; p && p != ancestor; p = p->parent()) {
    gfx::Transform translation;
    translation.Translate(static_cast<float>(p->bounds().x()),
                          static_cast<float>(p->bounds().y()));
    // Use target transform so that result will be correct once animation is
    // finished.
    if (!p->GetTargetTransform().IsIdentity())
      transform->ConcatTransform(p->GetTargetTransform());
    transform->ConcatTransform(translation);
  }
  return p == ancestor;
}

void Layer::SetFillsBoundsOpaquely(bool fills_bounds_opaquely) {
  if (fills_bounds_opaquely_ == fills_bounds_opaquely)
    return;

  fills_bounds_opaquely_ = fills_bounds_opaquely;

  cc_layer_->SetContentsOpaque(fills_bounds_opaquely);
}

void Layer::SetFillsBoundsCompletely(bool fills_bounds_completely) {
  fills_bounds_completely_ = fills_bounds_completely;
}

void Layer::SwitchToLayer(scoped_refptr<cc::Layer> new_layer) {
  // Finish animations being handled by cc_layer_.
  if (animator_.get()) {
    animator_->StopAnimatingProperty(LayerAnimationElement::TRANSFORM);
    animator_->StopAnimatingProperty(LayerAnimationElement::OPACITY);
  }

  if (texture_layer_.get())
    texture_layer_->ClearClient();
  // TODO(piman): delegated_renderer_layer_ cleanup.

  cc_layer_->RemoveAllChildren();
  if (cc_layer_->parent()) {
    cc_layer_->parent()->ReplaceChild(cc_layer_, new_layer);
  }
  cc_layer_->SetLayerClient(NULL);
  cc_layer_->RemoveLayerAnimationEventObserver(this);
  new_layer->SetOpacity(cc_layer_->opacity());
  new_layer->SetTransform(cc_layer_->transform());
  new_layer->SetPosition(cc_layer_->position());

  cc_layer_ = new_layer.get();
  content_layer_ = NULL;
  solid_color_layer_ = NULL;
  texture_layer_ = NULL;
  delegated_renderer_layer_ = NULL;

  cc_layer_->AddLayerAnimationEventObserver(this);
  for (size_t i = 0; i < children_.size(); ++i) {
    DCHECK(children_[i]->cc_layer_);
    cc_layer_->AddChild(children_[i]->cc_layer_);
  }
  cc_layer_->SetLayerClient(this);
  cc_layer_->SetTransformOrigin(gfx::Point3F());
  cc_layer_->SetContentsOpaque(fills_bounds_opaquely_);
  cc_layer_->SetForceRenderSurface(force_render_surface_);
  cc_layer_->SetIsDrawable(type_ != LAYER_NOT_DRAWN);
  cc_layer_->SetHideLayerAndSubtree(!visible_);
}

void Layer::SwitchCCLayerForTest() {
  scoped_refptr<cc::Layer> new_layer;
  if (Layer::UsingPictureLayer())
    new_layer = cc::PictureLayer::Create(this);
  else
    new_layer = cc::ContentLayer::Create(this);
  SwitchToLayer(new_layer);
  content_layer_ = new_layer;
}

void Layer::SetTextureMailbox(
    const cc::TextureMailbox& mailbox,
    scoped_ptr<cc::SingleReleaseCallback> release_callback,
    gfx::Size texture_size_in_dip) {
  DCHECK_EQ(type_, LAYER_TEXTURED);
  DCHECK(!solid_color_layer_.get());
  DCHECK(mailbox.IsValid());
  DCHECK(release_callback);
  if (!texture_layer_) {
    scoped_refptr<cc::TextureLayer> new_layer =
        cc::TextureLayer::CreateForMailbox(this);
    new_layer->SetFlipped(true);
    SwitchToLayer(new_layer);
    texture_layer_ = new_layer;
  }
  if (mailbox_release_callback_)
    mailbox_release_callback_->Run(0, false);
  mailbox_release_callback_ = release_callback.Pass();
  mailbox_ = mailbox;
  SetTextureSize(texture_size_in_dip);
}

void Layer::SetTextureSize(gfx::Size texture_size_in_dip) {
  DCHECK(texture_layer_.get());
  if (frame_size_in_dip_ == texture_size_in_dip)
    return;
  frame_size_in_dip_ = texture_size_in_dip;
  RecomputeDrawsContentAndUVRect();
  texture_layer_->SetNeedsDisplay();
}

void Layer::SetShowDelegatedContent(cc::DelegatedFrameProvider* frame_provider,
                                    gfx::Size frame_size_in_dip) {
  DCHECK_EQ(type_, LAYER_TEXTURED);

  scoped_refptr<cc::DelegatedRendererLayer> new_layer =
      cc::DelegatedRendererLayer::Create(frame_provider);
  SwitchToLayer(new_layer);
  delegated_renderer_layer_ = new_layer;

  frame_size_in_dip_ = frame_size_in_dip;
  RecomputeDrawsContentAndUVRect();
}

void Layer::SetShowPaintedContent() {
  if (content_layer_.get())
    return;

  scoped_refptr<cc::Layer> new_layer;
  if (Layer::UsingPictureLayer())
    new_layer = cc::PictureLayer::Create(this);
  else
    new_layer = cc::ContentLayer::Create(this);
  SwitchToLayer(new_layer);
  content_layer_ = new_layer;

  mailbox_ = cc::TextureMailbox();
  if (mailbox_release_callback_) {
    mailbox_release_callback_->Run(0, false);
    mailbox_release_callback_.reset();
  }
  RecomputeDrawsContentAndUVRect();
}

void Layer::SetColor(SkColor color) { GetAnimator()->SetColor(color); }

bool Layer::SchedulePaint(const gfx::Rect& invalid_rect) {
  if (type_ == LAYER_SOLID_COLOR || (!delegate_ && !mailbox_.IsValid()))
    return false;

  damaged_region_.op(invalid_rect.x(),
                     invalid_rect.y(),
                     invalid_rect.right(),
                     invalid_rect.bottom(),
                     SkRegion::kUnion_Op);
  ScheduleDraw();
  return true;
}

void Layer::ScheduleDraw() {
  Compositor* compositor = GetCompositor();
  if (compositor)
    compositor->ScheduleDraw();
}

void Layer::SendDamagedRects() {
  if ((delegate_ || mailbox_.IsValid()) && !damaged_region_.isEmpty()) {
    for (SkRegion::Iterator iter(damaged_region_); !iter.done(); iter.next()) {
      const SkIRect& sk_damaged = iter.rect();
      gfx::Rect damaged(
          sk_damaged.x(),
          sk_damaged.y(),
          sk_damaged.width(),
          sk_damaged.height());
      cc_layer_->SetNeedsDisplayRect(damaged);
    }
    damaged_region_.setEmpty();
  }
  for (size_t i = 0; i < children_.size(); ++i)
    children_[i]->SendDamagedRects();
}

void Layer::CompleteAllAnimations() {
  std::vector<scoped_refptr<LayerAnimator> > animators;
  CollectAnimators(&animators);
  std::for_each(animators.begin(), animators.end(),
                std::mem_fun(&LayerAnimator::StopAnimating));
}

void Layer::SuppressPaint() {
  if (!delegate_)
    return;
  delegate_ = NULL;
  for (size_t i = 0; i < children_.size(); ++i)
    children_[i]->SuppressPaint();
}

void Layer::OnDeviceScaleFactorChanged(float device_scale_factor) {
  if (device_scale_factor_ == device_scale_factor)
    return;
  if (animator_.get())
    animator_->StopAnimatingProperty(LayerAnimationElement::TRANSFORM);
  device_scale_factor_ = device_scale_factor;
  RecomputeDrawsContentAndUVRect();
  RecomputePosition();
  SchedulePaint(gfx::Rect(bounds_.size()));
  if (delegate_)
    delegate_->OnDeviceScaleFactorChanged(device_scale_factor);
  for (size_t i = 0; i < children_.size(); ++i)
    children_[i]->OnDeviceScaleFactorChanged(device_scale_factor);
  if (layer_mask_)
    layer_mask_->OnDeviceScaleFactorChanged(device_scale_factor);
}

void Layer::RequestCopyOfOutput(scoped_ptr<cc::CopyOutputRequest> request) {
  cc_layer_->RequestCopyOfOutput(request.Pass());
}

void Layer::PaintContents(SkCanvas* sk_canvas,
                          const gfx::Rect& clip,
                          gfx::RectF* opaque,
                          ContentLayerClient::GraphicsContextStatus gc_status) {
  TRACE_EVENT0("ui", "Layer::PaintContents");
  scoped_ptr<gfx::Canvas> canvas(gfx::Canvas::CreateCanvasWithoutScaling(
      sk_canvas, device_scale_factor_));
  if (delegate_)
    delegate_->OnPaintLayer(canvas.get());
}

bool Layer::FillsBoundsCompletely() const { return fills_bounds_completely_; }

bool Layer::PrepareTextureMailbox(
    cc::TextureMailbox* mailbox,
    scoped_ptr<cc::SingleReleaseCallback>* release_callback,
    bool use_shared_memory) {
  if (!mailbox_release_callback_)
    return false;
  *mailbox = mailbox_;
  *release_callback = mailbox_release_callback_.Pass();
  return true;
}

void Layer::SetForceRenderSurface(bool force) {
  if (force_render_surface_ == force)
    return;

  force_render_surface_ = force;
  cc_layer_->SetForceRenderSurface(force_render_surface_);
}

class LayerDebugInfo : public base::debug::ConvertableToTraceFormat {
 public:
  explicit LayerDebugInfo(std::string name) : name_(name) { }
  virtual void AppendAsTraceFormat(std::string* out) const OVERRIDE {
    base::DictionaryValue dictionary;
    dictionary.SetString("layer_name", name_);
    base::JSONWriter::Write(&dictionary, out);
  }

 private:
  virtual ~LayerDebugInfo() { }
  std::string name_;
};

scoped_refptr<base::debug::ConvertableToTraceFormat> Layer::TakeDebugInfo() {
  return new LayerDebugInfo(name_);
}

void Layer::OnAnimationStarted(const cc::AnimationEvent& event) {
  if (animator_.get())
    animator_->OnThreadedAnimationStarted(event);
}

void Layer::CollectAnimators(
    std::vector<scoped_refptr<LayerAnimator> >* animators) {
  if (IsAnimating())
    animators->push_back(animator_);
  std::for_each(children_.begin(), children_.end(),
                std::bind2nd(std::mem_fun(&Layer::CollectAnimators),
                             animators));
}

void Layer::StackRelativeTo(Layer* child, Layer* other, bool above) {
  DCHECK_NE(child, other);
  DCHECK_EQ(this, child->parent());
  DCHECK_EQ(this, other->parent());

  const size_t child_i =
      std::find(children_.begin(), children_.end(), child) - children_.begin();
  const size_t other_i =
      std::find(children_.begin(), children_.end(), other) - children_.begin();
  if ((above && child_i == other_i + 1) || (!above && child_i + 1 == other_i))
    return;

  const size_t dest_i =
      above ?
      (child_i < other_i ? other_i : other_i + 1) :
      (child_i < other_i ? other_i - 1 : other_i);
  children_.erase(children_.begin() + child_i);
  children_.insert(children_.begin() + dest_i, child);

  child->cc_layer_->RemoveFromParent();
  cc_layer_->InsertChild(child->cc_layer_, dest_i);
}

bool Layer::ConvertPointForAncestor(const Layer* ancestor,
                                    gfx::Point* point) const {
  gfx::Transform transform;
  bool result = GetTargetTransformRelativeTo(ancestor, &transform);
  gfx::Point3F p(*point);
  transform.TransformPoint(&p);
  *point = gfx::ToFlooredPoint(p.AsPointF());
  return result;
}

bool Layer::ConvertPointFromAncestor(const Layer* ancestor,
                                     gfx::Point* point) const {
  gfx::Transform transform;
  bool result = GetTargetTransformRelativeTo(ancestor, &transform);
  gfx::Point3F p(*point);
  transform.TransformPointReverse(&p);
  *point = gfx::ToFlooredPoint(p.AsPointF());
  return result;
}

void Layer::SetBoundsFromAnimation(const gfx::Rect& bounds) {
  if (bounds == bounds_)
    return;

  base::Closure closure;
  if (delegate_)
    closure = delegate_->PrepareForLayerBoundsChange();
  bool was_move = bounds_.size() == bounds.size();
  bounds_ = bounds;

  RecomputeDrawsContentAndUVRect();
  RecomputePosition();

  if (!closure.is_null())
    closure.Run();

  if (was_move) {
    // Don't schedule a draw if we're invisible. We'll schedule one
    // automatically when we get visible.
    if (IsDrawn())
      ScheduleDraw();
  } else {
    // Always schedule a paint, even if we're invisible.
    SchedulePaint(gfx::Rect(bounds.size()));
  }
}

void Layer::SetTransformFromAnimation(const gfx::Transform& transform) {
  cc_layer_->SetTransform(transform);
}

void Layer::SetOpacityFromAnimation(float opacity) {
  cc_layer_->SetOpacity(opacity);
  ScheduleDraw();
}

void Layer::SetVisibilityFromAnimation(bool visible) {
  if (visible_ == visible)
    return;

  visible_ = visible;
  cc_layer_->SetHideLayerAndSubtree(!visible_);
}

void Layer::SetBrightnessFromAnimation(float brightness) {
  layer_brightness_ = brightness;
  SetLayerFilters();
}

void Layer::SetGrayscaleFromAnimation(float grayscale) {
  layer_grayscale_ = grayscale;
  SetLayerFilters();
}

void Layer::SetColorFromAnimation(SkColor color) {
  DCHECK_EQ(type_, LAYER_SOLID_COLOR);
  solid_color_layer_->SetBackgroundColor(color);
  SetFillsBoundsOpaquely(SkColorGetA(color) == 0xFF);
}

void Layer::ScheduleDrawForAnimation() {
  ScheduleDraw();
}

const gfx::Rect& Layer::GetBoundsForAnimation() const {
  return bounds();
}

gfx::Transform Layer::GetTransformForAnimation() const {
  return transform();
}

float Layer::GetOpacityForAnimation() const {
  return opacity();
}

bool Layer::GetVisibilityForAnimation() const {
  return visible();
}

float Layer::GetBrightnessForAnimation() const {
  return layer_brightness();
}

float Layer::GetGrayscaleForAnimation() const {
  return layer_grayscale();
}

SkColor Layer::GetColorForAnimation() const {
  // WebColor is equivalent to SkColor, per WebColor.h.
  // The NULL check is here since this is invoked regardless of whether we have
  // been configured as LAYER_SOLID_COLOR.
  return solid_color_layer_.get() ?
      solid_color_layer_->background_color() : SK_ColorBLACK;
}

float Layer::GetDeviceScaleFactor() const {
  return device_scale_factor_;
}

void Layer::AddThreadedAnimation(scoped_ptr<cc::Animation> animation) {
  DCHECK(cc_layer_);
  // Until this layer has a compositor (and hence cc_layer_ has a
  // LayerTreeHost), addAnimation will fail.
  if (GetCompositor())
    cc_layer_->AddAnimation(animation.Pass());
  else
    pending_threaded_animations_.push_back(animation.Pass());
}

namespace{

struct HasAnimationId {
  HasAnimationId(int id): id_(id) {
  }

  bool operator()(cc::Animation* animation) const {
    return animation->id() == id_;
  }

 private:
  int id_;
};

}

void Layer::RemoveThreadedAnimation(int animation_id) {
  DCHECK(cc_layer_);
  if (pending_threaded_animations_.size() == 0) {
    cc_layer_->RemoveAnimation(animation_id);
    return;
  }

  pending_threaded_animations_.erase(
      cc::remove_if(&pending_threaded_animations_,
                    pending_threaded_animations_.begin(),
                    pending_threaded_animations_.end(),
                    HasAnimationId(animation_id)),
      pending_threaded_animations_.end());
}

LayerAnimatorCollection* Layer::GetLayerAnimatorCollection() {
  Compositor* compositor = GetCompositor();
  return compositor ? compositor->layer_animator_collection() : NULL;
}

void Layer::SendPendingThreadedAnimations() {
  for (cc::ScopedPtrVector<cc::Animation>::iterator it =
           pending_threaded_animations_.begin();
       it != pending_threaded_animations_.end();
       ++it)
    cc_layer_->AddAnimation(pending_threaded_animations_.take(it));

  pending_threaded_animations_.clear();

  for (size_t i = 0; i < children_.size(); ++i)
    children_[i]->SendPendingThreadedAnimations();
}

void Layer::CreateWebLayer() {
  if (type_ == LAYER_SOLID_COLOR) {
    solid_color_layer_ = cc::SolidColorLayer::Create();
    cc_layer_ = solid_color_layer_.get();
  } else {
    if (Layer::UsingPictureLayer())
      content_layer_ = cc::PictureLayer::Create(this);
    else
      content_layer_ = cc::ContentLayer::Create(this);
    cc_layer_ = content_layer_.get();
  }
  cc_layer_->SetTransformOrigin(gfx::Point3F());
  cc_layer_->SetContentsOpaque(true);
  cc_layer_->SetIsDrawable(type_ != LAYER_NOT_DRAWN);
  cc_layer_->AddLayerAnimationEventObserver(this);
  cc_layer_->SetLayerClient(this);
  RecomputePosition();
}

gfx::Transform Layer::transform() const {
  return cc_layer_->transform();
}

void Layer::RecomputeDrawsContentAndUVRect() {
  DCHECK(cc_layer_);
  gfx::Size size(bounds_.size());
  if (texture_layer_.get()) {
    size.SetToMin(frame_size_in_dip_);
    gfx::PointF uv_top_left(0.f, 0.f);
    gfx::PointF uv_bottom_right(
        static_cast<float>(size.width()) / frame_size_in_dip_.width(),
        static_cast<float>(size.height()) / frame_size_in_dip_.height());
    texture_layer_->SetUV(uv_top_left, uv_bottom_right);
  } else if (delegated_renderer_layer_.get()) {
    size.SetToMin(frame_size_in_dip_);
  }
  cc_layer_->SetBounds(size);
}

void Layer::RecomputePosition() {
  cc_layer_->SetPosition(bounds_.origin() + subpixel_position_offset_);
}

void Layer::AddAnimatorsInTreeToCollection(
    LayerAnimatorCollection* collection) {
  DCHECK(collection);
  if (IsAnimating())
    animator_->AddToCollection(collection);
  std::for_each(
      children_.begin(),
      children_.end(),
      std::bind2nd(std::mem_fun(&Layer::AddAnimatorsInTreeToCollection),
                   collection));
}

void Layer::RemoveAnimatorsInTreeFromCollection(
    LayerAnimatorCollection* collection) {
  DCHECK(collection);
  if (IsAnimating())
    animator_->RemoveFromCollection(collection);
  std::for_each(
      children_.begin(),
      children_.end(),
      std::bind2nd(std::mem_fun(&Layer::RemoveAnimatorsInTreeFromCollection),
                   collection));
}

bool Layer::IsAnimating() const {
  return animator_ && animator_->is_animating();
}

}  // namespace ui
