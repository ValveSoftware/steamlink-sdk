// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AnimationNodeTiming_h
#define AnimationNodeTiming_h

#include "core/animation/AnimationNode.h"
#include "wtf/RefCounted.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class AnimationNodeTiming : public RefCountedWillBeGarbageCollectedFinalized<AnimationNodeTiming> {
public:
    static PassRefPtrWillBeRawPtr<AnimationNodeTiming> create(AnimationNode* parent);
    double delay();
    double endDelay();
    String fill();
    double iterationStart();
    double iterations();
    void getDuration(String propertyName, bool& element0Enabled, double& element0, bool& element1Enabled, String& element1);
    double playbackRate();
    String direction();
    String easing();

    void setDelay(double);
    void setEndDelay(double);
    void setFill(String);
    void setIterationStart(double);
    void setIterations(double);
    bool setDuration(String name, double duration);
    void setPlaybackRate(double);
    void setDirection(String);
    void setEasing(String);

    void trace(Visitor*);

private:
    RefPtrWillBeMember<AnimationNode> m_parent;
    explicit AnimationNodeTiming(AnimationNode*);
};

} // namespace WebCore

#endif
