/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef File_h
#define File_h

#include "core/fileapi/Blob.h"
#include "platform/heap/Handle.h"
#include "wtf/PassRefPtr.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class ExceptionState;
class ExecutionContext;
struct FileMetadata;
class KURL;

class File FINAL : public Blob {
public:
    // AllContentTypes should only be used when the full path/name are trusted; otherwise, it could
    // allow arbitrary pages to determine what applications an user has installed.
    enum ContentTypeLookupPolicy {
        WellKnownContentTypes,
        AllContentTypes,
    };

    static PassRefPtrWillBeRawPtr<File> create(const String& path, ContentTypeLookupPolicy policy = WellKnownContentTypes)
    {
        return adoptRefWillBeNoop(new File(path, policy));
    }

    static PassRefPtrWillBeRawPtr<File> create(const String& name, double modificationTime, PassRefPtr<BlobDataHandle> blobDataHandle)
    {
        return adoptRefWillBeNoop(new File(name, modificationTime, blobDataHandle));
    }

    // For deserialization.
    static PassRefPtrWillBeRawPtr<File> create(const String& path, const String& name, const String& relativePath, bool hasSnaphotData, uint64_t size, double lastModified, PassRefPtr<BlobDataHandle> blobDataHandle)
    {
        return adoptRefWillBeNoop(new File(path, name, relativePath, hasSnaphotData, size, lastModified, blobDataHandle));
    }
    static PassRefPtrWillBeRawPtr<File> create(const String& path, const String& name, uint64_t size, double lastModified, PassRefPtr<BlobDataHandle> blobDataHandle)
    {
        return adoptRefWillBeNoop(new File(path, name, String(), true, size, lastModified, blobDataHandle));
    }

    static PassRefPtrWillBeRawPtr<File> createWithRelativePath(const String& path, const String& relativePath);

    // If filesystem files live in the remote filesystem, the port might pass the valid metadata (whose length field is non-negative) and cache in the File object.
    //
    // Otherwise calling size(), lastModifiedTime() and slice() will synchronously query the file metadata.
    static PassRefPtrWillBeRawPtr<File> createForFileSystemFile(const String& name, const FileMetadata& metadata)
    {
        return adoptRefWillBeNoop(new File(name, metadata));
    }

    static PassRefPtrWillBeRawPtr<File> createForFileSystemFile(const KURL& url, const FileMetadata& metadata)
    {
        return adoptRefWillBeNoop(new File(url, metadata));
    }

    KURL fileSystemURL() const { ASSERT(hasValidFileSystemURL()); return m_fileSystemURL; }

    // Create a file with a name exposed to the author (via File.name and associated DOM properties) that differs from the one provided in the path.
    static PassRefPtrWillBeRawPtr<File> createWithName(const String& path, const String& name, ContentTypeLookupPolicy policy = WellKnownContentTypes)
    {
        if (name.isEmpty())
            return adoptRefWillBeNoop(new File(path, policy));
        return adoptRefWillBeNoop(new File(path, name, policy));
    }

    virtual unsigned long long size() const OVERRIDE;
    virtual PassRefPtrWillBeRawPtr<Blob> slice(long long start, long long end, const String& contentType, ExceptionState&) const OVERRIDE;
    virtual void close(ExecutionContext*, ExceptionState&) OVERRIDE;

    virtual bool isFile() const OVERRIDE { return true; }
    virtual bool hasBackingFile() const OVERRIDE { return m_hasBackingFile; }

    virtual void appendTo(BlobData&) const OVERRIDE;

    const String& path() const { ASSERT(hasValidFilePath()); return m_path; }
    const String name() const { return m_name; }

    // This returns the current date and time if the file's last modification date is not known (per spec: http://www.w3.org/TR/FileAPI/#dfn-lastModifiedDate).
    double lastModifiedDate() const;

    // Returns the relative path of this file in the context of a directory selection.
    const String& webkitRelativePath() const { return m_relativePath; }

    // Note that this involves synchronous file operation. Think twice before calling this function.
    void captureSnapshot(long long& snapshotSize, double& snapshotModificationTime) const;

    // Returns true if this has a valid snapshot metadata (i.e. m_snapshotSize >= 0).
    bool hasValidSnapshotMetadata() const { return m_snapshotSize >= 0; }

private:
    File(const String& path, ContentTypeLookupPolicy);
    File(const String& path, const String& name, ContentTypeLookupPolicy);
    File(const String& path, const String& name, const String& relativePath, bool hasSnaphotData, uint64_t size, double lastModified, PassRefPtr<BlobDataHandle>);
    File(const String& name, double modificationTime, PassRefPtr<BlobDataHandle>);
    File(const String& name, const FileMetadata&);
    File(const KURL& fileSystemURL, const FileMetadata&);

    void invalidateSnapshotMetadata() { m_snapshotSize = -1; }

#ifndef NDEBUG
    bool hasValidFileSystemURL() const { return hasBackingFile(); }
    // Instances not backed by a file must have an empty path set.
    bool hasValidFilePath() const { return hasBackingFile() || m_path.isEmpty(); }
#endif

    bool m_hasBackingFile;
    String m_path;
    String m_name;

    KURL m_fileSystemURL;

    // If m_snapshotSize is negative (initialized to -1 by default), the snapshot metadata is invalid and we retrieve the latest metadata synchronously in size(), lastModifiedTime() and slice().
    // Otherwise, the snapshot metadata are used directly in those methods.
    long long m_snapshotSize;
    const double m_snapshotModificationTime;

    String m_relativePath;
};

DEFINE_TYPE_CASTS(File, Blob, blob, blob->isFile(), blob.isFile());

} // namespace WebCore

#endif // File_h
