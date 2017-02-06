// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RelatedApplication_h
#define RelatedApplication_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "platform/heap/GarbageCollected.h"
#include "wtf/text/WTFString.h"

namespace blink {

class RelatedApplication final : public GarbageCollectedFinalized<RelatedApplication>, public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
public:
    static RelatedApplication* create(const String& platform, const String& url, const String& id)
    {
        return new RelatedApplication(platform, url, id);
    }

    virtual ~RelatedApplication() {}

    String platform() const { return m_platform; }
    String url() const { return m_url; }
    String id() const { return m_id; }

    DEFINE_INLINE_TRACE() {}

private:
    RelatedApplication(const String& platform, const String& url, const String& id)
        : m_platform(platform)
        , m_url(url)
        , m_id(id)
    {
    }

    const String m_platform;
    const String m_url;
    const String m_id;
};

} // namespace blink

#endif // RelatedApplication_h
