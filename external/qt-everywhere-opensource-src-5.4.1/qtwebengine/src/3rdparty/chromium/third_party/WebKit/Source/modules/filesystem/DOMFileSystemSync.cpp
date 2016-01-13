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

#include "config.h"
#include "modules/filesystem/DOMFileSystemSync.h"

#include "bindings/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"
#include "core/fileapi/File.h"
#include "core/fileapi/FileError.h"
#include "modules/filesystem/DOMFilePath.h"
#include "modules/filesystem/DirectoryEntrySync.h"
#include "modules/filesystem/ErrorCallback.h"
#include "modules/filesystem/FileEntrySync.h"
#include "modules/filesystem/FileSystemCallbacks.h"
#include "modules/filesystem/FileWriterBaseCallback.h"
#include "modules/filesystem/FileWriterSync.h"
#include "platform/FileMetadata.h"
#include "public/platform/WebFileSystem.h"
#include "public/platform/WebFileSystemCallbacks.h"

namespace WebCore {

class FileWriterBase;

DOMFileSystemSync* DOMFileSystemSync::create(DOMFileSystemBase* fileSystem)
{
    return new DOMFileSystemSync(fileSystem->m_context, fileSystem->name(), fileSystem->type(), fileSystem->rootURL());
}

DOMFileSystemSync::DOMFileSystemSync(ExecutionContext* context, const String& name, FileSystemType type, const KURL& rootURL)
    : DOMFileSystemBase(context, name, type, rootURL)
{
    ScriptWrappable::init(this);
}

DOMFileSystemSync::~DOMFileSystemSync()
{
}

void DOMFileSystemSync::reportError(PassOwnPtr<ErrorCallback> errorCallback, PassRefPtrWillBeRawPtr<FileError> fileError)
{
    errorCallback->handleEvent(fileError.get());
}

DirectoryEntrySync* DOMFileSystemSync::root()
{
    return DirectoryEntrySync::create(this, DOMFilePath::root);
}

namespace {

class CreateFileHelper FINAL : public AsyncFileSystemCallbacks {
public:
    class CreateFileResult : public RefCountedWillBeGarbageCollected<CreateFileResult> {
      public:
        static PassRefPtrWillBeRawPtr<CreateFileResult> create()
        {
            return adoptRefWillBeNoop(new CreateFileResult());
        }

        bool m_failed;
        int m_code;
        RefPtrWillBeMember<File> m_file;

        void trace(Visitor* visitor)
        {
            visitor->trace(m_file);
        }

      private:
        CreateFileResult()
            : m_failed(false)
            , m_code(0)
        {
        }

#if !ENABLE(OILPAN)
        ~CreateFileResult()
        {
        }
#endif

        friend class RefCountedWillBeGarbageCollected<CreateFileResult>;
    };

    static PassOwnPtr<AsyncFileSystemCallbacks> create(PassRefPtrWillBeRawPtr<CreateFileResult> result, const String& name, const KURL& url, FileSystemType type)
    {
        return adoptPtr(static_cast<AsyncFileSystemCallbacks*>(new CreateFileHelper(result, name, url, type)));
    }

    virtual void didFail(int code) OVERRIDE
    {
        m_result->m_failed = true;
        m_result->m_code = code;
    }

    virtual ~CreateFileHelper()
    {
    }

    virtual void didCreateSnapshotFile(const FileMetadata& metadata, PassRefPtr<BlobDataHandle> snapshot) OVERRIDE
    {
        // We can't directly use the snapshot blob data handle because the content type on it hasn't been set.
        // The |snapshot| param is here to provide a a chain of custody thru thread bridging that is held onto until
        // *after* we've coined a File with a new handle that has the correct type set on it. This allows the
        // blob storage system to track when a temp file can and can't be safely deleted.

        // For regular filesystem types (temporary or persistent), we should not cache file metadata as it could change File semantics.
        // For other filesystem types (which could be platform-specific ones), there's a chance that the files are on remote filesystem.
        // If the port has returned metadata just pass it to File constructor (so we may cache the metadata).
        // FIXME: We should use the snapshot metadata for all files.
        // https://www.w3.org/Bugs/Public/show_bug.cgi?id=17746
        if (m_type == FileSystemTypeTemporary || m_type == FileSystemTypePersistent) {
            m_result->m_file = File::createWithName(metadata.platformPath, m_name);
        } else if (!metadata.platformPath.isEmpty()) {
            // If the platformPath in the returned metadata is given, we create a File object for the path.
            m_result->m_file = File::createForFileSystemFile(m_name, metadata).get();
        } else {
            // Otherwise create a File from the FileSystem URL.
            m_result->m_file = File::createForFileSystemFile(m_url, metadata).get();
        }
    }

