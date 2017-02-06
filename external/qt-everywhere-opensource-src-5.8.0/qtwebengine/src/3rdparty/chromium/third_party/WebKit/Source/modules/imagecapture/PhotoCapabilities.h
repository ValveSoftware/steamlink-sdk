// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PhotoCapabilities_h
#define PhotoCapabilities_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "modules/imagecapture/MediaSettingsRange.h"
#include "wtf/text/WTFString.h"

namespace blink {

class PhotoCapabilities final
    : public GarbageCollectedFinalized<PhotoCapabilities>
    , public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
public:
    static PhotoCapabilities* create()
    {
        return new PhotoCapabilities();
    }
    virtual ~PhotoCapabilities() = default;

    MediaSettingsRange* zoom() const { return m_zoom; }
    void setZoom(MediaSettingsRange* value) { m_zoom = value; }

    DEFINE_INLINE_TRACE()
    {
        visitor->trace(m_zoom);
    }

private:
    PhotoCapabilities() = default;

    Member<MediaSettingsRange> m_zoom;
};

} // namespace blink

#endif // PhotoCapabilities_h
