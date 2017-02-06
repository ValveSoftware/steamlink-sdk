// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/WindowProxyManager.h"

#include "bindings/core/v8/DOMWrapperWorld.h"
#include "bindings/core/v8/WindowProxy.h"
#include "core/frame/Frame.h"

namespace blink {

WindowProxyManager* WindowProxyManager::create(Frame& frame)
{
    return new WindowProxyManager(frame);
}

DEFINE_TRACE(WindowProxyManager)
{
    visitor->trace(m_frame);
    visitor->trace(m_windowProxy);
    visitor->trace(m_isolatedWorlds);
}

WindowProxy* WindowProxyManager::windowProxy(DOMWrapperWorld& world)
{
    WindowProxy* windowProxy = nullptr;
    if (world.isMainWorld()) {
        windowProxy = m_windowProxy.get();
    } else {
        IsolatedWorldMap::iterator iter = m_isolatedWorlds.find(world.worldId());
        if (iter != m_isolatedWorlds.end()) {
            windowProxy = iter->value.get();
        } else {
            windowProxy = WindowProxy::create(m_isolate, m_frame, world);
            m_isolatedWorlds.set(world.worldId(), windowProxy);
        }
    }
    return windowProxy;
}

void WindowProxyManager::clearForClose()
{
    m_windowProxy->clearForClose();
    for (auto& entry : m_isolatedWorlds)
        entry.value->clearForClose();
}

void WindowProxyManager::clearForNavigation()
{
    m_windowProxy->clearForNavigation();
    for (auto& entry : m_isolatedWorlds)
        entry.value->clearForNavigation();
}

WindowProxy* WindowProxyManager::existingWindowProxy(DOMWrapperWorld& world)
{
    if (world.isMainWorld())
        return m_windowProxy->isContextInitialized() ? m_windowProxy.get() : nullptr;

    IsolatedWorldMap::iterator iter = m_isolatedWorlds.find(world.worldId());
    if (iter == m_isolatedWorlds.end())
        return nullptr;
    return iter->value->isContextInitialized() ? iter->value.get() : nullptr;
}

void WindowProxyManager::collectIsolatedContexts(Vector<std::pair<ScriptState*, SecurityOrigin*>>& result)
{
    for (auto& entry : m_isolatedWorlds) {
        WindowProxy* isolatedWorldWindowProxy = entry.value.get();
        SecurityOrigin* origin = isolatedWorldWindowProxy->world().isolatedWorldSecurityOrigin();
        if (!isolatedWorldWindowProxy->isContextInitialized())
            continue;
        result.append(std::make_pair(isolatedWorldWindowProxy->getScriptState(), origin));
    }
}

void WindowProxyManager::releaseGlobals(HashMap<DOMWrapperWorld*, v8::Local<v8::Object>>& map)
{
    map.add(&m_windowProxy->world(), m_windowProxy->releaseGlobal());
    for (auto& entry : m_isolatedWorlds)
        map.add(&entry.value->world(), windowProxy(entry.value->world())->releaseGlobal());
}

void WindowProxyManager::setGlobals(const HashMap<DOMWrapperWorld*, v8::Local<v8::Object>>& map)
{
    for (auto& entry : map)
        windowProxy(*entry.key)->setGlobal(entry.value);
}

WindowProxyManager::WindowProxyManager(Frame& frame)
    : m_frame(&frame)
    , m_isolate(v8::Isolate::GetCurrent())
    , m_windowProxy(WindowProxy::create(m_isolate, &frame, DOMWrapperWorld::mainWorld()))
{
}

} // namespace blink
