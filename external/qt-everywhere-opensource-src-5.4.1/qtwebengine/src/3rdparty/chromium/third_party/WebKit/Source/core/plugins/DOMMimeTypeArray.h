/*
    Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
    Copyright (C) 2008 Apple Inc. All rights reserved.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef DOMMimeTypeArray_h
#define DOMMimeTypeArray_h

#include "bindings/v8/ScriptWrappable.h"
#include "core/frame/DOMWindowProperty.h"
#include "core/plugins/DOMMimeType.h"
#include "platform/heap/Handle.h"
#include "wtf/Forward.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/Vector.h"

namespace WebCore {

class LocalFrame;
class PluginData;

class DOMMimeTypeArray FINAL : public RefCountedWillBeGarbageCollectedFinalized<DOMMimeTypeArray>, public ScriptWrappable, public DOMWindowProperty {
public:
    static PassRefPtrWillBeRawPtr<DOMMimeTypeArray> create(LocalFrame* frame)
    {
        return adoptRefWillBeNoop(new DOMMimeTypeArray(frame));
    }
    virtual ~DOMMimeTypeArray();

    unsigned length() const;
    PassRefPtrWillBeRawPtr<DOMMimeType> item(unsigned index);
    bool canGetItemsForName(const AtomicString& propertyName);
    PassRefPtrWillBeRawPtr<DOMMimeType> namedItem(const AtomicString& propertyName);

    void trace(Visitor*) { }

private:
    explicit DOMMimeTypeArray(LocalFrame*);
    PluginData* getPluginData() const;
};

} // namespace WebCore

#endif // MimeTypeArray_h
