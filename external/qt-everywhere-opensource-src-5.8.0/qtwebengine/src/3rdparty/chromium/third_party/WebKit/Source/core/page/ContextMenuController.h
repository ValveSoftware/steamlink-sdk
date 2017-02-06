/*
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ContextMenuController_h
#define ContextMenuController_h

#include "core/CoreExport.h"
#include "core/layout/HitTestResult.h"
#include "platform/heap/Handle.h"
#include "wtf/Noncopyable.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"
#include <memory>

namespace blink {

class ContextMenu;
class ContextMenuClient;
class ContextMenuItem;
class ContextMenuProvider;
class Document;
class Event;
class LocalFrame;
class Page;

class CORE_EXPORT ContextMenuController final : public GarbageCollectedFinalized<ContextMenuController> {
    WTF_MAKE_NONCOPYABLE(ContextMenuController);
public:
    static ContextMenuController* create(Page*, ContextMenuClient*);
    ~ContextMenuController();
    DECLARE_TRACE();

    ContextMenu* contextMenu() const { return m_contextMenu.get(); }
    void clearContextMenu();

    void documentDetached(Document*);

    void handleContextMenuEvent(Event*);
    void showContextMenu(Event*, ContextMenuProvider*);
    void showContextMenuAtPoint(LocalFrame*, float x, float y, ContextMenuProvider*);

    void contextMenuItemSelected(const ContextMenuItem*);

    const HitTestResult& hitTestResult() { return m_hitTestResult; }

private:
    ContextMenuController(Page*, ContextMenuClient*);

    std::unique_ptr<ContextMenu> createContextMenu(Event*);
    std::unique_ptr<ContextMenu> createContextMenu(LocalFrame*, const LayoutPoint&);
    void populateCustomContextMenu(const Event&);
    void showContextMenu(Event*);

    ContextMenuClient* m_client;
    std::unique_ptr<ContextMenu> m_contextMenu;
    Member<ContextMenuProvider> m_menuProvider;
    HitTestResult m_hitTestResult;
};

} // namespace blink

#endif // ContextMenuController_h
