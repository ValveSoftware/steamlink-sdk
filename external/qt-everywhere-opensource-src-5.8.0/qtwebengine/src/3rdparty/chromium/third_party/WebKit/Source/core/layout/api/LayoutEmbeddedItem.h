// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayoutEmbeddedItem_h
#define LayoutEmbeddedItem_h

#include "core/layout/LayoutEmbeddedObject.h"
#include "core/layout/api/LayoutPartItem.h"

namespace blink {

class LayoutEmbeddedItem : public LayoutPartItem {
public:
    explicit LayoutEmbeddedItem(LayoutEmbeddedObject* layoutEmbeddedObject)
        : LayoutPartItem(layoutEmbeddedObject)
    {
    }

    explicit LayoutEmbeddedItem(const LayoutItem& item)
        : LayoutPartItem(item)
    {
        ASSERT_WITH_SECURITY_IMPLICATION(!item || item.isEmbeddedObject());
    }

    explicit LayoutEmbeddedItem(std::nullptr_t) : LayoutPartItem(nullptr) { }

    LayoutEmbeddedItem() { }

    void setPluginUnavailabilityReason(LayoutEmbeddedObject::PluginUnavailabilityReason reason)
    {
        toEmbeddedObject()->setPluginUnavailabilityReason(reason);
    }

    bool showsUnavailablePluginIndicator() const
    {
        return toEmbeddedObject()->showsUnavailablePluginIndicator();
    }

private:
    LayoutEmbeddedObject* toEmbeddedObject()
    {
        return toLayoutEmbeddedObject(layoutObject());
    }

    const LayoutEmbeddedObject* toEmbeddedObject() const
    {
        return toLayoutEmbeddedObject(layoutObject());
    }
};

} // namespace blink

#endif // LayoutEmbeddedItem_h
