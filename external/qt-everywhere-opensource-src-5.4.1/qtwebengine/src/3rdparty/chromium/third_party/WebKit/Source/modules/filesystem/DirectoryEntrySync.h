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

#ifndef DirectoryEntrySync_h
#define DirectoryEntrySync_h

#include "modules/filesystem/EntrySync.h"
#include "modules/filesystem/FileSystemFlags.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class DirectoryReaderSync;
class ExceptionState;
class FileEntrySync;

class DirectoryEntrySync FINAL : public EntrySync {
public:
    static DirectoryEntrySync* create(DOMFileSystemBase* fileSystem, const String& fullPath)
    {
        return new DirectoryEntrySync(fileSystem, fullPath);
    }
    virtual bool isDirectory() const OVERRIDE { return true; }

    DirectoryReaderSync* createReader();
    FileEntrySync* getFile(const String& path, const Dictionary&, ExceptionState&);
    DirectoryEntrySync* getDirectory(const String& path, const Dictionary&, ExceptionState&);
    void removeRecursively(ExceptionState&);

    virtual void trace(Visitor*) OVERRIDE;

private:
    DirectoryEntrySync(DOMFileSystemBase*, const String& fullPath);
};

DEFINE_TYPE_CASTS(DirectoryEntrySync, EntrySync, entry, entry->isDirectory(), entry.isDirectory());

}

#endif // DirectoryEntrySync_h
