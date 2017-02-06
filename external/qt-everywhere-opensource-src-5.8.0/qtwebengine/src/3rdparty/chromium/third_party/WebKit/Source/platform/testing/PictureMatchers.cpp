// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/testing/PictureMatchers.h"

#include "platform/geometry/FloatQuad.h"
#include "platform/geometry/FloatRect.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"
#include <utility>

namespace blink {

namespace {

class DrawsRectangleCanvas : public SkCanvas {
public:
    DrawsRectangleCanvas() : SkCanvas(800, 600) { }
    const Vector<std::pair<FloatQuad, Color>>& quads() const { return m_quads; }
    void onDrawRect(const SkRect& rect, const SkPaint& paint) override
    {
        SkPoint quad[4];
        getTotalMatrix().mapRectToQuad(quad, rect);
        FloatQuad floatQuad(quad);
        m_quads.append(std::make_pair(floatQuad, Color(paint.getColor())));
        SkCanvas::onDrawRect(rect, paint);
    }
private:
    Vector<std::pair<FloatQuad, Color>> m_quads;
};

class DrawsRectangleMatcher : public ::testing::MatcherInterface<const SkPicture&> {
public:
    DrawsRectangleMatcher(const FloatRect& rect, Color color) : m_rect(rect), m_color(color) { }

    bool MatchAndExplain(const SkPicture& picture, ::testing::MatchResultListener* listener) const override
    {
        DrawsRectangleCanvas canvas;
        picture.playback(&canvas);
        const auto& quads = canvas.quads();

        if (quads.size() != 1) {
            *listener << "which draws " << quads.size() << " quads";
            return false;
        }

        const FloatQuad& quad = quads[0].first;
        if (!quad.isRectilinear()) {
            if (listener->IsInterested()) {
                *listener << "which draws ";
                PrintTo(quad, listener->stream());
                *listener << " with color " << quads[0].second.serialized().ascii().data();
            }
            return false;
        }

        const FloatRect& rect = quad.boundingBox();
        if (rect != m_rect || quads[0].second != m_color) {
            if (listener->IsInterested()) {
                *listener << "which draws ";
                PrintTo(rect, listener->stream());
                *listener << " with color " << quads[0].second.serialized().ascii().data();
            }
            return false;
        }

        return true;
    }

    void DescribeTo(::std::ostream* os) const override
    {
        *os << "draws ";
        PrintTo(m_rect, os);
        *os << " with color " << m_color.serialized().ascii().data();
    }

private:
    const FloatRect m_rect;
    const Color m_color;
};

} // namespace

::testing::Matcher<const SkPicture&> drawsRectangle(const FloatRect& rect, Color color)
{
    return ::testing::MakeMatcher(new DrawsRectangleMatcher(rect, color));
}

} // namespace blink
