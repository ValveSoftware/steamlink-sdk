/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CompositorAnimationsTestHelper_h
#define CompositorAnimationsTestHelper_h

#include "core/animation/CompositorAnimations.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCompositorSupport.h"
#include "public/platform/WebFloatAnimationCurve.h"
#include "public/platform/WebFloatKeyframe.h"
#include "wtf/PassOwnPtr.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>


namespace testing {

template<typename T>
PassOwnPtr<T> CloneToPassOwnPtr(T& o)
{
    return adoptPtr(new T(o));
}

} // namespace testing


// Test helpers and mocks for blink::Web* types
// -----------------------------------------------------------------------
namespace blink {

// blink::WebFloatKeyframe is a plain struct, so we just create an == operator
// for it.
inline bool operator==(const WebFloatKeyframe& a, const WebFloatKeyframe& b)
{
    return a.time == b.time && a.value == b.value;
}

inline void PrintTo(const WebFloatKeyframe& frame, ::std::ostream* os)
{
    *os << "WebFloatKeyframe@" << &frame << "(" << frame.time << ", " << frame.value << ")";
}

// -----------------------------------------------------------------------

class WebAnimationMock : public blink::WebAnimation {
private:
    blink::WebAnimation::TargetProperty m_property;

public:
    // Target Property is set through the constructor.
    WebAnimationMock(blink::WebAnimation::TargetProperty p) : m_property(p) { }
    virtual blink::WebAnimation::TargetProperty targetProperty() const { return m_property; };

    MOCK_METHOD0(id, int());

    MOCK_CONST_METHOD0(iterations, int());
    MOCK_METHOD1(setIterations, void(int));

    MOCK_CONST_METHOD0(startTime, double());
    MOCK_METHOD1(setStartTime, void(double));

    MOCK_CONST_METHOD0(timeOffset, double());
    MOCK_METHOD1(setTimeOffset, void(double));

    MOCK_CONST_METHOD0(alternatesDirection, bool());
    MOCK_METHOD1(setAlternatesDirection, void(bool));

    MOCK_METHOD0(delete_, void());
    ~WebAnimationMock() { delete_(); }
};

template<typename CurveType, blink::WebAnimationCurve::AnimationCurveType CurveId, typename KeyframeType>
class WebAnimationCurveMock : public CurveType {
public:
    MOCK_METHOD1_T(add, void(const KeyframeType&));
    MOCK_METHOD2_T(add, void(const KeyframeType&, blink::WebAnimationCurve::TimingFunctionType));
    MOCK_METHOD5_T(add, void(const KeyframeType&, double, double, double, double));

    MOCK_CONST_METHOD1_T(getValue, float(double)); // Only on WebFloatAnimationCurve, but can't hurt to have here.

    virtual blink::WebAnimationCurve::AnimationCurveType type() const { return CurveId; };

    MOCK_METHOD0(delete_, void());
    ~WebAnimationCurveMock() { delete_(); }
};

typedef WebAnimationCurveMock<blink::WebFloatAnimationCurve, blink::WebAnimationCurve::AnimationCurveTypeFloat, blink::WebFloatKeyframe> WebFloatAnimationCurveMock;

} // namespace blink

namespace WebCore {

class AnimationCompositorAnimationsTestBase : public ::testing::Test {
public:
    AnimationCompositorAnimationsTestBase() : m_proxyPlatform(&m_mockCompositor) { };

    class WebCompositorSupportMock : public blink::WebCompositorSupport {
    public:
        MOCK_METHOD3(createAnimation, blink::WebAnimation*(const blink::WebAnimationCurve& curve, blink::WebAnimation::TargetProperty target, int animationId));
        MOCK_METHOD0(createFloatAnimationCurve, blink::WebFloatAnimationCurve*());
    };

private:
    class PlatformProxy : public blink::Platform {
    public:
        PlatformProxy(WebCompositorSupportMock** compositor) : m_compositor(compositor) { }

        virtual void cryptographicallyRandomValues(unsigned char* buffer, size_t length) { ASSERT_NOT_REACHED(); }
    private:
        WebCompositorSupportMock** m_compositor;
        virtual blink::WebCompositorSupport* compositorSupport() OVERRIDE { return *m_compositor; }
    };

    WebCompositorSupportMock* m_mockCompositor;
    PlatformProxy m_proxyPlatform;

protected:
    blink::Platform* m_platform;

    virtual void SetUp()
    {
        m_mockCompositor = 0;
        m_platform = blink::Platform::current();
        blink::Platform::initialize(&m_proxyPlatform);
    }

    virtual void TearDown()
    {
        blink::Platform::initialize(m_platform);
    }

    void setCompositorForTesting(WebCompositorSupportMock& mock)
    {
        ASSERT(!m_mockCompositor);
        m_mockCompositor = &mock;
    }
};

}

#endif
