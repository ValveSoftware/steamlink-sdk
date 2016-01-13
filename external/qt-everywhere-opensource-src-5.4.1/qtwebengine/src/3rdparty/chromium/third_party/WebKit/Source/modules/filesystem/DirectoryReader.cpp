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
#include "modules/filesystem/DirectoryReader.h"

#include "core/fileapi/FileError.h"
#include "modules/filesystem/EntriesCallback.h"
#include "modules/filesystem/Entry.h"
#include "modules/filesystem/ErrorCallback.h"

namespace WebCore {

class DirectoryReader::EntriesCallbackHelper : public EntriesCallback {
public:
    explicit EntriesCallbackHelper(DirectoryReader* reader)
        : m_reader(reader)
    {
    }

    virtual void handleEvent(const EntryHeapVector& entries) OVERRIDE
    {
        m_reader->addEntries(entries);
    }

private:
    // FIXME: This Persistent keeps the reader alive until all of the readDirectory results are received. crbug.com/350285
    Persistent<DirectoryReader> m_reader;
};

class DirectoryReader::ErrorCallbackHelper : public ErrorCallback {
public:
    explicit ErrorCallbackHelper(DirectoryReader* reader)
        : m_reader(reader)
    {
    }

    virtual void handleEvent(FileError* error) OVERRIDE
    {
        m_reader->onError(error);
    }

private:
    Persistent<DirectoryReader> m_reader;
};

DirectoryReader::DirectoryReader(DOMFileSystemBase* fileSystem, const String& fullPath)
    : DirectoryReaderBase(fileSystem, fullPath)
    , m_isReading(false)
{
    ScriptWrappable::init(this);
}

DirectoryReader::~DirectoryReader()
{
}

void DirectoryReader::readEntries(PassOwnPtr<EntriesCallback> entriesCallback, PassOwnPtr<ErrorCallback> errorCallback)
{
    if (!m_isReading) {
        m_isReading = true;
        filesystem()->readDirectory(this, m_fullPath, adoptPtr(new EntriesCallbackHelper(this)), adoptPtr(new ErrorCallbackHelper(this)));
    }

    if (m_error) {
        filesystem()->scheduleCallback(errorCallback, PassRefPtrWillBeRawPtr<FileError>(m_error.get()));
        return;
    }

    if (m_entriesCallback) {
        // Non-null m_entriesCallback means multiple readEntries() calls are made concurrently. We don't allow doing it.
        filesystem()->scheduleCallback(errorCallback, FileError::create(FileError::INVALID_STATE_ERR));
        return;
    }

    if (!m_hasMoreEntries || !m_entries.isEmpty()) {
        filesystem()->scheduleCallback(entriesCallback, m_entries);
        m_entries.clear();
        return;
    }

    m_entriesCallback = entriesCallback;
    m_errorCallback = errorCallback;
}

void DirectoryReader::addEntries(const EntryHeapVector& entries)
{
    m_entries.appendVector(entries);
    m_errorCallback = nullptr;
    if (m_entriesCallback) {
        OwnPtr<EntriesCallback> entriesCallback = m_entriesCallback.release();
        EntryHeapVector entries;
        entries.swap(m_entries);
        entriesCallback->handleEvent(entries);
    }
}

void DirectoryReader::onError(FileError* error)
{
    m_error = error;
    m_entriesCallback = nullptr;
    if (m_errorCallback) {
        OwnPtr<ErrorCallback> errorCallback = m_errorCallback.release();
        errorCallback->handleEvent(error);
    }
}

void DirectoryReader::trace(Visitor* visitor)
{
    visitor->trace(m_entries);
    visitor->trace(m_error);
    DirectoryReaderBase::trace(visitor);
}

}
