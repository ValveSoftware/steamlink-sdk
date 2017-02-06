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

#include "core/html/FormData.h"

#include "core/fileapi/Blob.h"
#include "core/fileapi/File.h"
#include "core/frame/UseCounter.h"
#include "core/html/HTMLFormElement.h"
#include "platform/network/FormDataEncoder.h"
#include "platform/text/LineEnding.h"
#include "wtf/text/WTFString.h"

namespace blink {

namespace {

class FormDataIterationSource final : public PairIterable<String, FormDataEntryValue>::IterationSource {
public:
    FormDataIterationSource(FormData* formData) : m_formData(formData), m_current(0) { }

    bool next(ScriptState* scriptState, String& name, FormDataEntryValue& value, ExceptionState& exceptionState) override
    {
        if (m_current >= m_formData->size())
            return false;

        const FormData::Entry& entry = *m_formData->entries()[m_current++];
        name = m_formData->decode(entry.name());
        if (entry.isString()) {
            value.setUSVString(m_formData->decode(entry.value()));
        } else {
            ASSERT(entry.isFile());
            value.setFile(entry.file());
        }
        return true;
    }

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        visitor->trace(m_formData);
        PairIterable<String, FormDataEntryValue>::IterationSource::trace(visitor);
    }

private:
    const Member<FormData> m_formData;
    size_t m_current;
};

} // namespace

FormData::FormData(const WTF::TextEncoding& encoding)
    : m_encoding(encoding)
{
}

FormData::FormData(HTMLFormElement* form)
    : m_encoding(UTF8Encoding())
{
    if (!form)
        return;

    for (unsigned i = 0; i < form->associatedElements().size(); ++i) {
        FormAssociatedElement* element = form->associatedElements()[i];
        if (!toHTMLElement(element)->isDisabledFormControl())
            element->appendToFormData(*this);
    }
}

DEFINE_TRACE(FormData)
{
    visitor->trace(m_entries);
}

void FormData::append(const String& name, const String& value)
{
    m_entries.append(new Entry(encodeAndNormalize(name), encodeAndNormalize(value)));
}

void FormData::append(ExecutionContext* context, const String& name, Blob* blob, const String& filename)
{
    if (blob) {
        if (blob->isFile()) {
            if (filename.isNull())
                UseCounter::count(context, UseCounter::FormDataAppendFile);
            else
                UseCounter::count(context, UseCounter::FormDataAppendFileWithFilename);
        } else {
            if (filename.isNull())
                UseCounter::count(context, UseCounter::FormDataAppendBlob);
            else
                UseCounter::count(context, UseCounter::FormDataAppendBlobWithFilename);
        }
    } else {
        UseCounter::count(context, UseCounter::FormDataAppendNull);
    }
    append(name, blob, filename);
}

void FormData::deleteEntry(const String& name)
{
    const CString encodedName = encodeAndNormalize(name);
    size_t i = 0;
    while (i < m_entries.size()) {
        if (m_entries[i]->name() == encodedName) {
            m_entries.remove(i);
        } else {
            ++i;
        }
    }
}

void FormData::get(const String& name, FormDataEntryValue& result)
{
    const CString encodedName = encodeAndNormalize(name);
    for (const auto& entry : entries()) {
        if (entry->name() == encodedName) {
            if (entry->isString()) {
                result.setUSVString(decode(entry->value()));
            } else {
                ASSERT(entry->isFile());
                result.setFile(entry->file());
            }
            return;
        }
    }
}

HeapVector<FormDataEntryValue> FormData::getAll(const String& name)
{
    HeapVector<FormDataEntryValue> results;

    const CString encodedName = encodeAndNormalize(name);
    for (const auto& entry : entries()) {
        if (entry->name() != encodedName)
            continue;
        FormDataEntryValue value;
        if (entry->isString()) {
            value.setUSVString(decode(entry->value()));
        } else {
            ASSERT(entry->isFile());
            value.setFile(entry->file());
        }
        results.append(value);
    }
    return results;
}

bool FormData::has(const String& name)
{
    const CString encodedName = encodeAndNormalize(name);
    for (const auto& entry : entries()) {
        if (entry->name() == encodedName)
            return true;
    }
    return false;
}

void FormData::set(const String& name, const String& value)
{
    setEntry(new Entry(encodeAndNormalize(name), encodeAndNormalize(value)));
}

void FormData::set(const String& name, Blob* blob, const String& filename)
{
    setEntry(new Entry(encodeAndNormalize(name), blob, filename));
}

void FormData::setEntry(const Entry* entry)
{
    ASSERT(entry);
    const CString encodedName = entry->name();
    bool found = false;
    size_t i = 0;
    while (i < m_entries.size()) {
        if (m_entries[i]->name() != encodedName) {
            ++i;
        } else if (found) {
            m_entries.remove(i);
        } else {
            found = true;
            m_entries[i] = entry;
            ++i;
        }
    }
    if (!found)
        m_entries.append(entry);
}

