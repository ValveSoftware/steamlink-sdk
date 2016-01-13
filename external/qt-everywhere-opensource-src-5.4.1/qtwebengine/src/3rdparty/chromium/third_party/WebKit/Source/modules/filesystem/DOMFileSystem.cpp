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
#include "modules/filesystem/DOMFileSystem.h"

#include "core/fileapi/File.h"
#include "modules/filesystem/DOMFilePath.h"
#include "modules/filesystem/DirectoryEntry.h"
#include "modules/filesystem/ErrorCallback.h"
#include "modules/filesystem/FileCallback.h"
#include "modules/filesystem/FileEntry.h"
#include "modules/filesystem/FileSystemCallbacks.h"
#include "modules/filesystem/FileWriter.h"
#include "modules/filesystem/FileWriterBaseCallback.h"
#include "modules/filesystem/FileWriterCallback.h"
#include "modules/filesystem/MetadataCallback.h"
#include "platform/FileMetadata.h"
#include "platform/weborigin/DatabaseIdentifier.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/WebFileSystem.h"
#include "public/platform/WebFileSystemCallbacks.h"
#include "wtf/OwnPtr.h"
#include "wtf/text/StringBuilder.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

// static
DOMFileSystem* DOMFileSystem::create(ExecutionContext* context, const String& name, FileSystemType type, const KURL& rootURL)
{
    DOMFileSystem* fileSystem(new DOMFileSystem(context, name, type, rootURL));
    fileSystem->suspendIfNeeded();
    return fileSystem;
}

DOMFileSystem* DOMFileSystem::createIsolatedFileSystem(ExecutionContext* context, const String& filesystemId)
{
    if (filesystemId.isEmpty())
        return 0;

    StringBuilder filesystemName;
    filesystemName.append(createDatabaseIdentifierFromSecurityOrigin(context->securityOrigin()));
    filesystemName.append(":Isolated_");
    filesystemName.append(filesystemId);

    // The rootURL created here is going to be attached to each filesystem request and
    // is to be validated each time the request is being handled.
    StringBuilder rootURL;
    rootURL.append("filesystem:");
    rootURL.append(context->securityOrigin()->toString());
    rootURL.append("/");
    rootURL.append(isolatedPathPrefix);
    rootURL.append("/");
    rootURL.append(filesystemId);
    rootURL.append("/");

    return DOMFileSystem::create(context, filesystemName.toString(), FileSystemTypeIsolated, KURL(ParsedURLString, rootURL.toString()));
}

DOMFileSystem::DOMFileSystem(ExecutionContext* context, const String& name, FileSystemType type, const KURL& rootURL)
    : DOMFileSystemBase(context, name, type, rootURL)
    , ActiveDOMObject(context)
    , m_numberOfPendingCallbacks(0)
{
    ScriptWrappable::init(this);
}

DirectoryEntry* DOMFileSystem::root()
{
    return DirectoryEntry::create(this, DOMFilePath::root);
}

void DOMFileSystem::addPendingCallbacks()
{
    ++m_numberOfPendingCallbacks;
}

void DOMFileSystem::removePendingCallbacks()
{
    ASSERT(m_numberOfPendingCallbacks > 0);
    --m_numberOfPendingCallbacks;
}

bool DOMFileSystem::hasPendingActivity() const
{
    ASSERT(m_numberOfPendingCallbacks >= 0);
    return m_numberOfPendingCallbacks;
}

void DOMFileSystem::reportError(PassOwnPtr<ErrorCallback> errorCallback, PassRefPtrWillBeRawPtr<FileError> fileError)
{
    scheduleCallback(errorCallback, fileError);
}

namespace {

class ConvertToFileWriterCallback : public FileWriterBaseCallback {
public:
    static PassOwnPtr<ConvertToFileWriterCallback> create(PassOwnPtr<FileWriterCallback> callback)
    {
        return adoptPtr(new ConvertToFileWriterCallback(callback));
    }

