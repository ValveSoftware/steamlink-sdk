// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayoutBoxModel_h
#define LayoutBoxModel_h

#include "core/layout/LayoutBoxModelObject.h"
#include "core/layout/api/LayoutItem.h"

namespace blink {

class LayoutBoxModelObject;

class LayoutBoxModel : public LayoutItem {
public:
    explicit LayoutBoxModel(LayoutBoxModelObject* layoutBox)
        : LayoutItem(layoutBox)
    {
    }

    explicit LayoutBoxModel(const LayoutItem& item)
        : LayoutItem(item)
    {
        ASSERT_WITH_SECURITY_IMPLICATION(!item || item.isBoxModelObject());
    }

    explicit LayoutBoxModel(std::nullptr_t) : LayoutItem(nullptr) { }

    LayoutBoxModel() { }

    PaintLayer* layer() const
    {
        return toBoxModel()->layer();
    }

    PaintLayerScrollableArea* getScrollableArea() const
    {
        return toBoxModel()->getScrollableArea();
    }

private:
    LayoutBoxModelObject* toBoxModel() { return toLayoutBoxModelObject(layoutObject()); }
    const LayoutBoxModelObject* toBoxModel() const { return toLayoutBoxModelObject(layoutObject()); }
};

} // namespace blink

#endif // LayoutBoxModel_h
