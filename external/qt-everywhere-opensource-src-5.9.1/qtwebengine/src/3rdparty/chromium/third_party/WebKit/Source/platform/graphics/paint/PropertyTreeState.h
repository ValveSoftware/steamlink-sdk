// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PropertyTreeState_h
#define PropertyTreeState_h

#include "platform/graphics/paint/ClipPaintPropertyNode.h"
#include "platform/graphics/paint/EffectPaintPropertyNode.h"
#include "platform/graphics/paint/ScrollPaintPropertyNode.h"
#include "platform/graphics/paint/TransformPaintPropertyNode.h"
#include "wtf/HashFunctions.h"
#include "wtf/HashTraits.h"

namespace blink {

// A complete set of paint properties including those that are inherited from
// other objects.  RefPtrs are used to guard against use-after-free bugs and
// DCHECKs ensure PropertyTreeState never retains the last reference to a
// property tree node.
class PropertyTreeState {
 public:
  PropertyTreeState(const TransformPaintPropertyNode* transform,
                    const ClipPaintPropertyNode* clip,
                    const EffectPaintPropertyNode* effect,
                    const ScrollPaintPropertyNode* scroll)
      : m_transform(transform),
        m_clip(clip),
        m_effect(effect),
        m_scroll(scroll) {
    DCHECK(!m_transform->hasOneRef() && !m_clip->hasOneRef() &&
           !m_effect->hasOneRef() && !m_scroll->hasOneRef());
  }

  const TransformPaintPropertyNode* transform() const {
    DCHECK(!m_transform->hasOneRef());
    return m_transform.get();
  }
  void setTransform(const TransformPaintPropertyNode* node) {
    m_transform = node;
    DCHECK(!node->hasOneRef());
  }

  const ClipPaintPropertyNode* clip() const {
    DCHECK(!m_clip->hasOneRef());
    return m_clip.get();
  }
  void setClip(const ClipPaintPropertyNode* node) {
    m_clip = node;
    DCHECK(!node->hasOneRef());
  }

  const EffectPaintPropertyNode* effect() const {
    DCHECK(!m_effect->hasOneRef());
    return m_effect.get();
  }
  void setEffect(const EffectPaintPropertyNode* node) {
    m_effect = node;
    DCHECK(!node->hasOneRef());
  }

  const ScrollPaintPropertyNode* scroll() const {
    DCHECK(!m_scroll->hasOneRef());
    return m_scroll.get();
  }
  void setScroll(const ScrollPaintPropertyNode* node) {
    m_scroll = node;
    DCHECK(!node->hasOneRef());
  }

 private:
  RefPtr<const TransformPaintPropertyNode> m_transform;
  RefPtr<const ClipPaintPropertyNode> m_clip;
  RefPtr<const EffectPaintPropertyNode> m_effect;
  RefPtr<const ScrollPaintPropertyNode> m_scroll;
};
}  // namespace blink

#endif  // PropertyTreeState_h
