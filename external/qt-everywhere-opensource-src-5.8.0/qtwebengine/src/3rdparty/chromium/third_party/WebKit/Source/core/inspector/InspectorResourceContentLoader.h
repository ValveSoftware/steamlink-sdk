// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InspectorResourceContentLoader_h
#define InspectorResourceContentLoader_h

#include "core/CoreExport.h"
#include "core/fetch/Resource.h"
#include "wtf/Functional.h"
#include "wtf/HashMap.h"
#include "wtf/HashSet.h"
#include "wtf/Noncopyable.h"
#include "wtf/Vector.h"

namespace blink {

class LocalFrame;
class Resource;

class CORE_EXPORT InspectorResourceContentLoader final : public GarbageCollectedFinalized<InspectorResourceContentLoader> {
    WTF_MAKE_NONCOPYABLE(InspectorResourceContentLoader);
public:
    static InspectorResourceContentLoader* create(LocalFrame* inspectedFrame)
    {
        return new InspectorResourceContentLoader(inspectedFrame);
    }
    ~InspectorResourceContentLoader();
    void dispose();
    DECLARE_TRACE();

    int createClientId();
    void ensureResourcesContentLoaded(int clientId, std::unique_ptr<WTF::Closure> callback);
    void cancel(int clientId);
    void didCommitLoadForLocalFrame(LocalFrame*);

private:
    class ResourceClient;

    explicit InspectorResourceContentLoader(LocalFrame*);
    void resourceFinished(ResourceClient*);
    void checkDone();
    void start();
    void stop();
    bool hasFinished();

    using Callbacks = Vector<std::unique_ptr<WTF::Closure>>;
    HashMap<int, Callbacks> m_callbacks;
    bool m_allRequestsStarted;
    bool m_started;
    Member<LocalFrame> m_inspectedFrame;
    HeapHashSet<Member<ResourceClient>> m_pendingResourceClients;
    HeapVector<Member<Resource>> m_resources;
    int m_lastClientId;

    friend class ResourceClient;
};

} // namespace blink


#endif // !defined(InspectorResourceContentLoader_h)
