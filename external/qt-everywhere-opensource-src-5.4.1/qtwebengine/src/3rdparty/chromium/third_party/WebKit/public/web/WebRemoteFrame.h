// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebRemoteFrame_h
#define WebRemoteFrame_h

#include "public/web/WebFrame.h"

namespace blink {

class WebRemoteFrame : public WebFrame {
public:
    BLINK_EXPORT static WebRemoteFrame* create(WebFrameClient*);

    virtual WebLocalFrame* createLocalChild(const WebString& name, WebFrameClient*) = 0;
    virtual WebRemoteFrame* createRemoteChild(const WebString& name, WebFrameClient*) = 0;
};

} // namespace blink

#endif // WebRemoteFrame_h
