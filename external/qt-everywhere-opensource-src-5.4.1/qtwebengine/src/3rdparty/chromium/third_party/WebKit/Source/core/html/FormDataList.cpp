/*
 * Copyright (C) 2005, 2006, 2008 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "core/html/FormDataList.h"

#include "core/fileapi/File.h"
#include "platform/network/FormDataBuilder.h"
#include "platform/text/LineEnding.h"

namespace WebCore {

FormDataList::FormDataList(const WTF::TextEncoding& c)
    : m_encoding(c)
{
}

void FormDataList::appendString(const String& string)
{
    CString encodedString = m_encoding.encode(string, WTF::EntitiesForUnencodables);
    m_items.append(normalizeLineEndingsToCRLF(encodedString));
}

void FormDataList::appendString(const CString& string)
{
    m_items.append(string);
}

void FormDataList::appendBlob(PassRefPtrWillBeRawPtr<Blob> blob, const String& filename)
{
    m_items.append(Item(blob, filename));
}

PassRefPtr<FormData> FormDataList::createFormData(FormData::EncodingType encodingType)
{
    RefPtr<FormData> result = FormData::create();
    appendKeyValuePairItemsTo(result.get(), m_encoding, false, encodingType);
    return result.release();
}

PassRefPtr<FormData> FormDataList::createMultiPartFormData()
{
    RefPtr<FormData> result = FormData::create();
    appendKeyValuePairItemsTo(result.get(), m_encoding, true);
    return result.release();
}

void FormDataList::appendKeyValuePairItemsTo(FormData* formData, const WTF::TextEncoding& encoding, bool isMultiPartForm, FormData::EncodingType encodingType)
{
    if (isMultiPartForm)
        formData->setBoundary(FormDataBuilder::generateUniqueBoundaryString());

    Vector<char> encodedData;

    const WillBeHeapVector<Item>& items = this->items();
    size_t formDataListSize = items.size();
    ASSERT(!(formDataListSize % 2));
    for (size_t i = 0; i < formDataListSize; i += 2) {
        const FormDataList::Item& key = items[i];
        const FormDataList::Item& value = items[i + 1];
        if (isMultiPartForm) {
            Vector<char> header;
            FormDataBuilder::beginMultiPartHeader(header, formData->boundary().data(), key.data());

            // If the current type is blob, then we also need to include the filename
            if (value.blob()) {
                String name;
                if (value.blob()->isFile()) {
                    File* file = toFile(value.blob());
                    // For file blob, use the filename (or relative path if it is present) as the name.
                    name = file->webkitRelativePath().isEmpty() ? file->name() : file->webkitRelativePath();

                    // If a filename is passed in FormData.append(), use it instead of the file blob's name.
                    if (!value.filename().isNull())
                        name = value.filename();
                } else {
                    // For non-file blob, use the filename if it is passed in FormData.append().
                    if (!value.filename().isNull())
                        name = value.filename();
                    else
                        name = "blob";
                }

                // We have to include the filename=".." part in the header, even if the filename is empty
                FormDataBuilder::addFilenameToMultiPartHeader(header, encoding, name);

                // Add the content type if available, or "application/octet-stream" otherwise (RFC 1867).
                String contentType;
                if (value.blob()->type().isEmpty())
                    contentType = "application/octet-stream";
                else
                    contentType = value.blob()->type();
                FormDataBuilder::addContentTypeToMultiPartHeader(header, contentType.latin1());
            }

            FormDataBuilder::finishMultiPartHeader(header);

            // Append body
            formData->appendData(header.data(), header.size());
            if (value.blob()) {
                if (value.blob()->hasBackingFile()) {
                    File* file = toFile(value.blob());
                    // Do not add the file if the path is empty.
                    if (!file->path().isEmpty())
                        formData->appendFile(file->path());
                    if (!file->fileSystemURL().isEmpty())
                        formData->appendFileSystemURL(file->fileSystemURL());
                } else {
                    formData->appendBlob(value.blob()->uuid(), value.blob()->blobDataHandle());
                }
            } else {
                formData->appendData(value.data().data(), value.data().length());
            }
            formData->appendData("\r\n", 2);
        } else {
            FormDataBuilder::addKeyValuePairAsFormData(encodedData, key.data(), value.data(), encodingType);
        }
    }

    if (isMultiPartForm)
        FormDataBuilder::addBoundaryToMultiPartHeader(encodedData, formData->boundary().data(), true);

    formData->appendData(encodedData.data(), encodedData.size());
}

void FormDataList::trace(Visitor* visitor)
{
    visitor->trace(m_items);
}

void FormDataList::Item::trace(Visitor* visitor)
{
    visitor->trace(m_blob);
}

} // namespace
