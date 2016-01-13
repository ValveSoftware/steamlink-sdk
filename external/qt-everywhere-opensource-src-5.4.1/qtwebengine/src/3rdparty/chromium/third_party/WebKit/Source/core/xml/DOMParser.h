/*
 *  Copyright (C) 2003, 2006 Apple Computer, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef DOMParser_h
#define DOMParser_h

#include "bindings/v8/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "wtf/Forward.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"

namespace WebCore {

class Document;
class ExceptionState;

class DOMParser : public RefCountedWillBeGarbageCollectedFinalized<DOMParser>, public ScriptWrappable {
public:
    static PassRefPtrWillBeRawPtr<DOMParser> create()
    {
        return adoptRefWillBeNoop(new DOMParser);
    }

    PassRefPtrWillBeRawPtr<Document> parseFromString(const String&, const String& contentType, ExceptionState&);

    void trace(Visitor*) { }

private:
    DOMParser()
    {
        ScriptWrappable::init(this);
    }
};

}

#endif // XMLSerializer.h
