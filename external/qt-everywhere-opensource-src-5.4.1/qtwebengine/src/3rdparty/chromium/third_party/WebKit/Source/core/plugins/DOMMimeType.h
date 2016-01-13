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

#ifndef DOMMimeType_h
#define DOMMimeType_h

#include "bindings/v8/ScriptWrappable.h"
#include "core/frame/FrameDestructionObserver.h"
#include "platform/heap/Handle.h"
#include "platform/plugins/PluginData.h"
#include "wtf/Forward.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"

namespace WebCore {

class DOMPlugin;
class LocalFrame;

class DOMMimeType FINAL : public RefCountedWillBeGarbageCollectedFinalized<DOMMimeType>, public ScriptWrappable, public FrameDestructionObserver {
public:
    static PassRefPtrWillBeRawPtr<DOMMimeType> create(PassRefPtr<PluginData> pluginData, LocalFrame* frame, unsigned index)
    {
        return adoptRefWillBeNoop(new DOMMimeType(pluginData, frame, index));
    }
    virtual ~DOMMimeType();

    const String& type() const;
    String suffixes() const;
    const String& description() const;
    PassRefPtrWillBeRawPtr<DOMPlugin> enabledPlugin() const;

    void trace(Visitor*) { }

private:
    const MimeClassInfo& mimeClassInfo() const { return m_pluginData->mimes()[m_index]; }

    DOMMimeType(PassRefPtr<PluginData>, LocalFrame*, unsigned index);
    RefPtr<PluginData> m_pluginData;
    unsigned m_index;
};

}

#endif
