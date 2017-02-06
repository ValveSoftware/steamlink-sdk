// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebMediaPlayerSource_h
#define WebMediaPlayerSource_h

#include "WebCommon.h"
#include "WebMediaStream.h"
#include "WebURL.h"

namespace blink {

class BLINK_PLATFORM_EXPORT WebMediaPlayerSource {
public:
    WebMediaPlayerSource();
    explicit WebMediaPlayerSource(const WebURL&);
    explicit WebMediaPlayerSource(const WebMediaStream&);
    ~WebMediaPlayerSource();

    bool isURL() const;
    WebURL getAsURL() const;

    bool isMediaStream() const;
    WebMediaStream getAsMediaStream() const;

private:
    WebURL m_url;
    WebMediaStream m_mediaStream;
};

} // namespace blink

#endif // WebMediaPlayerSource_h
