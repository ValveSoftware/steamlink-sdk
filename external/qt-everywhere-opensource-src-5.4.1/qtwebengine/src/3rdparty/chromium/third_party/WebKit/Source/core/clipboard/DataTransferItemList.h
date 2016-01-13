/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#ifndef DataTransferItemList_h
#define DataTransferItemList_h

#include "bindings/v8/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "wtf/Forward.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"

namespace WebCore {

class DataObject;
class Clipboard;
class DataTransferItem;
class File;

class ExceptionState;

class DataTransferItemList : public RefCountedWillBeGarbageCollectedFinalized<DataTransferItemList>, public ScriptWrappable {
public:
    static PassRefPtrWillBeRawPtr<DataTransferItemList> create(PassRefPtrWillBeRawPtr<Clipboard>, PassRefPtrWillBeRawPtr<DataObject>);
    ~DataTransferItemList();

    size_t length() const;
    PassRefPtrWillBeRawPtr<DataTransferItem> item(unsigned long index);
    void deleteItem(unsigned long index, ExceptionState&);
    void clear();
    PassRefPtrWillBeRawPtr<DataTransferItem> add(const String& data, const String& type, ExceptionState&);
    PassRefPtrWillBeRawPtr<DataTransferItem> add(PassRefPtrWillBeRawPtr<File>);

    void trace(Visitor*);

private:
    DataTransferItemList(PassRefPtrWillBeRawPtr<Clipboard>, PassRefPtrWillBeRawPtr<DataObject>);

    RefPtrWillBeMember<Clipboard> m_clipboard;
    RefPtrWillBeMember<DataObject> m_dataObject;
};

} // namespace WebCore

#endif // DataTransferItemList_h
