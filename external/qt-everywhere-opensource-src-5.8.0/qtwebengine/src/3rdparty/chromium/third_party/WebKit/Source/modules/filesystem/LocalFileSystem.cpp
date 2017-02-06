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

#include "modules/filesystem/LocalFileSystem.h"

#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/ExecutionContextTask.h"
#include "core/fileapi/FileError.h"
#include "core/frame/LocalFrame.h"
#include "core/workers/WorkerGlobalScope.h"
#include "modules/filesystem/FileSystemClient.h"
#include "platform/AsyncFileSystemCallbacks.h"
#include "platform/ContentSettingCallbacks.h"
#include "public/platform/Platform.h"
#include "public/platform/WebFileSystem.h"
#include "wtf/Functional.h"
#include <memory>

namespace blink {

namespace {

void reportFailure(std::unique_ptr<AsyncFileSystemCallbacks> callbacks, FileError::ErrorCode error)
{
    callbacks->didFail(error);
}

} // namespace

class CallbackWrapper final : public GarbageCollectedFinalized<CallbackWrapper> {
public:
    CallbackWrapper(std::unique_ptr<AsyncFileSystemCallbacks> c)
        : m_callbacks(std::move(c))
    {
    }
    virtual ~CallbackWrapper() { }
    std::unique_ptr<AsyncFileSystemCallbacks> release()
    {
        return std::move(m_callbacks);
    }

