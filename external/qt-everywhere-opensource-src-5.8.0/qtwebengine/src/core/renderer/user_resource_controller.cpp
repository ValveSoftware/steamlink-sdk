/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "user_resource_controller.h"

#include "base/strings/pattern.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_view.h"
#include "content/public/renderer/render_view_observer.h"
#include "extensions/common/url_pattern.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebScriptSource.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "v8/include/v8.h"

#include "common/qt_messages.h"
#include "common/user_script_data.h"
#include "type_conversion.h"
#include "user_script.h"

Q_GLOBAL_STATIC(UserResourceController, qt_webengine_userResourceController)

static content::RenderView * const globalScriptsIndex = 0;

// Scripts meant to run after the load event will be run 500ms after DOMContentLoaded if the load event doesn't come within that delay.
static const int afterLoadTimeout = 500;

static bool scriptMatchesURL(const UserScriptData &scriptData, const GURL &url) {
    // Logic taken from Chromium (extensions/common/user_script.cc)
    bool matchFound;
    if (!scriptData.urlPatterns.empty()) {
        matchFound = false;
        for (auto it = scriptData.urlPatterns.begin(), end = scriptData.urlPatterns.end(); it != end; ++it) {
            URLPattern urlPattern(QtWebEngineCore::UserScript::validUserScriptSchemes(), *it);
            if (urlPattern.MatchesURL(url))
                matchFound = true;
        }
        if (!matchFound)
            return false;
    }

    if (!scriptData.globs.empty()) {
        matchFound = false;
        for (auto it = scriptData.globs.begin(), end = scriptData.globs.end(); it != end; ++it) {
            if (base::MatchPattern(url.spec(), *it))
                matchFound = true;
        }
        if (!matchFound)
            return false;
    }

    if (!scriptData.excludeGlobs.empty()) {
        for (auto it = scriptData.excludeGlobs.begin(), end = scriptData.excludeGlobs.end(); it != end; ++it) {
            if (base::MatchPattern(url.spec(), *it))
                return false;
        }
    }

    return true;
}

class UserResourceController::RenderViewObserverHelper : public content::RenderViewObserver
{
public:
    RenderViewObserverHelper(content::RenderView *);
private:
    // RenderViewObserver implementation.
    virtual void DidFinishDocumentLoad(blink::WebLocalFrame* frame) Q_DECL_OVERRIDE;
    virtual void DidFinishLoad(blink::WebLocalFrame* frame) Q_DECL_OVERRIDE;
    virtual void DidStartProvisionalLoad(blink::WebLocalFrame* frame) Q_DECL_OVERRIDE;
    virtual void FrameDetached(blink::WebFrame* frame) Q_DECL_OVERRIDE;
    virtual void OnDestruct() Q_DECL_OVERRIDE;
    virtual bool OnMessageReceived(const IPC::Message& message) Q_DECL_OVERRIDE;

    void onUserScriptAdded(const UserScriptData &);
    void onUserScriptRemoved(const UserScriptData &);
    void onScriptsCleared();

    void runScripts(UserScriptData::InjectionPoint, blink::WebLocalFrame *);
    QSet<blink::WebLocalFrame *> m_pendingFrames;
};

void UserResourceController::RenderViewObserverHelper::runScripts(UserScriptData::InjectionPoint p, blink::WebLocalFrame *frame)
{
    if (p == UserScriptData::AfterLoad && !m_pendingFrames.remove(frame))
        return;

    UserResourceController::instance()->runScripts(p, frame);
}

void UserResourceController::runScripts(UserScriptData::InjectionPoint p, blink::WebLocalFrame *frame)
{
    content::RenderView *renderView = content::RenderView::FromWebView(frame->view());
    const bool isMainFrame = (frame == renderView->GetWebView()->mainFrame());

    QList<uint64_t> scriptsToRun = m_viewUserScriptMap.value(globalScriptsIndex).toList();
    scriptsToRun.append(m_viewUserScriptMap.value(renderView).toList());

    Q_FOREACH (uint64_t id, scriptsToRun) {
        const UserScriptData &script = m_scripts.value(id);
        if (script.injectionPoint != p
                || (!script.injectForSubframes && !isMainFrame))
            continue;
        if (!scriptMatchesURL(script, frame->document().url()))
            continue;
        blink::WebScriptSource source(blink::WebString::fromUTF8(script.source), script.url);
        if (script.worldId)
            frame->executeScriptInIsolatedWorld(script.worldId, &source, /*numSources = */1, /*contentScriptExtentsionGroup = */ 0);
        else
            frame->executeScript(source);
    }
}

void UserResourceController::RunScriptsAtDocumentStart(content::RenderFrame *render_frame)
{
    runScripts(UserScriptData::DocumentElementCreation, render_frame->GetWebFrame());
}

void UserResourceController::RunScriptsAtDocumentEnd(content::RenderFrame *render_frame)
{
    runScripts(UserScriptData::DocumentLoadFinished, render_frame->GetWebFrame());
}

UserResourceController::RenderViewObserverHelper::RenderViewObserverHelper(content::RenderView *renderView)
    : content::RenderViewObserver(renderView)
{
}