    void handleEvent(FileWriterBase* fileWriterBase)
    {
        m_callback->handleEvent(static_cast<FileWriter*>(fileWriterBase));
    }
private:
    ConvertToFileWriterCallback(PassOwnPtr<FileWriterCallback> callback)
        : m_callback(callback)
    {
    }
    OwnPtr<FileWriterCallback> m_callback;
};

}

void DOMFileSystem::createWriter(const FileEntry* fileEntry, PassOwnPtr<FileWriterCallback> successCallback, PassOwnPtr<ErrorCallback> errorCallback)
{
    ASSERT(fileEntry);

    FileWriter* fileWriter = FileWriter::create(executionContext());
    OwnPtr<FileWriterBaseCallback> conversionCallback = ConvertToFileWriterCallback::create(successCallback);
    OwnPtr<AsyncFileSystemCallbacks> callbacks = FileWriterBaseCallbacks::create(fileWriter, conversionCallback.release(), errorCallback, m_context);
    fileSystem()->createFileWriter(createFileSystemURL(fileEntry), fileWriter, callbacks.release());
}

namespace {

class SnapshotFileCallback : public FileSystemCallbacksBase {
public:
    static PassOwnPtr<AsyncFileSystemCallbacks> create(DOMFileSystem* filesystem, const String& name, const KURL& url, PassOwnPtr<FileCallback> successCallback, PassOwnPtr<ErrorCallback> errorCallback, ExecutionContext* context)
    {
        return adoptPtr(static_cast<AsyncFileSystemCallbacks*>(new SnapshotFileCallback(filesystem, name, url, successCallback, errorCallback, context)));
    }

    virtual void didCreateSnapshotFile(const FileMetadata& metadata, PassRefPtr<BlobDataHandle> snapshot)
    {
        if (!m_successCallback)
            return;

        // We can't directly use the snapshot blob data handle because the content type on it hasn't been set.
        // The |snapshot| param is here to provide a a chain of custody thru thread bridging that is held onto until
        // *after* we've coined a File with a new handle that has the correct type set on it. This allows the
        // blob storage system to track when a temp file can and can't be safely deleted.

        // For regular filesystem types (temporary or persistent), we should not cache file metadata as it could change File semantics.
        // For other filesystem types (which could be platform-specific ones), there's a chance that the files are on remote filesystem. If the port has returned metadata just pass it to File constructor (so we may cache the metadata).
        // FIXME: We should use the snapshot metadata for all files.
        // https://www.w3.org/Bugs/Public/show_bug.cgi?id=17746
        if (m_fileSystem->type() == FileSystemTypeTemporary || m_fileSystem->type() == FileSystemTypePersistent) {
            m_successCallback->handleEvent(File::createWithName(metadata.platformPath, m_name).get());
        } else if (!metadata.platformPath.isEmpty()) {
            // If the platformPath in the returned metadata is given, we create a File object for the path.
            m_successCallback->handleEvent(File::createForFileSystemFile(m_name, metadata).get());
        } else {
            // Otherwise create a File from the FileSystem URL.
            m_successCallback->handleEvent(File::createForFileSystemFile(m_url, metadata).get());
        }

        m_successCallback.release();
    }

private:
    SnapshotFileCallback(DOMFileSystem* filesystem, const String& name, const KURL& url, PassOwnPtr<FileCallback> successCallback, PassOwnPtr<ErrorCallback> errorCallback, ExecutionContext* context)
        : FileSystemCallbacksBase(errorCallback, filesystem, context)
        , m_name(name)
        , m_url(url)
        , m_successCallback(successCallback)
    {
    }

    String m_name;
    KURL m_url;
    OwnPtr<FileCallback> m_successCallback;
};

} // namespace

void DOMFileSystem::createFile(const FileEntry* fileEntry, PassOwnPtr<FileCallback> successCallback, PassOwnPtr<ErrorCallback> errorCallback)
{
    KURL fileSystemURL = createFileSystemURL(fileEntry);
    fileSystem()->createSnapshotFileAndReadMetadata(fileSystemURL, SnapshotFileCallback::create(this, fileEntry->name(), fileSystemURL, successCallback, errorCallback, m_context));
}

} // namespace WebCore