    DEFINE_INLINE_TRACE() { }

private:
    std::unique_ptr<AsyncFileSystemCallbacks> m_callbacks;
};

LocalFileSystem* LocalFileSystem::create(std::unique_ptr<FileSystemClient> client)
{
    return new LocalFileSystem(std::move(client));
}

LocalFileSystem::~LocalFileSystem()
{
}

void LocalFileSystem::resolveURL(ExecutionContext* context, const KURL& fileSystemURL, std::unique_ptr<AsyncFileSystemCallbacks> callbacks)
{
    CallbackWrapper* wrapper = new CallbackWrapper(std::move(callbacks));
    requestFileSystemAccessInternal(context,
        WTF::bind(&LocalFileSystem::resolveURLInternal, wrapPersistent(this), wrapPersistent(context), fileSystemURL, wrapPersistent(wrapper)),
        WTF::bind(&LocalFileSystem::fileSystemNotAllowedInternal, wrapPersistent(this), wrapPersistent(context), wrapPersistent(wrapper)));
}

void LocalFileSystem::requestFileSystem(ExecutionContext* context, FileSystemType type, long long size, std::unique_ptr<AsyncFileSystemCallbacks> callbacks)
{
    CallbackWrapper* wrapper = new CallbackWrapper(std::move(callbacks));
    requestFileSystemAccessInternal(context,
        WTF::bind(&LocalFileSystem::fileSystemAllowedInternal, wrapPersistent(this), wrapPersistent(context), type, wrapPersistent(wrapper)),
        WTF::bind(&LocalFileSystem::fileSystemNotAllowedInternal, wrapPersistent(this), wrapPersistent(context), wrapPersistent(wrapper)));
}

void LocalFileSystem::deleteFileSystem(ExecutionContext* context, FileSystemType type, std::unique_ptr<AsyncFileSystemCallbacks> callbacks)
{
    ASSERT(context);
    ASSERT_WITH_SECURITY_IMPLICATION(context->isDocument());

    CallbackWrapper* wrapper = new CallbackWrapper(std::move(callbacks));
    requestFileSystemAccessInternal(context,
        WTF::bind(&LocalFileSystem::deleteFileSystemInternal, wrapPersistent(this), wrapPersistent(context), type, wrapPersistent(wrapper)),
        WTF::bind(&LocalFileSystem::fileSystemNotAllowedInternal, wrapPersistent(this), wrapPersistent(context), wrapPersistent(wrapper)));
}

WebFileSystem* LocalFileSystem::getFileSystem() const
{
    Platform* platform = Platform::current();
    if (!platform)
        return nullptr;

    return platform->fileSystem();
}

void LocalFileSystem::requestFileSystemAccessInternal(ExecutionContext* context, std::unique_ptr<WTF::Closure> allowed, std::unique_ptr<WTF::Closure> denied)
{
    if (!client()) {
        (*denied)();
        return;
    }
    if (!context->isDocument()) {
        if (!client()->requestFileSystemAccessSync(context)) {
            (*denied)();
            return;
        }
        (*allowed)();
        return;
    }
    client()->requestFileSystemAccessAsync(context, ContentSettingCallbacks::create(std::move(allowed), std::move(denied)));
}

void LocalFileSystem::fileSystemNotAvailable(
    ExecutionContext* context,
    CallbackWrapper* callbacks)
{
    context->postTask(BLINK_FROM_HERE, createSameThreadTask(&reportFailure, passed(callbacks->release()), FileError::ABORT_ERR));
}

void LocalFileSystem::fileSystemNotAllowedInternal(
    ExecutionContext* context,
    CallbackWrapper* callbacks)
{
    context->postTask(BLINK_FROM_HERE, createSameThreadTask(&reportFailure, passed(callbacks->release()), FileError::ABORT_ERR));
}

void LocalFileSystem::fileSystemAllowedInternal(
    ExecutionContext* context,
    FileSystemType type,
    CallbackWrapper* callbacks)
{
    WebFileSystem* fileSystem = getFileSystem();
    if (!fileSystem) {
        fileSystemNotAvailable(context, callbacks);
        return;
    }
    KURL storagePartition = KURL(KURL(), context->getSecurityOrigin()->toString());
    fileSystem->openFileSystem(storagePartition, static_cast<WebFileSystemType>(type), callbacks->release());
}

void LocalFileSystem::resolveURLInternal(
    ExecutionContext* context,
    const KURL& fileSystemURL,
    CallbackWrapper* callbacks)
{
    WebFileSystem* fileSystem = getFileSystem();
    if (!fileSystem) {
        fileSystemNotAvailable(context, callbacks);
        return;
    }
    fileSystem->resolveURL(fileSystemURL, callbacks->release());
}

void LocalFileSystem::deleteFileSystemInternal(
    ExecutionContext* context,
    FileSystemType type,
    CallbackWrapper* callbacks)
{
    WebFileSystem* fileSystem = getFileSystem();
    if (!fileSystem) {
        fileSystemNotAvailable(context, callbacks);
        return;
    }
    KURL storagePartition = KURL(KURL(), context->getSecurityOrigin()->toString());
    fileSystem->deleteFileSystem(storagePartition, static_cast<WebFileSystemType>(type), callbacks->release());
}

LocalFileSystem::LocalFileSystem(std::unique_ptr<FileSystemClient> client)
    : m_client(std::move(client))
{
}

const char* LocalFileSystem::supplementName()
{
    return "LocalFileSystem";
}

LocalFileSystem* LocalFileSystem::from(ExecutionContext& context)
{
    if (context.isDocument())
        return static_cast<LocalFileSystem*>(Supplement<LocalFrame>::from(toDocument(context).frame(), supplementName()));

    WorkerClients* clients = toWorkerGlobalScope(context).clients();
    ASSERT(clients);
    return static_cast<LocalFileSystem*>(Supplement<WorkerClients>::from(clients, supplementName()));
}

void provideLocalFileSystemTo(LocalFrame& frame, std::unique_ptr<FileSystemClient> client)
{
    frame.provideSupplement(LocalFileSystem::supplementName(), LocalFileSystem::create(std::move(client)));
}

void provideLocalFileSystemToWorker(WorkerClients* clients, std::unique_ptr<FileSystemClient> client)
{
    clients->provideSupplement(LocalFileSystem::supplementName(), LocalFileSystem::create(std::move(client)));
}

} // namespace blink
