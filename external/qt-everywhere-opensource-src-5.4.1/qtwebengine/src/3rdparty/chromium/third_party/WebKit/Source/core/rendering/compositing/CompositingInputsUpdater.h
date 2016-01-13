// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositingInputsUpdater_h
#define CompositingInputsUpdater_h

#include "core/rendering/RenderGeometryMap.h"

namespace WebCore {

class RenderLayer;

class CompositingInputsUpdater {
private:
    struct AncestorInfo {
        AncestorInfo()
            : enclosingCompositedLayer(0)
            , ancestorScrollingLayer(0)
        {
        }

        RenderLayer* enclosingCompositedLayer;
        RenderLayer* ancestorScrollingLayer;
    };

public:
    explicit CompositingInputsUpdater(RenderLayer* rootRenderLayer);
    ~CompositingInputsUpdater();

    enum UpdateType {
        DoNotForceUpdate,
        ForceUpdate,
    };

    void update(RenderLayer*, UpdateType = DoNotForceUpdate, AncestorInfo = AncestorInfo());

#if ASSERT_ENABLED
    static void assertNeedsCompositingInputsUpdateBitsCleared(RenderLayer*);
#endif

private:
    RenderGeometryMap m_geometryMap;
    RenderLayer* m_rootRenderLayer;
};

} // namespace WebCore

#endif // CompositingInputsUpdater_h
