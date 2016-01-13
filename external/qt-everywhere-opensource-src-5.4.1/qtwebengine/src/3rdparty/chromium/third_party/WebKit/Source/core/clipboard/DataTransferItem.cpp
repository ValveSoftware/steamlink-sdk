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

#include "config.h"
#include "core/clipboard/DataTransferItem.h"

#include "bindings/v8/V8Binding.h"
#include "core/clipboard/Clipboard.h"
#include "core/clipboard/DataObjectItem.h"
#include "core/dom/StringCallback.h"
#include "wtf/StdLibExtras.h"

namespace WebCore {

PassRefPtrWillBeRawPtr<DataTransferItem> DataTransferItem::create(PassRefPtrWillBeRawPtr<Clipboard> clipboard, PassRefPtrWillBeRawPtr<DataObjectItem> item)
{
    return adoptRefWillBeNoop(new DataTransferItem(clipboard, item));
}

DataTransferItem::~DataTransferItem()
{
}

String DataTransferItem::kind() const
{
    DEFINE_STATIC_LOCAL(const String, kindString, ("string"));
    DEFINE_STATIC_LOCAL(const String, kindFile, ("file"));
    if (!m_clipboard->canReadTypes())
        return String();
    switch (m_item->kind()) {
    case DataObjectItem::StringKind:
        return kindString;
    case DataObjectItem::FileKind:
        return kindFile;
    }
    ASSERT_NOT_REACHED();
    return String();
}

String DataTransferItem::type() const
{
    if (!m_clipboard->canReadTypes())
        return String();
    return m_item->type();
}

void DataTransferItem::getAsString(ExecutionContext* context, PassOwnPtr<StringCallback> callback) const
{
    if (!m_clipboard->canReadData())
        return;
    if (!callback || m_item->kind() != DataObjectItem::StringKind)
        return;

    StringCallback::scheduleCallback(callback, context, m_item->getAsString());
}

PassRefPtrWillBeRawPtr<Blob> DataTransferItem::getAsFile() const
{
    if (!m_clipboard->canReadData())
        return nullptr;

    return m_item->getAsFile();
}

DataTransferItem::DataTransferItem(PassRefPtrWillBeRawPtr<Clipboard> clipboard, PassRefPtrWillBeRawPtr<DataObjectItem> item)
    : m_clipboard(clipboard)
    , m_item(item)
{
    ScriptWrappable::init(this);
}

void DataTransferItem::trace(Visitor* visitor)
{
    visitor->trace(m_clipboard);
    visitor->trace(m_item);
}

} // namespace WebCore

