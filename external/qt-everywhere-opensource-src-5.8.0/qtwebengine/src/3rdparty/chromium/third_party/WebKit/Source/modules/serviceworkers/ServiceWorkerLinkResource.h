// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ServiceWorkerLinkResource_h
#define ServiceWorkerLinkResource_h

#include "core/html/LinkResource.h"
#include "modules/ModulesExport.h"
#include "wtf/Allocator.h"
#include "wtf/RefPtr.h"

namespace blink {

class HTMLLinkElement;

class MODULES_EXPORT ServiceWorkerLinkResource final : public LinkResource {
public:

    static ServiceWorkerLinkResource* create(HTMLLinkElement* owner);

    ~ServiceWorkerLinkResource() override;

    // LinkResource implementation:
    void process() override;
    LinkResourceType type() const override { return Other; }
    bool hasLoaded() const override;
    void ownerRemoved() override;

private:
    explicit ServiceWorkerLinkResource(HTMLLinkElement* owner);
};

} // namespace blink

#endif // ServiceWorkerLinkResource_h