void UserResourceController::RenderViewObserverHelper::DidFinishDocumentLoad(blink::WebLocalFrame *frame)
{
    m_pendingFrames.insert(frame);
    base::MessageLoop::current()->PostDelayedTask(FROM_HERE, base::Bind(&UserResourceController::RenderViewObserverHelper::runScripts,
                                                                        base::Unretained(this), UserScriptData::AfterLoad, frame),
                                                  base::TimeDelta::FromMilliseconds(afterLoadTimeout));
}

void UserResourceController::RenderViewObserverHelper::DidFinishLoad(blink::WebLocalFrame *frame)
{
    // DidFinishDocumentLoad always comes before this, so frame has already been marked as pending.
    base::MessageLoop::current()->PostTask(FROM_HERE, base::Bind(&UserResourceController::RenderViewObserverHelper::runScripts,
                                                                 base::Unretained(this), UserScriptData::AfterLoad, frame));
}

void UserResourceController::RenderViewObserverHelper::DidStartProvisionalLoad(blink::WebLocalFrame *frame)
{
    m_pendingFrames.remove(frame);
}

void UserResourceController::RenderViewObserverHelper::FrameDetached(blink::WebFrame *frame)
{
    if (frame->isWebLocalFrame())
        m_pendingFrames.remove(frame->toWebLocalFrame());
}

void UserResourceController::RenderViewObserverHelper::OnDestruct()
{
    UserResourceController::instance()->renderViewDestroyed(render_view());
}

bool UserResourceController::RenderViewObserverHelper::OnMessageReceived(const IPC::Message &message)
{
    bool handled = true;
    IPC_BEGIN_MESSAGE_MAP(UserResourceController::RenderViewObserverHelper, message)
        IPC_MESSAGE_HANDLER(RenderViewObserverHelper_AddScript, onUserScriptAdded)
        IPC_MESSAGE_HANDLER(RenderViewObserverHelper_RemoveScript, onUserScriptRemoved)
        IPC_MESSAGE_HANDLER(RenderViewObserverHelper_ClearScripts, onScriptsCleared)
        IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
            return handled;
}

void UserResourceController::RenderViewObserverHelper::onUserScriptAdded(const UserScriptData &script)
{
    UserResourceController::instance()->addScriptForView(script, render_view());
}

void UserResourceController::RenderViewObserverHelper::onUserScriptRemoved(const UserScriptData &script)
{
    UserResourceController::instance()->removeScriptForView(script, render_view());
}

void UserResourceController::RenderViewObserverHelper::onScriptsCleared()
{
    UserResourceController::instance()->clearScriptsForView(render_view());
}

UserResourceController *UserResourceController::instance()
{
    return qt_webengine_userResourceController();
}

bool UserResourceController::OnControlMessageReceived(const IPC::Message &message)
{
    bool handled = true;
    IPC_BEGIN_MESSAGE_MAP(UserResourceController, message)
        IPC_MESSAGE_HANDLER(UserResourceController_AddScript, onAddScript)
        IPC_MESSAGE_HANDLER(UserResourceController_RemoveScript, onRemoveScript)
        IPC_MESSAGE_HANDLER(UserResourceController_ClearScripts, onClearScripts)
        IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
    return handled;
}

UserResourceController::UserResourceController()
{
#if !defined(QT_NO_DEBUG) || defined(QT_FORCE_ASSERTS)
    static bool onlyCalledOnce = true;
    Q_ASSERT(onlyCalledOnce);
    onlyCalledOnce = false;
#endif // !defined(QT_NO_DEBUG) || defined(QT_FORCE_ASSERTS)
}

void UserResourceController::renderViewCreated(content::RenderView *renderView)
{
    // Will destroy itself with their RenderView.
    new RenderViewObserverHelper(renderView);
}

void UserResourceController::renderViewDestroyed(content::RenderView *renderView)
{
    ViewUserScriptMap::iterator it = m_viewUserScriptMap.find(renderView);
    if (it == m_viewUserScriptMap.end()) // ASSERT maybe?
        return;
    Q_FOREACH (uint64_t id, it.value()) {
        m_scripts.remove(id);
    }
    m_viewUserScriptMap.remove(renderView);
}

void UserResourceController::addScriptForView(const UserScriptData &script, content::RenderView *view)
{
    ViewUserScriptMap::iterator it = m_viewUserScriptMap.find(view);
    if (it == m_viewUserScriptMap.end())
        it = m_viewUserScriptMap.insert(view, UserScriptSet());

    (*it).insert(script.scriptId);
    m_scripts.insert(script.scriptId, script);
}

void UserResourceController::removeScriptForView(const UserScriptData &script, content::RenderView *view)
{
    ViewUserScriptMap::iterator it = m_viewUserScriptMap.find(view);
    if (it == m_viewUserScriptMap.end())
        return;

    (*it).remove(script.scriptId);
    m_scripts.remove(script.scriptId);
}

void UserResourceController::clearScriptsForView(content::RenderView *view)
{
    ViewUserScriptMap::iterator it = m_viewUserScriptMap.find(view);
    if (it == m_viewUserScriptMap.end())
        return;
    Q_FOREACH (uint64_t id, it.value())
        m_scripts.remove(id);

    m_viewUserScriptMap.remove(view);
}

void UserResourceController::onAddScript(const UserScriptData &script)
{
    addScriptForView(script, globalScriptsIndex);
}

void UserResourceController::onRemoveScript(const UserScriptData &script)
{
    removeScriptForView(script, globalScriptsIndex);
}

void UserResourceController::onClearScripts()
{
    clearScriptsForView(globalScriptsIndex);
}

