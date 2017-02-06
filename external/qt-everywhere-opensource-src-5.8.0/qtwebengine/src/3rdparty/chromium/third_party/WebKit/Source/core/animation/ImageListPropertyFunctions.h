// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ImageListPropertyFunctions_h
#define ImageListPropertyFunctions_h

#include "core/CSSPropertyNames.h"
#include "core/style/ComputedStyle.h"
#include "platform/heap/Handle.h"

namespace blink {

using StyleImageList = PersistentHeapVector<Member<StyleImage>, 1>;

class ImageListPropertyFunctions {
public:
    static void getInitialImageList(CSSPropertyID, StyleImageList& result) { result.clear(); }

    static void getImageList(CSSPropertyID property, const ComputedStyle& style, StyleImageList& result)
    {
        const FillLayer* fillLayer = nullptr;
        switch (property) {
        case CSSPropertyBackgroundImage:
            fillLayer = &style.backgroundLayers();
            break;
        case CSSPropertyWebkitMaskImage:
            fillLayer = &style.maskLayers();
            break;
        default:
            NOTREACHED();
            return;
        }

        result.clear();
        while (fillLayer && fillLayer->image()) {
            result.append(fillLayer->image());
            fillLayer = fillLayer->next();
        }
    }

    static void setImageList(CSSPropertyID property, ComputedStyle& style, const StyleImageList& imageList)
    {
        FillLayer* fillLayer = nullptr;
        switch (property) {
        case CSSPropertyBackgroundImage:
            fillLayer = &style.accessBackgroundLayers();
            break;
        case CSSPropertyWebkitMaskImage:
            fillLayer = &style.accessMaskLayers();
            break;
        default:
            NOTREACHED();
            return;
        }

        FillLayer* prev = nullptr;
        for (size_t i = 0; i < imageList.size(); i++) {
            if (!fillLayer)
                fillLayer = prev->ensureNext();
            fillLayer->setImage(imageList[i]);
            prev = fillLayer;
            fillLayer = fillLayer->next();
        }
        while (fillLayer) {
            fillLayer->clearImage();
            fillLayer = fillLayer->next();
        }
    }
};

} // namespace blink

#endif // ImageListPropertyFunctions_h
