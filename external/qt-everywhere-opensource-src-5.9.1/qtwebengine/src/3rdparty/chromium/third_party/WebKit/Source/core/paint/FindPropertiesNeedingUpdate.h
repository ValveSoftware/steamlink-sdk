// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FindPropertiesNeedingUpdate_h
#define FindPropertiesNeedingUpdate_h

#if DCHECK_IS_ON()
namespace blink {

#define DCHECK_PTR_VAL_EQ(ptrA, ptrB) \
  DCHECK((!!ptrA == !!ptrB) && ((!ptrA && !ptrB) || (*ptrA == *ptrB)));

// This file contains two scope classes for catching cases where paint
// properties need an update but where not marked as such. If paint properties
// will change, the object must be marked as needing a paint property update
// using {FrameView, LayoutObject}::setNeedsPaintPropertyUpdate().
//
// Both scope classes work by recording the paint property state of an object
// before rebuilding properties, forcing the properties to get updated, then
// checking that the updated properties match the original properties.

class FindFrameViewPropertiesNeedingUpdateScope {
 public:
  FindFrameViewPropertiesNeedingUpdateScope(FrameView* frameView)
      : m_frameView(frameView),
        m_neededPaintPropertyUpdate(frameView->needsPaintPropertyUpdate()) {
    // No need to check when already marked as needing an update.
    if (m_neededPaintPropertyUpdate)
      return;

    // Mark the properties as needing an update to ensure they are rebuilt.
    m_frameView->setNeedsPaintPropertyUpdate();
    if (auto* preTranslation = m_frameView->preTranslation())
      m_preTranslation = preTranslation->clone();
    if (auto* contentClip = m_frameView->contentClip())
      m_contentClip = contentClip->clone();
    if (auto* scrollTranslation = m_frameView->scrollTranslation())
      m_scrollTranslation = scrollTranslation->clone();
    if (auto* scroll = m_frameView->scroll())
      m_scroll = scroll->clone();
  }

  ~FindFrameViewPropertiesNeedingUpdateScope() {
    // No need to check when already marked as needing an update.
    if (m_neededPaintPropertyUpdate)
      return;

    // If paint properties are not marked as needing an update but still change,
    // we are missing a call to FrameView::setNeedsPaintPropertyUpdate().
    DCHECK_PTR_VAL_EQ(m_preTranslation, m_frameView->preTranslation());
    DCHECK_PTR_VAL_EQ(m_contentClip, m_frameView->contentClip());
    DCHECK_PTR_VAL_EQ(m_scrollTranslation, m_frameView->scrollTranslation());
    DCHECK_PTR_VAL_EQ(m_scroll, m_frameView->scroll());
    // Restore original clean bit.
    m_frameView->clearNeedsPaintPropertyUpdate();
  }

 private:
  Persistent<FrameView> m_frameView;
  bool m_neededPaintPropertyUpdate;
  RefPtr<TransformPaintPropertyNode> m_preTranslation;
  RefPtr<ClipPaintPropertyNode> m_contentClip;
  RefPtr<TransformPaintPropertyNode> m_scrollTranslation;
  RefPtr<ScrollPaintPropertyNode> m_scroll;
};

class FindObjectPropertiesNeedingUpdateScope {
 public:
  FindObjectPropertiesNeedingUpdateScope(const LayoutObject& object)
      : m_object(object),
        m_neededPaintPropertyUpdate(object.needsPaintPropertyUpdate()) {
    // No need to check when already marked as needing an update.
    if (m_neededPaintPropertyUpdate)
      return;

    // Mark the properties as needing an update to ensure they are rebuilt.
    const_cast<LayoutObject&>(m_object).setNeedsPaintPropertyUpdate();
    if (const auto* properties = m_object.paintProperties())
      m_properties = properties->clone();
  }

  ~FindObjectPropertiesNeedingUpdateScope() {
    // No need to check when already marked as needing an update.
    if (m_neededPaintPropertyUpdate)
      return;

    // If paint properties are not marked as needing an update but still change,
    // we are missing a call to LayoutObject::setNeedsPaintPropertyUpdate().
    const auto* objectProperties = m_object.paintProperties();
    if (m_properties && objectProperties) {
      DCHECK_PTR_VAL_EQ(m_properties->paintOffsetTranslation(),
                        objectProperties->paintOffsetTranslation());
      DCHECK_PTR_VAL_EQ(m_properties->transform(),
                        objectProperties->transform());
      DCHECK_PTR_VAL_EQ(m_properties->effect(), objectProperties->effect());
      DCHECK_PTR_VAL_EQ(m_properties->cssClip(), objectProperties->cssClip());
      DCHECK_PTR_VAL_EQ(m_properties->cssClipFixedPosition(),
                        objectProperties->cssClipFixedPosition());
      DCHECK_PTR_VAL_EQ(m_properties->innerBorderRadiusClip(),
                        objectProperties->innerBorderRadiusClip());
      DCHECK_PTR_VAL_EQ(m_properties->overflowClip(),
                        objectProperties->overflowClip());
      DCHECK_PTR_VAL_EQ(m_properties->perspective(),
                        objectProperties->perspective());
      DCHECK_PTR_VAL_EQ(m_properties->svgLocalToBorderBoxTransform(),
                        objectProperties->svgLocalToBorderBoxTransform());
      DCHECK_PTR_VAL_EQ(m_properties->scrollTranslation(),
                        objectProperties->scrollTranslation());
      DCHECK_PTR_VAL_EQ(m_properties->scrollbarPaintOffset(),
                        objectProperties->scrollbarPaintOffset());
      const auto* borderBox = m_properties->localBorderBoxProperties();
      const auto* objectBorderBox =
          objectProperties->localBorderBoxProperties();
      if (borderBox && objectBorderBox) {
        DCHECK(borderBox->paintOffset == objectBorderBox->paintOffset);
        DCHECK_EQ(borderBox->propertyTreeState.transform(),
                  objectBorderBox->propertyTreeState.transform());
        DCHECK_EQ(borderBox->propertyTreeState.clip(),
                  objectBorderBox->propertyTreeState.clip());
        DCHECK_EQ(borderBox->propertyTreeState.effect(),
                  objectBorderBox->propertyTreeState.effect());
        DCHECK_EQ(borderBox->propertyTreeState.scroll(),
                  objectBorderBox->propertyTreeState.scroll());
      } else {
        DCHECK_EQ(!!borderBox, !!objectBorderBox);
      }
    } else {
      DCHECK_EQ(!!m_properties, !!objectProperties);
    }
    // Restore original clean bit.
    const_cast<LayoutObject&>(m_object).clearNeedsPaintPropertyUpdate();
  }

 private:
  const LayoutObject& m_object;
  bool m_neededPaintPropertyUpdate;
  std::unique_ptr<const ObjectPaintProperties> m_properties;
};

}  // namespace blink
#endif  // DCHECK_IS_ON()

#endif  // FindPropertiesNeedingUpdate_h
