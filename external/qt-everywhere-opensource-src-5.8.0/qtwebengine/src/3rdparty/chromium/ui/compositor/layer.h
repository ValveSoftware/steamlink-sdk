// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_LAYER_H_
#define UI_COMPOSITOR_LAYER_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "cc/base/region.h"
#include "cc/layers/content_layer_client.h"
#include "cc/layers/layer_client.h"
#include "cc/layers/surface_layer.h"
#include "cc/layers/texture_layer_client.h"
#include "cc/resources/texture_mailbox.h"
#include "cc/surfaces/surface_id.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer_animation_delegate.h"
#include "ui/compositor/layer_delegate.h"
#include "ui/compositor/layer_type.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/transform.h"

class SkCanvas;

namespace cc {
class ContentLayer;
class CopyOutputRequest;
class Layer;
class NinePatchLayer;
class ResourceUpdateQueue;
class SolidColorLayer;
class SurfaceLayer;
class TextureLayer;
struct ReturnedResource;
typedef std::vector<ReturnedResource> ReturnedResourceArray;
}

namespace ui {

class Compositor;
class LayerAnimator;
class LayerOwner;
class LayerThreadedAnimationDelegate;

// Layer manages a texture, transform and a set of child Layers. Any View that
// has enabled layers ends up creating a Layer to manage the texture.
// A Layer can also be created without a texture, in which case it renders
// nothing and is simply used as a node in a hierarchy of layers.
// Coordinate system used in layers is DIP (Density Independent Pixel)
// coordinates unless explicitly mentioned as pixel coordinates.
//
// NOTE: Unlike Views, each Layer does *not* own its child Layers. If you
// delete a Layer and it has children, the parent of each child Layer is set to
// NULL, but the children are not deleted.
class COMPOSITOR_EXPORT Layer
    : public LayerAnimationDelegate,
      NON_EXPORTED_BASE(public cc::ContentLayerClient),
      NON_EXPORTED_BASE(public cc::TextureLayerClient),
      NON_EXPORTED_BASE(public cc::LayerClient) {
 public:
  Layer();
  explicit Layer(LayerType type);
  ~Layer() override;

  // Retrieves the Layer's compositor. The Layer will walk up its parent chain
  // to locate it. Returns NULL if the Layer is not attached to a compositor.
  Compositor* GetCompositor() {
    return const_cast<Compositor*>(
        const_cast<const Layer*>(this)->GetCompositor());
  }
  const Compositor* GetCompositor() const;

  // Called by the compositor when the Layer is set as its root Layer. This can
  // only ever be called on the root layer.
  void SetCompositor(Compositor* compositor,
                     scoped_refptr<cc::Layer> root_layer);
  void ResetCompositor();

  LayerDelegate* delegate() { return delegate_; }
  void set_delegate(LayerDelegate* delegate) { delegate_ = delegate; }

  LayerOwner* owner() { return owner_; }

  // Adds a new Layer to this Layer.
  void Add(Layer* child);

  // Removes a Layer from this Layer.
  void Remove(Layer* child);

  // Stacks |child| above all other children.
  void StackAtTop(Layer* child);

  // Stacks |child| directly above |other|.  Both must be children of this
  // layer.  Note that if |child| is initially stacked even higher, calling this
  // method will result in |child| being lowered in the stacking order.
  void StackAbove(Layer* child, Layer* other);

  // Stacks |child| below all other children.
  void StackAtBottom(Layer* child);

  // Stacks |child| directly below |other|.  Both must be children of this
  // layer.
  void StackBelow(Layer* child, Layer* other);

  // Returns the child Layers.
  const std::vector<Layer*>& children() const { return children_; }

  // The parent.
  const Layer* parent() const { return parent_; }
  Layer* parent() { return parent_; }

  LayerType type() const { return type_; }

  // Returns true if this Layer contains |other| somewhere in its children.
  bool Contains(const Layer* other) const;

  // The layer's animator is responsible for causing automatic animations when
  // properties are set. It also manages a queue of pending animations and
  // handles blending of animations. The layer takes ownership of the animator.
  void SetAnimator(LayerAnimator* animator);

  // Returns the layer's animator. Creates a default animator of one has not
  // been set. Will not return NULL.
  LayerAnimator* GetAnimator();

  // The transform, relative to the parent.
  void SetTransform(const gfx::Transform& transform);
  gfx::Transform transform() const;

  gfx::PointF position() const { return cc_layer_->position(); }

  // Return the target transform if animator is running, or the current
  // transform otherwise.
  gfx::Transform GetTargetTransform() const;

  // The bounds, relative to the parent.
  void SetBounds(const gfx::Rect& bounds);
  const gfx::Rect& bounds() const { return bounds_; }
  const gfx::Size& size() const { return bounds_.size(); }

  // The offset from our parent (stored in bounds.origin()) is an integer but we
  // may need to be at a fractional pixel offset to align properly on screen.
  void SetSubpixelPositionOffset(const gfx::Vector2dF& offset);
  const gfx::Vector2dF& subpixel_position_offset() const {
    return subpixel_position_offset_;
  }

  // Return the target bounds if animator is running, or the current bounds
  // otherwise.
  gfx::Rect GetTargetBounds() const;

  // Sets/gets whether or not drawing of child layers should be clipped to the
  // bounds of this layer.
  void SetMasksToBounds(bool masks_to_bounds);
  bool GetMasksToBounds() const;

  // The opacity of the layer. The opacity is applied to each pixel of the
  // texture (resulting alpha = opacity * alpha).
  float opacity() const;
  void SetOpacity(float opacity);

  // Returns the actual opacity, which the opacity of this layer multipled by
  // the combined opacity of the parent.
  float GetCombinedOpacity() const;

  // Blur pixels by this amount in anything below the layer and visible through
  // the layer.
  int background_blur() const { return background_blur_radius_; }
  void SetBackgroundBlur(int blur_radius);

  // Saturate all pixels of this layer by this amount.
  // This effect will get "combined" with the inverted,
  // brightness and grayscale setting.
  float layer_saturation() const { return layer_saturation_; }
  void SetLayerSaturation(float saturation);

  // Change the brightness of all pixels from this layer by this amount.
  // This effect will get "combined" with the inverted, saturate
  // and grayscale setting.
  float layer_brightness() const { return layer_brightness_; }
  void SetLayerBrightness(float brightness);

  // Return the target brightness if animator is running, or the current
  // brightness otherwise.
  float GetTargetBrightness() const;

  // Change the grayscale of all pixels from this layer by this amount.
  // This effect will get "combined" with the inverted, saturate
  // and brightness setting.
  float layer_grayscale() const { return layer_grayscale_; }
  void SetLayerGrayscale(float grayscale);

  // Return the target grayscale if animator is running, or the current
  // grayscale otherwise.
  float GetTargetGrayscale() const;

  // Zoom the background by a factor of |zoom|. The effect is blended along the
  // edge across |inset| pixels.
  void SetBackgroundZoom(float zoom, int inset);

  // Set the shape of this layer.
  SkRegion* alpha_shape() const { return alpha_shape_.get(); }
  void SetAlphaShape(std::unique_ptr<SkRegion> region);

  // Invert the layer.
  bool layer_inverted() const { return layer_inverted_; }
  void SetLayerInverted(bool inverted);

  // Return the target opacity if animator is running, or the current opacity
  // otherwise.
  float GetTargetOpacity() const;

  // Set a layer mask for a layer.
  // Note the provided layer mask can neither have a layer mask itself nor can
  // it have any children. The ownership of |layer_mask| will not be
  // transferred with this call.
  // Furthermore: A mask layer can only be set to one layer.
  void SetMaskLayer(Layer* layer_mask);
  Layer* layer_mask_layer() { return layer_mask_; }

  // Sets the visibility of the Layer. A Layer may be visible but not
  // drawn. This happens if any ancestor of a Layer is not visible.
  void SetVisible(bool visible);
  bool visible() const { return visible_; }

  // Returns the target visibility if the animator is running. Otherwise, it
  // returns the current visibility.
  bool GetTargetVisibility() const;

  // Returns true if this Layer is drawn. A Layer is drawn only if all ancestors
  // are visible.
  bool IsDrawn() const;

  // Returns true if this layer can have a texture (has_texture_ is true)
  // and is not completely obscured by a child.
  bool ShouldDraw() const;

  // Converts a point from the coordinates of |source| to the coordinates of
  // |target|. Necessarily, |source| and |target| must inhabit the same Layer
  // tree.
  static void ConvertPointToLayer(const Layer* source,
                                  const Layer* target,
                                  gfx::Point* point);

  // Converts a transform to be relative to the given |ancestor|. Returns
  // whether success (that is, whether the given ancestor was really an
  // ancestor of this layer).
  bool GetTargetTransformRelativeTo(const Layer* ancestor,
                                    gfx::Transform* transform) const;

  // See description in View for details
  void SetFillsBoundsOpaquely(bool fills_bounds_opaquely);
  bool fills_bounds_opaquely() const { return fills_bounds_opaquely_; }

  // Set to true if this layer always paints completely within its bounds. If so
  // we can omit an unnecessary clear, even if the layer is transparent.
  void SetFillsBoundsCompletely(bool fills_bounds_completely);

  const std::string& name() const { return name_; }
  void set_name(const std::string& name) { name_ = name; }

  // Set new TextureMailbox for this layer. Note that |mailbox| may hold a
  // shared memory resource or an actual mailbox for a texture.
  void SetTextureMailbox(
      const cc::TextureMailbox& mailbox,
      std::unique_ptr<cc::SingleReleaseCallback> release_callback,
      gfx::Size texture_size_in_dip);
  void SetTextureSize(gfx::Size texture_size_in_dip);
  void SetTextureFlipped(bool flipped);
  bool TextureFlipped() const;

  // The alpha value applied to the whole texture. The effective value of each
  // pixel is computed as:
  // pixel.a = pixel.a * alpha.
  // Note: This is different from SetOpacity() as it only applies to the
  // texture and child layers are unaffected.
  // TODO(reveman): Remove once components/exo code is using SetShowSurface.
  // crbug.com/610086
  void SetTextureAlpha(float alpha);

  // The texture crop rectangle to be used. Empty rectangle means no cropping.
  void SetTextureCrop(const gfx::RectF& crop);

  // The texture scale to be used. Defaults to no scaling.
  void SetTextureScale(float x_scale, float y_scale);

  // Begins showing content from a surface with a particular id.
  void SetShowSurface(cc::SurfaceId surface_id,
                      const cc::SurfaceLayer::SatisfyCallback& satisfy_callback,
                      const cc::SurfaceLayer::RequireCallback& require_callback,
                      gfx::Size surface_size,
                      float scale,
                      gfx::Size frame_size_in_dip);

  bool has_external_content() {
    return texture_layer_.get() || surface_layer_.get();
  }

  // Show a solid color instead of delegated or surface contents.
  void SetShowSolidColorContent();

  // Sets the layer's fill color.  May only be called for LAYER_SOLID_COLOR.
  void SetColor(SkColor color);
  SkColor GetTargetColor();
  SkColor background_color() const;

  // Updates the nine patch layer's image, aperture and border. May only be
  // called for LAYER_NINE_PATCH.
  void UpdateNinePatchLayerImage(const gfx::ImageSkia& image);
  void UpdateNinePatchLayerAperture(const gfx::Rect& aperture_in_dip);
  void UpdateNinePatchLayerBorder(const gfx::Rect& border);

  // Adds |invalid_rect| to the Layer's pending invalid rect and calls
  // ScheduleDraw(). Returns false if the paint request is ignored.
  bool SchedulePaint(const gfx::Rect& invalid_rect);

  // Schedules a redraw of the layer tree at the compositor.
  // Note that this _does not_ invalidate any region of this layer; use
  // SchedulePaint() for that.
  void ScheduleDraw();

  // Uses damaged rectangles recorded in |damaged_region_| to invalidate the
  // |cc_layer_|.
  void SendDamagedRects();

  const cc::Region& damaged_region() const { return damaged_region_; }

  void CompleteAllAnimations();

  // Suppresses painting the content by disconnecting |delegate_|.
  void SuppressPaint();

  // Notifies the layer that the device scale factor has changed.
  void OnDeviceScaleFactorChanged(float device_scale_factor);

  // Notifies the layer that one of its children has received a new
  // delegated frame.
  void OnDelegatedFrameDamage(const gfx::Rect& damage_rect_in_dip);

  // Requets a copy of the layer's output as a texture or bitmap.
  void RequestCopyOfOutput(std::unique_ptr<cc::CopyOutputRequest> request);

  // ContentLayerClient
  gfx::Rect PaintableRegion() override;
  scoped_refptr<cc::DisplayItemList> PaintContentsToDisplayList(
      ContentLayerClient::PaintingControlSetting painting_control) override;
  bool FillsBoundsCompletely() const override;
  size_t GetApproximateUnsharedMemoryUsage() const override;

  cc::Layer* cc_layer_for_testing() { return cc_layer_; }

  // TextureLayerClient
  bool PrepareTextureMailbox(
      cc::TextureMailbox* mailbox,
      std::unique_ptr<cc::SingleReleaseCallback>* release_callback,
      bool use_shared_memory) override;

  float device_scale_factor() const { return device_scale_factor_; }

  // LayerClient
  std::unique_ptr<base::trace_event::ConvertableToTraceFormat> TakeDebugInfo(
      cc::Layer* layer) override;

  // Whether this layer has animations waiting to get sent to its cc::Layer.
  bool HasPendingThreadedAnimationsForTesting() const;

  // Triggers a call to SwitchToLayer.
  void SwitchCCLayerForTest();

  const cc::Region& damaged_region_for_testing() const {
    return damaged_region_;
  }

 private:
  friend class LayerOwner;

  void CollectAnimators(std::vector<scoped_refptr<LayerAnimator> >* animators);

  // Stacks |child| above or below |other|.  Helper method for StackAbove() and
  // StackBelow().
  void StackRelativeTo(Layer* child, Layer* other, bool above);

  bool ConvertPointForAncestor(const Layer* ancestor, gfx::Point* point) const;
  bool ConvertPointFromAncestor(const Layer* ancestor, gfx::Point* point) const;

  // Implementation of LayerAnimatorDelegate
  void SetBoundsFromAnimation(const gfx::Rect& bounds) override;
  void SetTransformFromAnimation(const gfx::Transform& transform) override;
  void SetOpacityFromAnimation(float opacity) override;
  void SetVisibilityFromAnimation(bool visibility) override;
  void SetBrightnessFromAnimation(float brightness) override;
  void SetGrayscaleFromAnimation(float grayscale) override;
  void SetColorFromAnimation(SkColor color) override;
  void ScheduleDrawForAnimation() override;
  const gfx::Rect& GetBoundsForAnimation() const override;
  gfx::Transform GetTransformForAnimation() const override;
  float GetOpacityForAnimation() const override;
  bool GetVisibilityForAnimation() const override;
  float GetBrightnessForAnimation() const override;
  float GetGrayscaleForAnimation() const override;
  SkColor GetColorForAnimation() const override;
  float GetDeviceScaleFactor() const override;
  cc::Layer* GetCcLayer() const override;
  LayerThreadedAnimationDelegate* GetThreadedAnimationDelegate() override;
  LayerAnimatorCollection* GetLayerAnimatorCollection() override;

  // Creates a corresponding composited layer for |type_|.
  void CreateCcLayer();

  // Recomputes and sets to |cc_layer_|.
  void RecomputeDrawsContentAndUVRect();
  void RecomputePosition();

  // Set all filters which got applied to the layer.
  void SetLayerFilters();

  // Set all filters which got applied to the layer background.
  void SetLayerBackgroundFilters();

  // Cleanup |cc_layer_| and replaces it with |new_layer|.
  void SwitchToLayer(scoped_refptr<cc::Layer> new_layer);

  void SetCompositorForAnimatorsInTree(Compositor* compositor);
  void ResetCompositorForAnimatorsInTree(Compositor* compositor);

  const LayerType type_;

  Compositor* compositor_;

  Layer* parent_;

  // This layer's children, in bottom-to-top stacking order.
  std::vector<Layer*> children_;

  gfx::Rect bounds_;
  gfx::Vector2dF subpixel_position_offset_;

  // Visibility of this layer. See SetVisible/IsDrawn for more details.
  bool visible_;

  bool fills_bounds_opaquely_;
  bool fills_bounds_completely_;

  // Union of damaged rects, in layer space, that SetNeedsDisplayRect should
  // be called on.
  cc::Region damaged_region_;

  // Union of damaged rects, in layer space, to be used when compositor is ready
  // to paint the content.
  cc::Region paint_region_;

  int background_blur_radius_;

  // Several variables which will change the visible representation of
  // the layer.
  float layer_saturation_;
  float layer_brightness_;
  float layer_grayscale_;
  bool layer_inverted_;

  // The associated mask layer with this layer.
  Layer* layer_mask_;
  // The back link from the mask layer to it's associated masked layer.
  // We keep this reference for the case that if the mask layer gets deleted
  // while attached to the main layer before the main layer is deleted.
  Layer* layer_mask_back_link_;

  // The zoom factor to scale the layer by.  Zooming is disabled when this is
  // set to 1.
  float zoom_;

  // Width of the border in pixels, where the scaling is blended.
  int zoom_inset_;

  // Shape of the window.
  std::unique_ptr<SkRegion> alpha_shape_;

  std::string name_;

  LayerDelegate* delegate_;

  LayerOwner* owner_;

  scoped_refptr<LayerAnimator> animator_;

  // Ownership of the layer is held through one of the strongly typed layer
  // pointers, depending on which sort of layer this is.
  scoped_refptr<cc::Layer> content_layer_;
  scoped_refptr<cc::NinePatchLayer> nine_patch_layer_;
  scoped_refptr<cc::TextureLayer> texture_layer_;
  scoped_refptr<cc::SolidColorLayer> solid_color_layer_;
  scoped_refptr<cc::SurfaceLayer> surface_layer_;
  cc::Layer* cc_layer_;

  // A cached copy of |Compositor::device_scale_factor()|.
  float device_scale_factor_;

  // A cached copy of the nine patch layer's image and aperture.
  // These are required for device scale factor change.
  gfx::ImageSkia nine_patch_layer_image_;
  gfx::Rect nine_patch_layer_aperture_;

  // The mailbox used by texture_layer_.
  cc::TextureMailbox mailbox_;

  // The callback to release the mailbox. This is only set after
  // SetTextureMailbox is called, before we give it to the TextureLayer.
  std::unique_ptr<cc::SingleReleaseCallback> mailbox_release_callback_;

  // The size of the frame or texture in DIP, set when SetShowDelegatedContent
  // or SetTextureMailbox was called.
  gfx::Size frame_size_in_dip_;

  // The texture crop rectangle.
  gfx::RectF texture_crop_;

  // The texture scale.
  float texture_x_scale_;
  float texture_y_scale_;

  DISALLOW_COPY_AND_ASSIGN(Layer);
};

}  // namespace ui

#endif  // UI_COMPOSITOR_LAYER_H_
