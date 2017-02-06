// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayoutMediaItem_h
#define LayoutMediaItem_h

#include "core/layout/LayoutMedia.h"
#include "core/layout/api/LayoutImageItem.h"

namespace blink {

class LayoutMediaItem : public LayoutImageItem {
public:
    explicit LayoutMediaItem(LayoutMedia* layoutMedia)
        : LayoutImageItem(layoutMedia)
    {
    }

    explicit LayoutMediaItem(const LayoutItem& item)
        : LayoutImageItem(item)
    {
        ASSERT_WITH_SECURITY_IMPLICATION(!item || item.isMedia());
    }

    explicit LayoutMediaItem(std::nullptr_t) : LayoutImageItem(nullptr) { }

    LayoutMediaItem() { }

    void setRequestPositionUpdates(bool want)
    {
        return toMedia()->setRequestPositionUpdates(want);
    }

private:
    LayoutMedia* toMedia()
    {
        return toLayoutMedia(layoutObject());
    }

    const LayoutMedia* toMedia() const
    {
        return toLayoutMedia(layoutObject());
    }
};

} // namespace blink

#endif // LayoutMediaItem_h
