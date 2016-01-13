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

#ifndef Entry_h
#define Entry_h

#include "bindings/v8/ScriptWrappable.h"
#include "modules/filesystem/DOMFileSystem.h"
#include "modules/filesystem/EntryBase.h"
#include "platform/heap/Handle.h"

namespace WebCore {

class DirectoryEntry;
class EntryCallback;
class EntrySync;
class ErrorCallback;
class MetadataCallback;
class VoidCallback;

class Entry : public EntryBase, public ScriptWrappable {
public:
    DOMFileSystem* filesystem() const { return static_cast<DOMFileSystem*>(m_fileSystem.get()); }

    void getMetadata(PassOwnPtr<MetadataCallback> successCallback = nullptr, PassOwnPtr<ErrorCallback> = nullptr);
    void moveTo(DirectoryEntry* parent, const String& name = String(), PassOwnPtr<EntryCallback> successCallback = nullptr, PassOwnPtr<ErrorCallback> = nullptr) const;
    void copyTo(DirectoryEntry* parent, const String& name = String(), PassOwnPtr<EntryCallback> successCallback = nullptr, PassOwnPtr<ErrorCallback> = nullptr) const;
    void remove(PassOwnPtr<VoidCallback> successCallback = nullptr, PassOwnPtr<ErrorCallback> = nullptr) const;
    void getParent(PassOwnPtr<EntryCallback> successCallback = nullptr, PassOwnPtr<ErrorCallback> = nullptr) const;

    virtual void trace(Visitor*) OVERRIDE;

protected:
    Entry(DOMFileSystemBase*, const String& fullPath);
};

} // namespace WebCore

#endif // Entry_h
