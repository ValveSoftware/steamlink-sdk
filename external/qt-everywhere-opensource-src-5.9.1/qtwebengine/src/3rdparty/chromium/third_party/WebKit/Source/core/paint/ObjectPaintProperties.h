// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ObjectPaintProperties_h
#define ObjectPaintProperties_h

#include "core/CoreExport.h"
#include "platform/geometry/LayoutPoint.h"
#include "platform/graphics/paint/ClipPaintPropertyNode.h"
#include "platform/graphics/paint/EffectPaintPropertyNode.h"
#include "platform/graphics/paint/PaintChunkProperties.h"
#include "platform/graphics/paint/PropertyTreeState.h"
#include "platform/graphics/paint/ScrollPaintPropertyNode.h"
#include "platform/graphics/paint/TransformPaintPropertyNode.h"
#include "wtf/PassRefPtr.h"
#include "wtf/PtrUtil.h"
#include "wtf/RefPtr.h"
#include <memory>

namespace blink {

// This class stores property tree related information associated with a
// LayoutObject.
// Currently there are two groups of information:
// 1. The set of property nodes created locally by this LayoutObject.
// 2. The set of property nodes (inherited, or created locally) and paint offset
//    that can be used to paint the border box of this LayoutObject (see:
//    localBorderBoxProperties).
class CORE_EXPORT ObjectPaintProperties {
  WTF_MAKE_NONCOPYABLE(ObjectPaintProperties);
  USING_FAST_MALLOC(ObjectPaintProperties);

 public:
  static std::unique_ptr<ObjectPaintProperties> create() {
    return wrapUnique(new ObjectPaintProperties());
  }

  // The hierarchy of the transform subtree created by a LayoutObject is as
  // follows:
  // [ paintOffsetTranslation ]           Normally paint offset is accumulated
  // |                                    without creating a node until we see,
  // |                                    for example, transform or
  // |                                    position:fixed.
  // +---[ transform ]                    The space created by CSS transform.
  //     |                                This is the local border box space,
  //     |                                see: localBorderBoxProperties below.
  //     +---[ perspective ]              The space created by CSS perspective.
  //     |   +---[ svgLocalToBorderBoxTransform ] Additional transform for
  //                                      children of the outermost root SVG.
  //     |              OR                (SVG does not support scrolling.)
  //     |   +---[ scrollTranslation ]    The space created by overflow clip.
  //     +---[ scrollbarPaintOffset ]     TODO(trchen): Remove this once we bake
  //                                      the paint offset into frameRect.  This
  //                                      is equivalent to the local border box
  //                                      space above, with pixel snapped paint
  //                                      offset baked in. It is really
  //                                      redundant, but it is a pain to teach
  //                                      scrollbars to paint with an offset.
  const TransformPaintPropertyNode* paintOffsetTranslation() const {
    return m_paintOffsetTranslation.get();
  }
  const TransformPaintPropertyNode* transform() const {
    return m_transform.get();
  }
  const TransformPaintPropertyNode* perspective() const {
    return m_perspective.get();
  }
  const TransformPaintPropertyNode* svgLocalToBorderBoxTransform() const {
    return m_svgLocalToBorderBoxTransform.get();
  }
  const TransformPaintPropertyNode* scrollTranslation() const {
    return m_scrollTranslation.get();
  }
  const TransformPaintPropertyNode* scrollbarPaintOffset() const {
    return m_scrollbarPaintOffset.get();
  }

  // Auxiliary scrolling information. Includes information such as the hierarchy
  // of scrollable areas, the extent that can be scrolled, etc. The actual
  // scroll offset is stored in the transform tree (m_scrollTranslation).
  const ScrollPaintPropertyNode* scroll() const { return m_scroll.get(); }

  const EffectPaintPropertyNode* effect() const { return m_effect.get(); }

  // The hierarchy of the clip subtree created by a LayoutObject is as follows:
  // [ css clip ]
  // [ css clip fixed position]
  // [ inner border radius clip ] Clip created by a rounded border with overflow
  //                              clip. This clip is not inset by scrollbars.
  // +--- [ overflow clip ]       Clip created by overflow clip and is inset by
  //                              the scrollbars.
  const ClipPaintPropertyNode* cssClip() const { return m_cssClip.get(); }
  const ClipPaintPropertyNode* cssClipFixedPosition() const {
    return m_cssClipFixedPosition.get();
  }
  const ClipPaintPropertyNode* innerBorderRadiusClip() const {
    return m_innerBorderRadiusClip.get();
  }
  const ClipPaintPropertyNode* overflowClip() const {
    return m_overflowClip.get();
  }

