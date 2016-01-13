/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#include "config.h"
#include "modules/quota/DeprecatedStorageQuota.h"

#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "modules/quota/DeprecatedStorageQuotaCallbacksImpl.h"
#include "modules/quota/StorageErrorCallback.h"
#include "modules/quota/StorageQuotaClient.h"
#include "modules/quota/StorageUsageCallback.h"
#include "platform/StorageQuotaCallbacks.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/Platform.h"
#include "public/platform/WebStorageQuotaCallbacks.h"
#include "public/platform/WebStorageQuotaType.h"

namespace WebCore {

DeprecatedStorageQuota::DeprecatedStorageQuota(Type type)
    : m_type(type)
{
    ScriptWrappable::init(this);
}

void DeprecatedStorageQuota::queryUsageAndQuota(ExecutionContext* executionContext, PassOwnPtr<StorageUsageCallback> successCallback, PassOwnPtr<StorageErrorCallback> errorCallback)
{
    ASSERT(executionContext);

    blink::WebStorageQuotaType storageType = static_cast<blink::WebStorageQuotaType>(m_type);
    if (storageType != blink::WebStorageQuotaTypeTemporary && storageType != blink::WebStorageQuotaTypePersistent) {
        // Unknown storage type is requested.
        executionContext->postTask(StorageErrorCallback::CallbackTask::create(errorCallback, NotSupportedError));
        return;
    }

    SecurityOrigin* securityOrigin = executionContext->securityOrigin();
    if (securityOrigin->isUnique()) {
        executionContext->postTask(StorageErrorCallback::CallbackTask::create(errorCallback, NotSupportedError));
        return;
    }

    KURL storagePartition = KURL(KURL(), securityOrigin->toString());
    OwnPtr<StorageQuotaCallbacks> callbacks = DeprecatedStorageQuotaCallbacksImpl::create(successCallback, errorCallback);
    blink::Platform::current()->queryStorageUsageAndQuota(storagePartition, storageType, callbacks.release());
}

void DeprecatedStorageQuota::requestQuota(ExecutionContext* executionContext, unsigned long long newQuotaInBytes, PassOwnPtr<StorageQuotaCallback> successCallback, PassOwnPtr<StorageErrorCallback> errorCallback)
{
    ASSERT(executionContext);

    blink::WebStorageQuotaType storageType = static_cast<blink::WebStorageQuotaType>(m_type);
    if (storageType != blink::WebStorageQuotaTypeTemporary && storageType != blink::WebStorageQuotaTypePersistent) {
        // Unknown storage type is requested.
        executionContext->postTask(StorageErrorCallback::CallbackTask::create(errorCallback, NotSupportedError));
        return;
    }

    StorageQuotaClient* client = StorageQuotaClient::from(executionContext);
    if (!client) {
        executionContext->postTask(StorageErrorCallback::CallbackTask::create(errorCallback, NotSupportedError));
        return;
    }

    client->requestQuota(executionContext, storageType, newQuotaInBytes, successCallback, errorCallback);
}

DeprecatedStorageQuota::~DeprecatedStorageQuota()
{
}

} // namespace WebCore
