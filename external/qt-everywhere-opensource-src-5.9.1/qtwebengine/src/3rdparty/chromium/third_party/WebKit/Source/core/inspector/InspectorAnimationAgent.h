// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InspectorAnimationAgent_h
#define InspectorAnimationAgent_h

#include "core/CoreExport.h"
#include "core/animation/Animation.h"
#include "core/css/CSSKeyframesRule.h"
#include "core/inspector/InspectorBaseAgent.h"
#include "core/inspector/protocol/Animation.h"
#include "wtf/text/WTFString.h"

#include <v8-inspector.h>

namespace blink {

class AnimationTimeline;
class InspectedFrames;
class InspectorCSSAgent;

class CORE_EXPORT InspectorAnimationAgent final
    : public InspectorBaseAgent<protocol::Animation::Metainfo> {
  WTF_MAKE_NONCOPYABLE(InspectorAnimationAgent);

 public:
  InspectorAnimationAgent(InspectedFrames*,
                          InspectorCSSAgent*,
                          v8_inspector::V8InspectorSession*);

  // Base agent methods.
  void restore() override;
  void didCommitLoadForLocalFrame(LocalFrame*) override;

  // Protocol method implementations
  Response enable() override;
  Response disable() override;
  Response getPlaybackRate(double* playbackRate) override;
  Response setPlaybackRate(double) override;
  Response getCurrentTime(const String& id, double* currentTime) override;
  Response setPaused(std::unique_ptr<protocol::Array<String>> animations,
                     bool paused) override;
  Response setTiming(const String& animationId,
                     double duration,
                     double delay) override;
  Response seekAnimations(std::unique_ptr<protocol::Array<String>> animations,
                          double currentTime) override;
  Response releaseAnimations(
      std::unique_ptr<protocol::Array<String>> animations) override;
  Response resolveAnimation(
      const String& animationId,
      std::unique_ptr<v8_inspector::protocol::Runtime::API::RemoteObject>*)
      override;

  // API for InspectorInstrumentation
  void didCreateAnimation(unsigned);
  void animationPlayStateChanged(blink::Animation*,
                                 blink::Animation::AnimationPlayState,
                                 blink::Animation::AnimationPlayState);
  void didClearDocumentOfWindowObject(LocalFrame*);

  // Methods for other agents to use.
  Response assertAnimation(const String& id, blink::Animation*& result);

  DECLARE_VIRTUAL_TRACE();

 private:
  using AnimationType = protocol::Animation::Animation::TypeEnum;

  std::unique_ptr<protocol::Animation::Animation> buildObjectForAnimation(
      blink::Animation&);
  std::unique_ptr<protocol::Animation::Animation> buildObjectForAnimation(
      blink::Animation&,
      String,
      std::unique_ptr<protocol::Animation::KeyframesRule> keyframeRule =
          nullptr);
  double normalizedStartTime(blink::Animation&);
  AnimationTimeline& referenceTimeline();
  blink::Animation* animationClone(blink::Animation*);
  String createCSSId(blink::Animation&);

  Member<InspectedFrames> m_inspectedFrames;
  Member<InspectorCSSAgent> m_cssAgent;
  v8_inspector::V8InspectorSession* m_v8Session;
  HeapHashMap<String, Member<blink::Animation>> m_idToAnimation;
  HeapHashMap<String, Member<blink::Animation>> m_idToAnimationClone;
  HashMap<String, String> m_idToAnimationType;
  bool m_isCloning;
  HashSet<String> m_clearedAnimations;
};

}  // namespace blink

#endif  // InspectorAnimationAgent_h
