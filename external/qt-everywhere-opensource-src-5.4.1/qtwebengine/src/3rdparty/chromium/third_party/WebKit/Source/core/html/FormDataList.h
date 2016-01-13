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

#ifndef FormDataList_h
#define FormDataList_h

#include "core/fileapi/Blob.h"
#include "platform/heap/Handle.h"
#include "platform/network/FormData.h"
#include "wtf/Forward.h"
#include "wtf/text/CString.h"
#include "wtf/text/TextEncoding.h"

namespace WebCore {

class FormDataList {
public:
    class Item {
        ALLOW_ONLY_INLINE_ALLOCATION();
    public:
        Item() { }
        Item(const WTF::CString& data) : m_data(data) { }
        Item(PassRefPtrWillBeRawPtr<Blob> blob, const String& filename) : m_blob(blob), m_filename(filename) { }

        const WTF::CString& data() const { return m_data; }
        Blob* blob() const { return m_blob.get(); }
        const String& filename() const { return m_filename; }

        void trace(Visitor*);

    private:
        WTF::CString m_data;
        RefPtrWillBeMember<Blob> m_blob;
        String m_filename;
    };

    FormDataList(const WTF::TextEncoding&);

    void appendData(const String& key, const String& value)
    {
        appendString(key);
        appendString(value);
    }
    void appendData(const String& key, const CString& value)
    {
        appendString(key);
        appendString(value);
    }
    void appendData(const String& key, int value)
    {
        appendString(key);
        appendString(String::number(value));
    }
    void appendBlob(const String& key, PassRefPtrWillBeRawPtr<Blob> blob, const String& filename = String())
    {
        appendString(key);
        appendBlob(blob, filename);
    }

    const WillBeHeapVector<Item>& items() const { return m_items; }
    const WTF::TextEncoding& encoding() const { return m_encoding; }

    PassRefPtr<FormData> createFormData(FormData::EncodingType = FormData::FormURLEncoded);
    PassRefPtr<FormData> createMultiPartFormData();

    void trace(Visitor*);

private:
    void appendKeyValuePairItemsTo(FormData*, const WTF::TextEncoding&, bool isMultiPartForm, FormData::EncodingType = FormData::FormURLEncoded);

    void appendString(const CString&);
    void appendString(const String&);
    void appendBlob(PassRefPtrWillBeRawPtr<Blob>, const String& filename);

    WTF::TextEncoding m_encoding;
    WillBeHeapVector<Item> m_items;
};

} // namespace WebCore

WTF_ALLOW_MOVE_AND_INIT_WITH_MEM_FUNCTIONS(WebCore::FormDataList::Item);

#endif // FormDataList_h
