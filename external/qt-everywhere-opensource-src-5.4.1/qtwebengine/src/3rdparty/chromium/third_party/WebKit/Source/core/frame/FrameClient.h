// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FrameClient_h
#define FrameClient_h

namespace WebCore {

class Frame;

class FrameClient {
public:
    virtual Frame* opener() const = 0;
    virtual void setOpener(Frame*) = 0;

    virtual Frame* parent() const = 0;
    virtual Frame* top() const = 0;
    virtual Frame* previousSibling() const = 0;
    virtual Frame* nextSibling() const = 0;
    virtual Frame* firstChild() const = 0;
    virtual Frame* lastChild() const = 0;

    virtual ~FrameClient() { }
};

} // namespace WebCore

#endif // FrameClient_h