  // The complete set of property tree nodes (inherited, or created locally) and
  // paint offset that can be used to paint. |paintOffset| is relative to the
  // propertyTreeState's transform space.
  // See: localBorderBoxProperties and contentsProperties.
  struct PropertyTreeStateWithOffset {
    PropertyTreeStateWithOffset(LayoutPoint offset, PropertyTreeState treeState)
        : paintOffset(offset), propertyTreeState(treeState) {}
    LayoutPoint paintOffset;
    PropertyTreeState propertyTreeState;
  };

  // This is a complete set of property nodes and paint offset that should be
  // used as a starting point to paint this layout object. This is cached
  // because some properties inherit from the containing block chain instead of
  // the painting parent and cannot be derived in O(1) during the paint walk.
  // For example, <div style='opacity: 0.3; position: relative; margin: 11px;'/>
  // would have a paint offset of (11px, 11px) and propertyTreeState.effect()
  // would be an effect node with opacity of 0.3 which was created by the div
  // itself. Note that propertyTreeState.transform() would not be null but would
  // instead point to the transform space setup by div's ancestors.
  const PropertyTreeStateWithOffset* localBorderBoxProperties() const {
    return m_localBorderBoxProperties.get();
  }
  void setLocalBorderBoxProperties(
      std::unique_ptr<PropertyTreeStateWithOffset> properties) {
    m_localBorderBoxProperties = std::move(properties);
  }

  // This is the complete set of property nodes and paint offset that can be
  // used to paint the contents of this object. It is similar to
  // localBorderBoxProperties but includes properties (e.g., overflow clip,
  // scroll translation) that apply to contents. This is suitable for paint
  // invalidation.
  ObjectPaintProperties::PropertyTreeStateWithOffset contentsProperties() const;

  void clearPaintOffsetTranslation() { m_paintOffsetTranslation = nullptr; }
  void clearTransform() { m_transform = nullptr; }
  void clearEffect() { m_effect = nullptr; }
  void clearCssClip() { m_cssClip = nullptr; }
  void clearCssClipFixedPosition() { m_cssClipFixedPosition = nullptr; }
  void clearInnerBorderRadiusClip() { m_innerBorderRadiusClip = nullptr; }
  void clearOverflowClip() { m_overflowClip = nullptr; }
  void clearLocalBorderBoxProperties() { m_localBorderBoxProperties = nullptr; }
  void clearPerspective() { m_perspective = nullptr; }
  void clearSvgLocalToBorderBoxTransform() {
    m_svgLocalToBorderBoxTransform = nullptr;
  }
  void clearScrollTranslation() { m_scrollTranslation = nullptr; }
  void clearScrollbarPaintOffset() { m_scrollbarPaintOffset = nullptr; }
  void clearScroll() { m_scroll = nullptr; }

