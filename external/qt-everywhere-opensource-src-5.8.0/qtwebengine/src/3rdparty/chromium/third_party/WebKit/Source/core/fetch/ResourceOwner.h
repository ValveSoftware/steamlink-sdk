/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ResourceOwner_h
#define ResourceOwner_h

#include "core/fetch/Resource.h"
#include <type_traits>

namespace blink {

template <typename Client, bool isClientGarbageCollectedMixin>
class ResourceOwnerBase;

template <typename Client>
class ResourceOwnerBase<Client, true> : public Client {
public:
    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        Client::trace(visitor);
    }
};

// TODO(yhirano): Remove this template once all ResourceClients become
// GarbageCollectedMixin.
template <typename Client>
class ResourceOwnerBase<Client, false> : public GarbageCollectedMixin, public Client {
public:
    DEFINE_INLINE_VIRTUAL_TRACE() {}
};

template<class R, class C = typename R::ClientType>
class ResourceOwner : public ResourceOwnerBase<C, std::is_base_of<GarbageCollectedMixin, C>::value> {
    USING_PRE_FINALIZER(ResourceOwner, clearResource);
public:
    using ResourceType = R;
    ~ResourceOwner() override {}
    ResourceType* resource() const { return m_resource; }

    DEFINE_INLINE_TRACE()
    {
        visitor->trace(m_resource);
        ResourceOwnerBase<C, std::is_base_of<GarbageCollectedMixin, C>::value>::trace(visitor);
    }

protected:
    ResourceOwner()
    {
        ThreadState::current()->registerPreFinalizer(this);
    }

    void setResource(ResourceType*);
    void clearResource() { setResource(nullptr); }

private:
    Member<ResourceType> m_resource;
};

template<class R, class C>
inline void ResourceOwner<R, C>::setResource(R* newResource)
{
    if (newResource == m_resource)
        return;

    // Some ResourceClient implementations reenter this so
    // we need to prevent double removal.
    if (ResourceType* oldResource = m_resource.release())
        oldResource->removeClient(this);

    if (newResource) {
        m_resource = newResource;
        m_resource->addClient(this);
    }
}

} // namespace blink

#endif