    virtual bool shouldBlockUntilCompletion() const OVERRIDE
    {
        return true;
    }

private:
    CreateFileHelper(PassRefPtrWillBeRawPtr<CreateFileResult> result, const String& name, const KURL& url, FileSystemType type)
        : m_result(result)
        , m_name(name)
        , m_url(url)
        , m_type(type)
    {
    }

    RefPtrWillBePersistent<CreateFileResult> m_result;
    String m_name;
    KURL m_url;
    FileSystemType m_type;
};

} // namespace

PassRefPtrWillBeRawPtr<File> DOMFileSystemSync::createFile(const FileEntrySync* fileEntry, ExceptionState& exceptionState)
{
    KURL fileSystemURL = createFileSystemURL(fileEntry);
    RefPtrWillBeRawPtr<CreateFileHelper::CreateFileResult> result(CreateFileHelper::CreateFileResult::create());
    fileSystem()->createSnapshotFileAndReadMetadata(fileSystemURL, CreateFileHelper::create(result, fileEntry->name(), fileSystemURL, type()));
    if (result->m_failed) {
        exceptionState.throwDOMException(result->m_code, "Could not create '" + fileEntry->name() + "'.");
        return nullptr;
    }
    return result->m_file.get();
}

namespace {

class ReceiveFileWriterCallback FINAL : public FileWriterBaseCallback {
public:
    static PassOwnPtr<ReceiveFileWriterCallback> create()
    {
        return adoptPtr(new ReceiveFileWriterCallback());
    }

    virtual void handleEvent(FileWriterBase*) OVERRIDE
    {
    }

private:
    ReceiveFileWriterCallback()
    {
    }
};

class LocalErrorCallback FINAL : public ErrorCallback {
public:
    static PassOwnPtr<LocalErrorCallback> create(FileError::ErrorCode& errorCode)
    {
        return adoptPtr(new LocalErrorCallback(errorCode));
    }

    virtual void handleEvent(FileError* error) OVERRIDE
    {
        ASSERT(error->code() != FileError::OK);
        m_errorCode = error->code();
    }

private:
    explicit LocalErrorCallback(FileError::ErrorCode& errorCode)
        : m_errorCode(errorCode)
    {
    }

    FileError::ErrorCode& m_errorCode;
};

}

FileWriterSync* DOMFileSystemSync::createWriter(const FileEntrySync* fileEntry, ExceptionState& exceptionState)
{
    ASSERT(fileEntry);

    FileWriterSync* fileWriter = FileWriterSync::create();
    OwnPtr<ReceiveFileWriterCallback> successCallback = ReceiveFileWriterCallback::create();
    FileError::ErrorCode errorCode = FileError::OK;
    OwnPtr<LocalErrorCallback> errorCallback = LocalErrorCallback::create(errorCode);

    OwnPtr<AsyncFileSystemCallbacks> callbacks = FileWriterBaseCallbacks::create(fileWriter, successCallback.release(), errorCallback.release(), m_context);
    callbacks->setShouldBlockUntilCompletion(true);

    fileSystem()->createFileWriter(createFileSystemURL(fileEntry), fileWriter, callbacks.release());
    if (errorCode != FileError::OK) {
        FileError::throwDOMException(exceptionState, errorCode);
        return 0;
    }
    return fileWriter;
}

}
