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
#include "modules/filesystem/Entry.h"

#include "core/dom/ExecutionContext.h"
#include "core/fileapi/FileError.h"
#include "core/frame/UseCounter.h"
#include "core/html/VoidCallback.h"
#include "modules/filesystem/DirectoryEntry.h"
#include "modules/filesystem/EntryCallback.h"
#include "modules/filesystem/ErrorCallback.h"
#include "modules/filesystem/FileSystemCallbacks.h"
#include "modules/filesystem/MetadataCallback.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "wtf/text/StringBuilder.h"

namespace blink {

Entry::Entry(DOMFileSystemBase* fileSystem, const String& fullPath)
    : EntryBase(fileSystem, fullPath)
{
}

DOMFileSystem* Entry::filesystem(ExecutionContext* context) const
{
    if (m_fileSystem->type() == FileSystemTypeIsolated)
        UseCounter::count(context, UseCounter::Entry_Filesystem_AttributeGetter_IsolatedFileSystem);
    return filesystem();
}

void Entry::getMetadata(ExecutionContext* context, MetadataCallback* successCallback, ErrorCallback* errorCallback)
{
    if (m_fileSystem->type() == FileSystemTypeIsolated)
        UseCounter::count(context, UseCounter::Entry_GetMetadata_Method_IsolatedFileSystem);
    m_fileSystem->getMetadata(this, successCallback, errorCallback);
}

void Entry::moveTo(ExecutionContext* context, DirectoryEntry* parent, const String& name, EntryCallback* successCallback, ErrorCallback* errorCallback) const
{
    if (m_fileSystem->type() == FileSystemTypeIsolated)
        UseCounter::count(context, UseCounter::Entry_MoveTo_Method_IsolatedFileSystem);
    m_fileSystem->move(this, parent, name, successCallback, errorCallback);
}

void Entry::copyTo(ExecutionContext* context, DirectoryEntry* parent, const String& name, EntryCallback* successCallback, ErrorCallback* errorCallback) const
{
    if (m_fileSystem->type() == FileSystemTypeIsolated)
        UseCounter::count(context, UseCounter::Entry_CopyTo_Method_IsolatedFileSystem);
    m_fileSystem->copy(this, parent, name, successCallback, errorCallback);
}

void Entry::remove(ExecutionContext* context, VoidCallback* successCallback, ErrorCallback* errorCallback) const
{
    if (m_fileSystem->type() == FileSystemTypeIsolated)
        UseCounter::count(context, UseCounter::Entry_Remove_Method_IsolatedFileSystem);
    m_fileSystem->remove(this, successCallback, errorCallback);
}

void Entry::getParent(ExecutionContext* context, EntryCallback* successCallback, ErrorCallback* errorCallback) const
{
    if (m_fileSystem->type() == FileSystemTypeIsolated)
        UseCounter::count(context, UseCounter::Entry_GetParent_Method_IsolatedFileSystem);
    m_fileSystem->getParent(this, successCallback, errorCallback);
}

String Entry::toURL(ExecutionContext* context) const
{
    if (m_fileSystem->type() == FileSystemTypeIsolated)
        UseCounter::count(context, UseCounter::Entry_ToURL_Method_IsolatedFileSystem);
    return static_cast<const EntryBase*>(this)->toURL();
}

DEFINE_TRACE(Entry)
{
    EntryBase::trace(visitor);
}

} // namespace blink
