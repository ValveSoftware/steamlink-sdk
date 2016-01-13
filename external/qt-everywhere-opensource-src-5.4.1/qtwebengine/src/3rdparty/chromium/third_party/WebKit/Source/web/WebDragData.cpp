/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#include "core/clipboard/DataObject.h"
#include "core/clipboard/DataTransferItem.h"
#include "modules/filesystem/DraggedIsolatedFileSystem.h"
#include "platform/clipboard/ClipboardMimeTypes.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebData.h"
#include "public/platform/WebDragData.h"
#include "public/platform/WebString.h"
#include "public/platform/WebURL.h"
#include "public/platform/WebVector.h"
#include "wtf/HashMap.h"
#include "wtf/PassRefPtr.h"

using namespace WebCore;

namespace blink {

void WebDragData::initialize()
{
    m_private = DataObject::create();
}

void WebDragData::reset()
{
    m_private.reset();
}

void WebDragData::assign(const WebDragData& other)
{
    m_private = other.m_private;
}

WebDragData::WebDragData(const PassRefPtrWillBeRawPtr<WebCore::DataObject>& object)
{
    m_private = object;
}

WebDragData& WebDragData::operator=(const PassRefPtrWillBeRawPtr<WebCore::DataObject>& object)
{
    m_private = object;
    return *this;
}

DataObject* WebDragData::getValue() const
{
    return m_private.get();
}

void WebDragData::ensureMutable()
{
    ASSERT(!isNull());
}

WebVector<WebDragData::Item> WebDragData::items() const
{
    Vector<Item> itemList;
    for (size_t i = 0; i < m_private->length(); ++i) {
        DataObjectItem* originalItem = m_private->item(i).get();
        WebDragData::Item item;
        if (originalItem->kind() == DataObjectItem::StringKind) {
            item.storageType = Item::StorageTypeString;
            item.stringType = originalItem->type();
            item.stringData = originalItem->getAsString();
        } else if (originalItem->kind() == DataObjectItem::FileKind) {
            if (originalItem->sharedBuffer()) {
                item.storageType = Item::StorageTypeBinaryData;
                item.binaryData = originalItem->sharedBuffer();
            } else if (originalItem->isFilename()) {
                RefPtrWillBeRawPtr<WebCore::Blob> blob = originalItem->getAsFile();
                if (blob->isFile()) {
                    File* file = toFile(blob.get());
                    if (!file->path().isEmpty()) {
                        item.storageType = Item::StorageTypeFilename;
                        item.filenameData = file->path();
                        item.displayNameData = file->name();
                    } else if (!file->fileSystemURL().isEmpty()) {
                        item.storageType = Item::StorageTypeFileSystemFile;
                        item.fileSystemURL = file->fileSystemURL();
                        item.fileSystemFileSize = file->size();
                    } else {
                        ASSERT_NOT_REACHED();
                    }
                } else
                    ASSERT_NOT_REACHED();
            } else
                ASSERT_NOT_REACHED();
        } else
            ASSERT_NOT_REACHED();
        item.title = originalItem->title();
        item.baseURL = originalItem->baseURL();
        itemList.append(item);
    }
    return itemList;
}

void WebDragData::setItems(const WebVector<Item>& itemList)
{
    m_private->clearAll();
    for (size_t i = 0; i < itemList.size(); ++i)
        addItem(itemList[i]);
}

void WebDragData::addItem(const Item& item)
{
    ensureMutable();
    switch (item.storageType) {
    case Item::StorageTypeString:
        if (String(item.stringType) == mimeTypeTextURIList)
            m_private->setURLAndTitle(item.stringData, item.title);
        else if (String(item.stringType) == mimeTypeTextHTML)
            m_private->setHTMLAndBaseURL(item.stringData, item.baseURL);
        else
            m_private->setData(item.stringType, item.stringData);
        return;
    case Item::StorageTypeFilename:
        m_private->addFilename(item.filenameData, item.displayNameData);
        return;
    case Item::StorageTypeBinaryData:
        // This should never happen when dragging in.
        ASSERT_NOT_REACHED();
        return;
    case Item::StorageTypeFileSystemFile:
        {
            FileMetadata fileMetadata;
            fileMetadata.length = item.fileSystemFileSize;
            m_private->add(File::createForFileSystemFile(item.fileSystemURL, fileMetadata));
        }
        return;
    }
}

WebString WebDragData::filesystemId() const
{
    ASSERT(!isNull());
    return m_private.get()->filesystemId();
}

void WebDragData::setFilesystemId(const WebString& filesystemId)
{
    // The ID is an opaque string, given by and validated by chromium port.
    ensureMutable();
    DraggedIsolatedFileSystem::provideTo(*m_private.get(), DraggedIsolatedFileSystem::supplementName(), DraggedIsolatedFileSystem::create(*m_private.get(), filesystemId));
}

} // namespace blink