void FormData::append(const String& name, int value)
{
    append(name, String::number(value));
}

void FormData::append(const String& name, Blob* blob, const String& filename)
{
    m_entries.append(new Entry(encodeAndNormalize(name), blob, filename));
}

CString FormData::encodeAndNormalize(const String& string) const
{
    CString encodedString = m_encoding.encode(string, WTF::EntitiesForUnencodables);
    return normalizeLineEndingsToCRLF(encodedString);
}

String FormData::decode(const CString& data) const
{
    return encoding().decode(data.data(), data.length());
}

PassRefPtr<EncodedFormData> FormData::encodeFormData(EncodedFormData::EncodingType encodingType)
{
    RefPtr<EncodedFormData> formData = EncodedFormData::create();
    Vector<char> encodedData;
    for (const auto& entry : entries())
        FormDataEncoder::addKeyValuePairAsFormData(encodedData, entry->name(), entry->isFile() ? encodeAndNormalize(entry->file()->name()) : entry->value(), encodingType);
    formData->appendData(encodedData.data(), encodedData.size());
    return formData.release();
}

PassRefPtr<EncodedFormData> FormData::encodeMultiPartFormData()
{
    RefPtr<EncodedFormData> formData = EncodedFormData::create();
    formData->setBoundary(FormDataEncoder::generateUniqueBoundaryString());
    Vector<char> encodedData;
    for (const auto& entry : entries()) {
        Vector<char> header;
        FormDataEncoder::beginMultiPartHeader(header, formData->boundary().data(), entry->name());

        // If the current type is blob, then we also need to include the
        // filename.
        if (entry->blob()) {
            String name;
            if (entry->blob()->isFile()) {
                File* file = toFile(entry->blob());
                // For file blob, use the filename (or relative path if it is
                // present) as the name.
                name = file->webkitRelativePath().isEmpty() ? file->name() : file->webkitRelativePath();

                // If a filename is passed in FormData.append(), use it instead
                // of the file blob's name.
                if (!entry->filename().isNull())
                    name = entry->filename();
            } else {
                // For non-file blob, use the filename if it is passed in
                // FormData.append().
                if (!entry->filename().isNull())
                    name = entry->filename();
                else
                    name = "blob";
            }

            // We have to include the filename=".." part in the header, even if
            // the filename is empty.
            FormDataEncoder::addFilenameToMultiPartHeader(header, encoding(), name);

            // Add the content type if available, or "application/octet-stream"
            // otherwise (RFC 1867).
            String contentType;
            if (entry->blob()->type().isEmpty())
                contentType = "application/octet-stream";
            else
                contentType = entry->blob()->type();
            FormDataEncoder::addContentTypeToMultiPartHeader(header, contentType.latin1());
        }

        FormDataEncoder::finishMultiPartHeader(header);

        // Append body
        formData->appendData(header.data(), header.size());
        if (entry->blob()) {
            if (entry->blob()->hasBackingFile()) {
                File* file = toFile(entry->blob());
                // Do not add the file if the path is empty.
                if (!file->path().isEmpty())
                    formData->appendFile(file->path());
                if (!file->fileSystemURL().isEmpty())
                    formData->appendFileSystemURL(file->fileSystemURL());
            } else {
                formData->appendBlob(entry->blob()->uuid(), entry->blob()->blobDataHandle());
            }
        } else {
            formData->appendData(entry->value().data(), entry->value().length());
        }
        formData->appendData("\r\n", 2);
    }
    FormDataEncoder::addBoundaryToMultiPartHeader(encodedData, formData->boundary().data(), true);
    formData->appendData(encodedData.data(), encodedData.size());
    return formData.release();
}

PairIterable<String, FormDataEntryValue>::IterationSource* FormData::startIteration(ScriptState*, ExceptionState&)
{
    return new FormDataIterationSource(this);
}

// ----------------------------------------------------------------

DEFINE_TRACE(FormData::Entry)
{
    visitor->trace(m_blob);
}

File* FormData::Entry::file() const
{
    ASSERT(blob());
    // The spec uses the passed filename when inserting entries into the list.
    // Here, we apply the filename (if present) as an override when extracting
    // entries.
    // FIXME: Consider applying the name during insertion.

    if (blob()->isFile()) {
        File* file = toFile(blob());
        if (filename().isNull())
            return file;
        return file->clone(filename());
    }

    String filename = m_filename;
    if (filename.isNull())
        filename = "blob";
    return File::create(filename, currentTimeMS(), blob()->blobDataHandle());
}

} // namespace blink