  template <typename... Args>
  void updatePaintOffsetTranslation(Args&&... args) {
    updateProperty(m_paintOffsetTranslation, std::forward<Args>(args)...);
  }
  template <typename... Args>
  void updateTransform(Args&&... args) {
    updateProperty(m_transform, std::forward<Args>(args)...);
  }
  template <typename... Args>
  void updatePerspective(Args&&... args) {
    updateProperty(m_perspective, std::forward<Args>(args)...);
  }
  template <typename... Args>
  void updateSvgLocalToBorderBoxTransform(Args&&... args) {
    DCHECK(!scrollTranslation()) << "SVG elements cannot scroll so there "
                                    "should never be both a scroll translation "
                                    "and an SVG local to border box transform.";
    updateProperty(m_svgLocalToBorderBoxTransform, std::forward<Args>(args)...);
  }
  template <typename... Args>
  void updateScrollTranslation(Args&&... args) {
    DCHECK(!svgLocalToBorderBoxTransform())
        << "SVG elements cannot scroll so there should never be both a scroll "
           "translation and an SVG local to border box transform.";
    updateProperty(m_scrollTranslation, std::forward<Args>(args)...);
  }
  template <typename... Args>
  void updateScrollbarPaintOffset(Args&&... args) {
    updateProperty(m_scrollbarPaintOffset, std::forward<Args>(args)...);
  }
  template <typename... Args>
  void updateScroll(Args&&... args) {
    updateProperty(m_scroll, std::forward<Args>(args)...);
  }
  template <typename... Args>
  void updateEffect(Args&&... args) {
    updateProperty(m_effect, std::forward<Args>(args)...);
  }
  template <typename... Args>
  void updateCssClip(Args&&... args) {
    updateProperty(m_cssClip, std::forward<Args>(args)...);
  }
  template <typename... Args>
  void updateCssClipFixedPosition(Args&&... args) {
    updateProperty(m_cssClipFixedPosition, std::forward<Args>(args)...);
  }
  template <typename... Args>
  void updateInnerBorderRadiusClip(Args&&... args) {
    updateProperty(m_innerBorderRadiusClip, std::forward<Args>(args)...);
  }
  template <typename... Args>
  void updateOverflowClip(Args&&... args) {
    updateProperty(m_overflowClip, std::forward<Args>(args)...);
  }

#if DCHECK_IS_ON()
  // Used by FindPropertiesNeedingUpdate.h for recording the current properties.
  std::unique_ptr<ObjectPaintProperties> clone() const {
    std::unique_ptr<ObjectPaintProperties> cloned = create();
    if (m_paintOffsetTranslation)
      cloned->m_paintOffsetTranslation = m_paintOffsetTranslation->clone();
    if (m_transform)
      cloned->m_transform = m_transform->clone();
    if (m_effect)
      cloned->m_effect = m_effect->clone();
    if (m_cssClip)
      cloned->m_cssClip = m_cssClip->clone();
    if (m_cssClipFixedPosition)
      cloned->m_cssClipFixedPosition = m_cssClipFixedPosition->clone();
    if (m_innerBorderRadiusClip)
      cloned->m_innerBorderRadiusClip = m_innerBorderRadiusClip->clone();
    if (m_overflowClip)
      cloned->m_overflowClip = m_overflowClip->clone();
    if (m_perspective)
      cloned->m_perspective = m_perspective->clone();
    if (m_svgLocalToBorderBoxTransform) {
      cloned->m_svgLocalToBorderBoxTransform =
          m_svgLocalToBorderBoxTransform->clone();
    }
    if (m_scrollTranslation)
      cloned->m_scrollTranslation = m_scrollTranslation->clone();
    if (m_scrollbarPaintOffset)
      cloned->m_scrollbarPaintOffset = m_scrollbarPaintOffset->clone();
    if (m_scroll)
      cloned->m_scroll = m_scroll->clone();
    if (m_localBorderBoxProperties) {
      auto& state = m_localBorderBoxProperties->propertyTreeState;
      cloned->m_localBorderBoxProperties =
          wrapUnique(new PropertyTreeStateWithOffset(
              m_localBorderBoxProperties->paintOffset,
              PropertyTreeState(state.transform(), state.clip(), state.effect(),
                                state.scroll())));
    }
    return cloned;
  }
#endif

 private:
  ObjectPaintProperties() {}

  template <typename PaintPropertyNode, typename... Args>
  void updateProperty(RefPtr<PaintPropertyNode>& field, Args&&... args) {
    if (field)
      field->update(std::forward<Args>(args)...);
    else
      field = PaintPropertyNode::create(std::forward<Args>(args)...);
  }

  RefPtr<TransformPaintPropertyNode> m_paintOffsetTranslation;
  RefPtr<TransformPaintPropertyNode> m_transform;
  RefPtr<EffectPaintPropertyNode> m_effect;
  RefPtr<ClipPaintPropertyNode> m_cssClip;
  RefPtr<ClipPaintPropertyNode> m_cssClipFixedPosition;
  RefPtr<ClipPaintPropertyNode> m_innerBorderRadiusClip;
  RefPtr<ClipPaintPropertyNode> m_overflowClip;
  RefPtr<TransformPaintPropertyNode> m_perspective;
  // TODO(pdr): Only LayoutSVGRoot needs this and it should be moved there.
  RefPtr<TransformPaintPropertyNode> m_svgLocalToBorderBoxTransform;
  RefPtr<TransformPaintPropertyNode> m_scrollTranslation;
  RefPtr<TransformPaintPropertyNode> m_scrollbarPaintOffset;
  RefPtr<ScrollPaintPropertyNode> m_scroll;

  std::unique_ptr<PropertyTreeStateWithOffset> m_localBorderBoxProperties;
};

}  // namespace blink

#endif  // ObjectPaintProperties_h
