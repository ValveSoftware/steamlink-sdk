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

#ifndef DOMPlugin_h
#define DOMPlugin_h

#include "bindings/v8/ScriptWrappable.h"
#include "core/frame/FrameDestructionObserver.h"
#include "core/plugins/DOMMimeType.h"
#include "platform/heap/Handle.h"
#include "wtf/Forward.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"

namespace WebCore {

class Plugin;
class PluginData;

class DOMPlugin FINAL : public RefCountedWillBeGarbageCollectedFinalized<DOMPlugin>, public ScriptWrappable, public FrameDestructionObserver {
public:
    static PassRefPtrWillBeRawPtr<DOMPlugin> create(PluginData* pluginData, LocalFrame* frame, unsigned index)
    {
        return adoptRefWillBeNoop(new DOMPlugin(pluginData, frame, index));
    }
    virtual ~DOMPlugin();

    String name() const;
    String filename() const;
    String description() const;

    unsigned length() const;

    PassRefPtrWillBeRawPtr<DOMMimeType> item(unsigned index);
    bool canGetItemsForName(const AtomicString& propertyName);
    PassRefPtrWillBeRawPtr<DOMMimeType> namedItem(const AtomicString& propertyName);

    void trace(Visitor*) { }

private:
    const PluginInfo& pluginInfo() const { return m_pluginData->plugins()[m_index]; }

    DOMPlugin(PluginData*, LocalFrame*, unsigned index);
    RefPtr<PluginData> m_pluginData;
    unsigned m_index;
};

} // namespace WebCore

#endif // Plugin_h
