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

#include "bindings/core/v8/ActiveScriptWrappable.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "core/dom/ActiveDOMObject.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/ExecutionContextTask.h"
#include "modules/ModulesExport.h"
#include "modules/filesystem/DOMFileSystemBase.h"
#include "modules/filesystem/EntriesCallback.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebTraceLocation.h"
#include "wtf/PtrUtil.h"

namespace blink {

class DirectoryEntry;
class BlobCallback;
class FileEntry;
class FileWriterCallback;

class MODULES_EXPORT DOMFileSystem final : public DOMFileSystemBase, public ScriptWrappable, public ActiveScriptWrappable, public ActiveDOMObject {
    DEFINE_WRAPPERTYPEINFO();
    USING_GARBAGE_COLLECTED_MIXIN(DOMFileSystem);
public:
    static DOMFileSystem* create(ExecutionContext*, const String& name, FileSystemType, const KURL& rootURL);

    // Creates a new isolated file system for the given filesystemId.
    static DOMFileSystem* createIsolatedFileSystem(ExecutionContext*, const String& filesystemId);

    DirectoryEntry* root() const;

    // DOMFileSystemBase overrides.
    void addPendingCallbacks() override;
    void removePendingCallbacks() override;
    void reportError(ErrorCallback*, FileError*) override;

    static void reportError(ExecutionContext*, ErrorCallback*, FileError*);

    // ActiveScriptWrappable overrides.
    bool hasPendingActivity() const final;

    void createWriter(const FileEntry*, FileWriterCallback*, ErrorCallback*);
    void createFile(const FileEntry*, BlobCallback*, ErrorCallback*);

    // Schedule a callback. This should not cross threads (should be called on the same context thread).
    static void scheduleCallback(ExecutionContext* executionContext, std::unique_ptr<ExecutionContextTask> task)
    {
        DCHECK(executionContext->isContextThread());
        executionContext->postTask(BLINK_FROM_HERE, std::move(task), taskNameForInstrumentation());
    }

    DECLARE_VIRTUAL_TRACE();

private:
    DOMFileSystem(ExecutionContext*, const String& name, FileSystemType, const KURL& rootURL);

    static String taskNameForInstrumentation()
    {
        return "FileSystem";
    }

    int m_numberOfPendingCallbacks;
    Member<DirectoryEntry> m_rootEntry;
};

} // namespace blink

#endif // DOMFileSystem_h
