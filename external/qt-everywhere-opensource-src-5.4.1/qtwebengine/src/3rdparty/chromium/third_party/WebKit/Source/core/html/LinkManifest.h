// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LinkManifest_h
#define LinkManifest_h

#include "core/html/LinkResource.h"
#include "wtf/FastAllocBase.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/RefPtr.h"

namespace WebCore {

class HTMLLinkElement;

class LinkManifest FINAL : public LinkResource {
    WTF_MAKE_FAST_ALLOCATED_WILL_BE_REMOVED;
public:

    static PassOwnPtrWillBeRawPtr<LinkManifest> create(HTMLLinkElement* owner);

    virtual ~LinkManifest();

    // LinkResource
    virtual void process() OVERRIDE;
    virtual Type type() const OVERRIDE { return Manifest; }
    virtual bool hasLoaded() const OVERRIDE;
    virtual void ownerRemoved() OVERRIDE;

private:
    explicit LinkManifest(HTMLLinkElement* owner);
};

}

#endif // LinkManifest_h
