/*
    Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)

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

#ifndef Navigator_h
#define Navigator_h

#include "bindings/v8/ScriptWrappable.h"
#include "core/frame/DOMWindowProperty.h"
#include "core/frame/NavigatorCPU.h"
#include "core/frame/NavigatorID.h"
#include "core/frame/NavigatorLanguage.h"
#include "core/frame/NavigatorOnLine.h"
#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"
#include "wtf/Forward.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"

namespace WebCore {

class DOMMimeTypeArray;
class DOMPluginArray;
class LocalFrame;
class PluginData;

typedef int ExceptionCode;

class Navigator FINAL
    : public RefCountedWillBeGarbageCollectedFinalized<Navigator>
    , public NavigatorCPU
    , public NavigatorID
    , public NavigatorLanguage
    , public NavigatorOnLine
    , public ScriptWrappable
    , public DOMWindowProperty
    , public WillBeHeapSupplementable<Navigator> {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(Navigator);
public:
    static PassRefPtrWillBeRawPtr<Navigator> create(LocalFrame* frame)
    {
        return adoptRefWillBeNoop(new Navigator(frame));
    }

    virtual ~Navigator();

    DOMPluginArray* plugins() const;
    DOMMimeTypeArray* mimeTypes() const;
    bool cookieEnabled() const;
    bool javaEnabled() const;

    String productSub() const;
    String vendor() const;
    String vendorSub() const;

    virtual String userAgent() const OVERRIDE;

    // Relinquishes the storage lock, if one exists.
    void getStorageUpdates();

    // NavigatorLanguage
    virtual Vector<String> languages() OVERRIDE;

    virtual void trace(Visitor*);

private:
    explicit Navigator(LocalFrame*);

    mutable RefPtrWillBeMember<DOMPluginArray> m_plugins;
    mutable RefPtrWillBeMember<DOMMimeTypeArray> m_mimeTypes;
};

}

#endif
