// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebFrameImplBase_h
#define WebFrameImplBase_h

#include "platform/heap/Handle.h"
#include "web/WebExport.h"
#include "wtf/text/AtomicString.h"

namespace blink {

class Frame;
class FrameHost;
class FrameOwner;

// WebFrameImplBase exists to avoid the diamond inheritance problem:
// - The public interfaces WebLocalFrame/WebRemoteFrame extend WebFrame.
// - WebLocalFrameImpl implements WebLocalFrame and WebRemoteFrameImpl
//   implements WebRemoteFrame.
// - The private implementations should share some functionality, but cannot
//   inherit from a common base class inheriting WebFrame. This would result in
//   WebFrame beind inherited from two different base classes.
//
// To get around this, only the private implementations have WebFrameImplBase as
// a base class. WebFrame exposes a virtual accessor to retrieve the underlying
// implementation as an instance of the base class, but has no inheritance
// relationship with it. The cost is a virtual indirection, but this is nicer
// than the previous manual dispatch emulating real virtual dispatch.
class WEB_EXPORT WebFrameImplBase : public GarbageCollectedFinalized<WebFrameImplBase> {
public:
    virtual ~WebFrameImplBase();

    virtual void initializeCoreFrame(FrameHost*, FrameOwner*, const AtomicString& name, const AtomicString& uniqueName) = 0;
    // TODO(dcheng): Rename this to coreFrame()? This probably also shouldn't be const...
    virtual Frame* frame() const = 0;

    DECLARE_VIRTUAL_TRACE();
};

} // namespace blink

#endif // WebFrameImplBase_h
