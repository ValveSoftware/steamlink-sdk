// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BeaconLoader_h
#define BeaconLoader_h

#include "core/loader/PingLoader.h"
#include "platform/heap/Handle.h"
#include "wtf/Noncopyable.h"

namespace WTF {
class ArrayBufferView;
}

namespace WebCore {

class Blob;
class DOMFormData;
class KURL;
class LocalFrame;

// Issue asynchronous beacon transmission loads independent of LocalFrame
// staying alive. PingLoader providing the service.
class BeaconLoader FINAL : public PingLoader {
    WTF_MAKE_NONCOPYABLE(BeaconLoader);
    WTF_MAKE_FAST_ALLOCATED;
public:
    virtual ~BeaconLoader() { }

    static bool sendBeacon(LocalFrame*, int, const KURL&, const String&, int&);
    static bool sendBeacon(LocalFrame*, int, const KURL&, PassRefPtr<WTF::ArrayBufferView>&, int&);
    static bool sendBeacon(LocalFrame*, int, const KURL&, PassRefPtrWillBeRawPtr<Blob>&, int&);
    static bool sendBeacon(LocalFrame*, int, const KURL&, PassRefPtrWillBeRawPtr<DOMFormData>&, int&);

private:
    static void prepareRequest(LocalFrame*, ResourceRequest&);
    static void issueRequest(LocalFrame*, ResourceRequest&);
};

} // namespace WebCore

#endif // BeaconLoader_h
