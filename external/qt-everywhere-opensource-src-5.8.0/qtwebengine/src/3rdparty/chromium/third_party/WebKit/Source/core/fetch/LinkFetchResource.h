// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef LinkFetchResource_h
#define LinkFetchResource_h

#include "core/fetch/Resource.h"
#include "core/fetch/ResourceClient.h"

namespace blink {

class FetchRequest;
class ResourceFetcher;

class LinkFetchResource final : public Resource {
public:
    using ClientType = ResourceClient;

    static Resource* fetch(Resource::Type, FetchRequest&, ResourceFetcher*);
    ~LinkFetchResource() override;

private:
    class LinkResourceFactory : public ResourceFactory {
    public:
        LinkResourceFactory(Resource::Type type)
            : ResourceFactory(type) { }

        Resource* create(const ResourceRequest& request, const ResourceLoaderOptions& options, const String& charset) const override
        {
            return new LinkFetchResource(request, type(), options);
        }
    };
    LinkFetchResource(const ResourceRequest&, Type, const ResourceLoaderOptions&);
};

} // namespace blink

#endif // LinkFetchResource_h
