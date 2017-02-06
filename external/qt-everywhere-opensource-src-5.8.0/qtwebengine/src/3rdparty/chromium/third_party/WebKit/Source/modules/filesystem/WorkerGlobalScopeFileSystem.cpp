/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 * Copyright (C) 2009, 2011 Google Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "modules/filesystem/WorkerGlobalScopeFileSystem.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"
#include "core/fileapi/FileError.h"
#include "core/workers/WorkerGlobalScope.h"
#include "modules/filesystem/DOMFileSystemBase.h"
#include "modules/filesystem/DirectoryEntrySync.h"
#include "modules/filesystem/ErrorCallback.h"
#include "modules/filesystem/FileEntrySync.h"
#include "modules/filesystem/FileSystemCallback.h"
#include "modules/filesystem/FileSystemCallbacks.h"
#include "modules/filesystem/LocalFileSystem.h"
#include "modules/filesystem/SyncCallbackHelper.h"
#include "platform/FileSystemType.h"
#include "platform/weborigin/SecurityOrigin.h"
#include <memory>

namespace blink {

void WorkerGlobalScopeFileSystem::webkitRequestFileSystem(WorkerGlobalScope& worker, int type, long long size, FileSystemCallback* successCallback, ErrorCallback* errorCallback)
{
    ExecutionContext* secureContext = worker.getExecutionContext();
    if (!secureContext->getSecurityOrigin()->canAccessFileSystem()) {
        DOMFileSystem::reportError(&worker, errorCallback, FileError::create(FileError::SECURITY_ERR));
        return;
    }

    FileSystemType fileSystemType = static_cast<FileSystemType>(type);
    if (!DOMFileSystemBase::isValidType(fileSystemType)) {
        DOMFileSystem::reportError(&worker, errorCallback, FileError::create(FileError::INVALID_MODIFICATION_ERR));
        return;
    }

    LocalFileSystem::from(worker)->requestFileSystem(&worker, fileSystemType, size, FileSystemCallbacks::create(successCallback, errorCallback, &worker, fileSystemType));
}

DOMFileSystemSync* WorkerGlobalScopeFileSystem::webkitRequestFileSystemSync(WorkerGlobalScope& worker, int type, long long size, ExceptionState& exceptionState)
{
    ExecutionContext* secureContext = worker.getExecutionContext();
    if (!secureContext->getSecurityOrigin()->canAccessFileSystem()) {
        exceptionState.throwSecurityError(FileError::securityErrorMessage);
        return 0;
    }

    FileSystemType fileSystemType = static_cast<FileSystemType>(type);
    if (!DOMFileSystemBase::isValidType(fileSystemType)) {
        exceptionState.throwDOMException(InvalidModificationError, "the type must be TEMPORARY or PERSISTENT.");
        return 0;
    }

    FileSystemSyncCallbackHelper* helper = FileSystemSyncCallbackHelper::create();
    std::unique_ptr<AsyncFileSystemCallbacks> callbacks = FileSystemCallbacks::create(helper->getSuccessCallback(), helper->getErrorCallback(), &worker, fileSystemType);
    callbacks->setShouldBlockUntilCompletion(true);

    LocalFileSystem::from(worker)->requestFileSystem(&worker, fileSystemType, size, std::move(callbacks));
    return helper->getResult(exceptionState);
}

void WorkerGlobalScopeFileSystem::webkitResolveLocalFileSystemURL(WorkerGlobalScope& worker, const String& url, EntryCallback* successCallback, ErrorCallback* errorCallback)
{
    KURL completedURL = worker.completeURL(url);
    ExecutionContext* secureContext = worker.getExecutionContext();
    if (!secureContext->getSecurityOrigin()->canAccessFileSystem() || !secureContext->getSecurityOrigin()->canRequest(completedURL)) {
        DOMFileSystem::reportError(&worker, errorCallback, FileError::create(FileError::SECURITY_ERR));
        return;
    }

    if (!completedURL.isValid()) {
        DOMFileSystem::reportError(&worker, errorCallback, FileError::create(FileError::ENCODING_ERR));
        return;
    }

    LocalFileSystem::from(worker)->resolveURL(&worker, completedURL, ResolveURICallbacks::create(successCallback, errorCallback, &worker));
}

EntrySync* WorkerGlobalScopeFileSystem::webkitResolveLocalFileSystemSyncURL(WorkerGlobalScope& worker, const String& url, ExceptionState& exceptionState)
{
    KURL completedURL = worker.completeURL(url);
    ExecutionContext* secureContext = worker.getExecutionContext();
    if (!secureContext->getSecurityOrigin()->canAccessFileSystem() || !secureContext->getSecurityOrigin()->canRequest(completedURL)) {
        exceptionState.throwSecurityError(FileError::securityErrorMessage);
        return 0;
    }

    if (!completedURL.isValid()) {
        exceptionState.throwDOMException(EncodingError, "the URL '" + url + "' is invalid.");
        return 0;
    }

    EntrySyncCallbackHelper* resolveURLHelper = EntrySyncCallbackHelper::create();
    std::unique_ptr<AsyncFileSystemCallbacks> callbacks = ResolveURICallbacks::create(resolveURLHelper->getSuccessCallback(), resolveURLHelper->getErrorCallback(), &worker);
    callbacks->setShouldBlockUntilCompletion(true);

    LocalFileSystem::from(worker)->resolveURL(&worker, completedURL, std::move(callbacks));

    return resolveURLHelper->getResult(exceptionState);
}

static_assert(static_cast<int>(WorkerGlobalScopeFileSystem::TEMPORARY) == static_cast<int>(FileSystemTypeTemporary), "WorkerGlobalScopeFileSystem::TEMPORARY should match FileSystemTypeTemporary");
static_assert(static_cast<int>(WorkerGlobalScopeFileSystem::PERSISTENT) == static_cast<int>(FileSystemTypePersistent), "WorkerGlobalScopeFileSystem::PERSISTENT should match FileSystemTypePersistent");

} // namespace blink
