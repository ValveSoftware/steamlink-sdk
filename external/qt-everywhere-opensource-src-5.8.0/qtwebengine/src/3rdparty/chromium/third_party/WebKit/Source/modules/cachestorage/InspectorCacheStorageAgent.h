// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InspectorCacheStorageAgent_h
#define InspectorCacheStorageAgent_h

#include "core/inspector/InspectorBaseAgent.h"
#include "core/inspector/protocol/CacheStorage.h"
#include "modules/ModulesExport.h"
#include "wtf/text/WTFString.h"

namespace blink {


class MODULES_EXPORT InspectorCacheStorageAgent final : public InspectorBaseAgent<protocol::CacheStorage::Metainfo> {
    WTF_MAKE_NONCOPYABLE(InspectorCacheStorageAgent);

public:
    static InspectorCacheStorageAgent* create()
    {
        return new InspectorCacheStorageAgent();
    }

    ~InspectorCacheStorageAgent() override;

    DECLARE_VIRTUAL_TRACE();

    void requestCacheNames(ErrorString*, const String& in_securityOrigin, std::unique_ptr<RequestCacheNamesCallback>) override;
    void requestEntries(ErrorString*, const String& in_cacheId, int in_skipCount, int in_pageSize, std::unique_ptr<RequestEntriesCallback>) override;
    void deleteCache(ErrorString*, const String& in_cacheId, std::unique_ptr<DeleteCacheCallback>) override;
    void deleteEntry(ErrorString*, const String& in_cacheId, const String& in_request, std::unique_ptr<DeleteEntryCallback>) override;

private:
    explicit InspectorCacheStorageAgent();
};

} // namespace blink

#endif // InspectorCacheStorageAgent_h
