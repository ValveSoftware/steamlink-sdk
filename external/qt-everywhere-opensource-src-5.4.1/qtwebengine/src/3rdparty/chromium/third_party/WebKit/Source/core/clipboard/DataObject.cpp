/*
 * Copyright (c) 2008, 2009, 2012 Google Inc. All rights reserved.
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

#include "core/clipboard/Pasteboard.h"
#include "platform/clipboard/ClipboardMimeTypes.h"
#include "platform/clipboard/ClipboardUtilities.h"
#include "public/platform/Platform.h"
#include "public/platform/WebClipboard.h"

namespace WebCore {

PassRefPtrWillBeRawPtr<DataObject> DataObject::createFromPasteboard(PasteMode pasteMode)
{
    RefPtrWillBeRawPtr<DataObject> dataObject = create();
    blink::WebClipboard::Buffer buffer = Pasteboard::generalPasteboard()->buffer();
    uint64_t sequenceNumber = blink::Platform::current()->clipboard()->sequenceNumber(buffer);
    bool ignored;
    blink::WebVector<blink::WebString> webTypes = blink::Platform::current()->clipboard()->readAvailableTypes(buffer, &ignored);
    ListHashSet<String> types;
    for (size_t i = 0; i < webTypes.size(); ++i)
        types.add(webTypes[i]);
    for (ListHashSet<String>::const_iterator it = types.begin(); it != types.end(); ++it) {
        if (pasteMode == PlainTextOnly && *it != mimeTypeTextPlain)
            continue;
        dataObject->m_itemList.append(DataObjectItem::createFromPasteboard(*it, sequenceNumber));
    }
    return dataObject.release();
}

PassRefPtrWillBeRawPtr<DataObject> DataObject::create()
{
    return adoptRefWillBeNoop(new DataObject());
}

PassRefPtrWillBeRawPtr<DataObject> DataObject::copy() const
{
    return adoptRefWillBeNoop(new DataObject(*this));
}

DataObject::~DataObject()
{
}

size_t DataObject::length() const
{
    return m_itemList.size();
}

PassRefPtrWillBeRawPtr<DataObjectItem> DataObject::item(unsigned long index)
{
    if (index >= length())
        return nullptr;
    return m_itemList[index];
}

void DataObject::deleteItem(unsigned long index)
{
    if (index >= length())
        return;
    m_itemList.remove(index);
}

void DataObject::clearAll()
{
    m_itemList.clear();
}

PassRefPtrWillBeRawPtr<DataObjectItem> DataObject::add(const String& data, const String& type)
{
    RefPtrWillBeRawPtr<DataObjectItem> item = DataObjectItem::createFromString(type, data);
    if (!internalAddStringItem(item))
        return nullptr;
    return item;
}

PassRefPtrWillBeRawPtr<DataObjectItem> DataObject::add(PassRefPtrWillBeRawPtr<File> file)
{
    if (!file)
        return nullptr;

    RefPtrWillBeRawPtr<DataObjectItem> item = DataObjectItem::createFromFile(file);
    m_itemList.append(item);
    return item;
}

void DataObject::clearData(const String& type)
{
    for (size_t i = 0; i < m_itemList.size(); ++i) {
        if (m_itemList[i]->kind() == DataObjectItem::StringKind && m_itemList[i]->type() == type) {
            // Per the spec, type must be unique among all items of kind 'string'.
            m_itemList.remove(i);
            return;
        }
    }
}

ListHashSet<String> DataObject::types() const
{
    ListHashSet<String> results;
    bool containsFiles = false;
    for (size_t i = 0; i < m_itemList.size(); ++i) {
        switch (m_itemList[i]->kind()) {
        case DataObjectItem::StringKind:
            results.add(m_itemList[i]->type());
            break;
        case DataObjectItem::FileKind:
            containsFiles = true;
            break;
        }
    }
    if (containsFiles)
        results.add(mimeTypeFiles);
    return results;
}

String DataObject::getData(const String& type) const
{
    for (size_t i = 0; i < m_itemList.size(); ++i)  {
        if (m_itemList[i]->kind() == DataObjectItem::StringKind && m_itemList[i]->type() == type)
            return m_itemList[i]->getAsString();
    }
    return String();
}

bool DataObject::setData(const String& type, const String& data)
{
    clearData(type);
    if (!add(data, type))
        ASSERT_NOT_REACHED();
    return true;
}

void DataObject::urlAndTitle(String& url, String* title) const
{
    RefPtrWillBeRawPtr<DataObjectItem> item = findStringItem(mimeTypeTextURIList);
    if (!item)
        return;
    url = convertURIListToURL(item->getAsString());
    if (title)
        *title = item->title();
}

void DataObject::setURLAndTitle(const String& url, const String& title)
{
    clearData(mimeTypeTextURIList);
    internalAddStringItem(DataObjectItem::createFromURL(url, title));
}

void DataObject::htmlAndBaseURL(String& html, KURL& baseURL) const
{
    RefPtrWillBeRawPtr<DataObjectItem> item = findStringItem(mimeTypeTextHTML);
    if (!item)
        return;
    html = item->getAsString();
    baseURL = item->baseURL();
}

void DataObject::setHTMLAndBaseURL(const String& html, const KURL& baseURL)
{
    clearData(mimeTypeTextHTML);
    internalAddStringItem(DataObjectItem::createFromHTML(html, baseURL));
}

bool DataObject::containsFilenames() const
{
    for (size_t i = 0; i < m_itemList.size(); ++i) {
        if (m_itemList[i]->isFilename())
            return true;
    }
    return false;
}

Vector<String> DataObject::filenames() const
{
    Vector<String> results;
    for (size_t i = 0; i < m_itemList.size(); ++i) {
        if (m_itemList[i]->isFilename())
            results.append(static_cast<File*>(m_itemList[i]->getAsFile().get())->path());
    }
    return results;
}

void DataObject::addFilename(const String& filename, const String& displayName)
{
    internalAddFileItem(DataObjectItem::createFromFile(File::createWithName(filename, displayName, File::AllContentTypes)));
}

void DataObject::addSharedBuffer(const String& name, PassRefPtr<SharedBuffer> buffer)
{
    internalAddFileItem(DataObjectItem::createFromSharedBuffer(name, buffer));
}

DataObject::DataObject()
    : m_modifierKeyState(0)
{
}

DataObject::DataObject(const DataObject& other)
    : m_itemList(other.m_itemList)
    , m_modifierKeyState(0)
{
}

PassRefPtrWillBeRawPtr<DataObjectItem> DataObject::findStringItem(const String& type) const
{
    for (size_t i = 0; i < m_itemList.size(); ++i) {
        if (m_itemList[i]->kind() == DataObjectItem::StringKind && m_itemList[i]->type() == type)
            return m_itemList[i];
    }
    return nullptr;
}

bool DataObject::internalAddStringItem(PassRefPtrWillBeRawPtr<DataObjectItem> item)
{
    ASSERT(item->kind() == DataObjectItem::StringKind);
    for (size_t i = 0; i < m_itemList.size(); ++i) {
        if (m_itemList[i]->kind() == DataObjectItem::StringKind && m_itemList[i]->type() == item->type())
            return false;
    }

    m_itemList.append(item);
    return true;
}

void DataObject::internalAddFileItem(PassRefPtrWillBeRawPtr<DataObjectItem> item)
{
    ASSERT(item->kind() == DataObjectItem::FileKind);
    m_itemList.append(item);
}

void DataObject::trace(Visitor* visitor)
{
    visitor->trace(m_itemList);
    WillBeHeapSupplementable<DataObject>::trace(visitor);
}

} // namespace WebCore
