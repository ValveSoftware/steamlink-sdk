/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#ifndef DOMFileSystem_h
#define DOMFileSystem_h

#include "bindings/v8/ScriptWrappable.h"
#include "core/dom/ActiveDOMObject.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/ExecutionContextTask.h"
#include "modules/filesystem/DOMFileSystemBase.h"
#include "modules/filesystem/EntriesCallback.h"
#include "platform/heap/Handle.h"

namespace WebCore {

class DirectoryEntry;
class File;
class FileCallback;
class FileEntry;
class FileWriterCallback;

class DOMFileSystem FINAL : public DOMFileSystemBase, public ScriptWrappable, public ActiveDOMObject {
public:
    static DOMFileSystem* create(ExecutionContext*, const String& name, FileSystemType, const KURL& rootURL);

    // Creates a new isolated file system for the given filesystemId.
    static DOMFileSystem* createIsolatedFileSystem(ExecutionContext*, const String& filesystemId);

    DirectoryEntry* root();

    // DOMFileSystemBase overrides.
    virtual void addPendingCallbacks() OVERRIDE;
    virtual void removePendingCallbacks() OVERRIDE;
    virtual void reportError(PassOwnPtr<ErrorCallback>, PassRefPtrWillBeRawPtr<FileError>) OVERRIDE;

    // ActiveDOMObject overrides.
    virtual bool hasPendingActivity() const OVERRIDE;

    void createWriter(const FileEntry*, PassOwnPtr<FileWriterCallback>, PassOwnPtr<ErrorCallback>);
    void createFile(const FileEntry*, PassOwnPtr<FileCallback>, PassOwnPtr<ErrorCallback>);

    // Schedule a callback. This should not cross threads (should be called on the same context thread).
    // FIXME: move this to a more generic place.
    template <typename CB, typename CBArg>
    static void scheduleCallback(ExecutionContext*, PassOwnPtr<CB>, PassRefPtrWillBeRawPtr<CBArg>);

    template <typename CB, typename CBArg>
    static void scheduleCallback(ExecutionContext*, PassOwnPtr<CB>, CBArg*);

    template <typename CB, typename CBArg>
    static void scheduleCallback(ExecutionContext*, PassOwnPtr<CB>, const HeapVector<CBArg>&);

    template <typename CB, typename CBArg>
    static void scheduleCallback(ExecutionContext*, PassOwnPtr<CB>, const CBArg&);

    template <typename CB>
    static void scheduleCallback(ExecutionContext*, PassOwnPtr<CB>);

    template <typename CB, typename CBArg>
    void scheduleCallback(PassOwnPtr<CB> callback, PassRefPtrWillBeRawPtr<CBArg> callbackArg)
    {
        scheduleCallback(executionContext(), callback, callbackArg);
    }

    template <typename CB, typename CBArg>
    void scheduleCallback(PassOwnPtr<CB> callback, CBArg* callbackArg)
    {
        scheduleCallback(executionContext(), callback, callbackArg);
    }

    template <typename CB, typename CBArg>
    void scheduleCallback(PassOwnPtr<CB> callback, const CBArg& callbackArg)
    {
        scheduleCallback(executionContext(), callback, callbackArg);
    }

private:
    DOMFileSystem(ExecutionContext*, const String& name, FileSystemType, const KURL& rootURL);

    // A helper template to schedule a callback task.
    template <typename CB, typename CBArg>
    class DispatchCallbackRefPtrArgTask FINAL : public ExecutionContextTask {
    public:
        DispatchCallbackRefPtrArgTask(PassOwnPtr<CB> callback, PassRefPtrWillBeRawPtr<CBArg> arg)
            : m_callback(callback)
            , m_callbackArg(arg)
        {
        }

        virtual void performTask(ExecutionContext*) OVERRIDE
        {
            m_callback->handleEvent(m_callbackArg.get());
        }

    private:
        OwnPtr<CB> m_callback;
        RefPtrWillBePersistent<CBArg> m_callbackArg;
    };

    template <typename CB, typename CBArg>
    class DispatchCallbackPtrArgTask FINAL : public ExecutionContextTask {
    public:
        DispatchCallbackPtrArgTask(PassOwnPtr<CB> callback, CBArg* arg)
            : m_callback(callback)
            , m_callbackArg(arg)
        {
        }

        virtual void performTask(ExecutionContext*) OVERRIDE
        {
            m_callback->handleEvent(m_callbackArg.get());
        }

    private:
        OwnPtr<CB> m_callback;
        Persistent<CBArg> m_callbackArg;
    };

    template <typename CB, typename CBArg>
    class DispatchCallbackNonPtrArgTask FINAL : public ExecutionContextTask {
    public:
        DispatchCallbackNonPtrArgTask(PassOwnPtr<CB> callback, const CBArg& arg)
            : m_callback(callback)
            , m_callbackArg(arg)
        {
        }

        virtual void performTask(ExecutionContext*) OVERRIDE
        {
            m_callback->handleEvent(m_callbackArg);
        }

    private:
        OwnPtr<CB> m_callback;
        CBArg m_callbackArg;
    };

    template <typename CB>
    class DispatchCallbackNoArgTask FINAL : public ExecutionContextTask {
    public:
        DispatchCallbackNoArgTask(PassOwnPtr<CB> callback)
            : m_callback(callback)
        {
        }

        virtual void performTask(ExecutionContext*) OVERRIDE
        {
            m_callback->handleEvent();
        }

    private:
        OwnPtr<CB> m_callback;
    };

    int m_numberOfPendingCallbacks;
};

template <typename CB, typename CBArg>
void DOMFileSystem::scheduleCallback(ExecutionContext* executionContext, PassOwnPtr<CB> callback, PassRefPtrWillBeRawPtr<CBArg> arg)
{
    ASSERT(executionContext->isContextThread());
    if (callback)
        executionContext->postTask(adoptPtr(new DispatchCallbackRefPtrArgTask<CB, CBArg>(callback, arg)));
}

template <typename CB, typename CBArg>
void DOMFileSystem::scheduleCallback(ExecutionContext* executionContext, PassOwnPtr<CB> callback, CBArg* arg)
{
    ASSERT(executionContext->isContextThread());
    if (callback)
        executionContext->postTask(adoptPtr(new DispatchCallbackPtrArgTask<CB, CBArg>(callback, arg)));
}

template <typename CB, typename CBArg>
void DOMFileSystem::scheduleCallback(ExecutionContext* executionContext, PassOwnPtr<CB> callback, const HeapVector<CBArg>& arg)
{
    ASSERT(executionContext->isContextThread());
    if (callback)
        executionContext->postTask(adoptPtr(new DispatchCallbackNonPtrArgTask<CB, PersistentHeapVector<CBArg> >(callback, arg)));
}

template <typename CB, typename CBArg>
void DOMFileSystem::scheduleCallback(ExecutionContext* executionContext, PassOwnPtr<CB> callback, const CBArg& arg)
{
    ASSERT(executionContext->isContextThread());
    if (callback)
        executionContext->postTask(adoptPtr(new DispatchCallbackNonPtrArgTask<CB, CBArg>(callback, arg)));
}

template <typename CB>
void DOMFileSystem::scheduleCallback(ExecutionContext* executionContext, PassOwnPtr<CB> callback)
{
    ASSERT(executionContext->isContextThread());
    if (callback)
        executionContext->postTask(adoptPtr(new DispatchCallbackNoArgTask<CB>(callback)));
}

} // namespace

#endif // DOMFileSystem_h
