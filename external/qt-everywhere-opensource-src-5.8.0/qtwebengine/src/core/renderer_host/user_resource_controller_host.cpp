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

#include "user_resource_controller_host.h"

#include "common/qt_messages.h"
#include "type_conversion.h"
#include "web_contents_adapter.h"
#include "web_contents_adapter_p.h"

#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

namespace QtWebEngineCore {

class UserResourceControllerHost::WebContentsObserverHelper : public content::WebContentsObserver {
public:
    WebContentsObserverHelper(UserResourceControllerHost *, content::WebContents *);

    // WebContentsObserver overrides:
    void RenderViewCreated(content::RenderViewHost *renderViewHost) override;
    void RenderViewHostChanged(content::RenderViewHost *oldHost, content::RenderViewHost *newHost) override;
    void WebContentsDestroyed() override;

private:
    UserResourceControllerHost *m_controllerHost;
};

UserResourceControllerHost::WebContentsObserverHelper::WebContentsObserverHelper(UserResourceControllerHost *controller, content::WebContents *contents)
    : content::WebContentsObserver(contents)
    , m_controllerHost(controller)
{
}

void UserResourceControllerHost::WebContentsObserverHelper::RenderViewCreated(content::RenderViewHost *renderViewHost)
{
    content::WebContents *contents = web_contents();
    Q_FOREACH (const UserScript &script, m_controllerHost->m_perContentsScripts.value(contents))
        renderViewHost->Send(new RenderViewObserverHelper_AddScript(renderViewHost->GetRoutingID(), script.data()));
}

void UserResourceControllerHost::WebContentsObserverHelper::RenderViewHostChanged(content::RenderViewHost *oldHost,
                                                                                content::RenderViewHost *newHost)
{
    if (oldHost)
        oldHost->Send(new RenderViewObserverHelper_ClearScripts(oldHost->GetRoutingID()));

    content::WebContents *contents = web_contents();
    Q_FOREACH (const UserScript &script, m_controllerHost->m_perContentsScripts.value(contents))
        newHost->Send(new RenderViewObserverHelper_AddScript(newHost->GetRoutingID(), script.data()));
}

void UserResourceControllerHost::WebContentsObserverHelper::WebContentsDestroyed()
{
    m_controllerHost->webContentsDestroyed(web_contents());
    delete this;
}

class UserResourceControllerHost::RenderProcessObserverHelper : public content::RenderProcessHostObserver {
public:
    RenderProcessObserverHelper(UserResourceControllerHost *);
    virtual void RenderProcessHostDestroyed(content::RenderProcessHost *) Q_DECL_OVERRIDE;
private:
    UserResourceControllerHost *m_controllerHost;
};

UserResourceControllerHost::RenderProcessObserverHelper::RenderProcessObserverHelper(UserResourceControllerHost *controller)
    : m_controllerHost(controller)
{
}

void UserResourceControllerHost::RenderProcessObserverHelper::RenderProcessHostDestroyed(content::RenderProcessHost *renderer)
{
    Q_ASSERT(m_controllerHost);
    m_controllerHost->m_observedProcesses.remove(renderer);
}

void UserResourceControllerHost::addUserScript(const UserScript &script, WebContentsAdapter *adapter)
{
    if (script.isNull())
        return;
    // Global scripts should be dispatched to all our render processes.
    if (!adapter) {
        if (!m_profileWideScripts.contains(script)) {
            m_profileWideScripts.append(script);
            Q_FOREACH (content::RenderProcessHost *renderer, m_observedProcesses)
                renderer->Send(new UserResourceController_AddScript(script.data()));
        }
    } else {
        content::WebContents *contents = adapter->webContents();
        ContentsScriptsMap::iterator it = m_perContentsScripts.find(contents);
        if (it == m_perContentsScripts.end()) {
            // We need to keep track of RenderView/RenderViewHost changes for a given contents
            // in order to make sure the scripts stay in sync
            new WebContentsObserverHelper(this, contents);
            it = m_perContentsScripts.insert(contents, (QList<UserScript>() << script));
        } else {
            QList<UserScript> currentScripts = it.value();
            if (!currentScripts.contains(script)) {
                currentScripts.append(script);
                m_perContentsScripts.insert(contents, currentScripts);
            }
        }
        contents->Send(new RenderViewObserverHelper_AddScript(contents->GetRoutingID(), script.data()));
    }
}

bool UserResourceControllerHost::containsUserScript(const UserScript &script, WebContentsAdapter *adapter)
{
    if (script.isNull())
        return false;
    // Global scripts should be dispatched to all our render processes.
    if (!adapter)
        return m_profileWideScripts.contains(script);
    return m_perContentsScripts.value(adapter->webContents()).contains(script);
}

bool UserResourceControllerHost::removeUserScript(const UserScript &script, WebContentsAdapter *adapter)
{
    if (script.isNull())
        return false;
    if (!adapter) {
        QList<UserScript>::iterator it
                = std::find(m_profileWideScripts.begin(), m_profileWideScripts.end(), script);
        if (it == m_profileWideScripts.end())
            return false;
        Q_FOREACH (content::RenderProcessHost *renderer, m_observedProcesses)
            renderer->Send(new UserResourceController_RemoveScript((*it).data()));
        m_profileWideScripts.erase(it);
    } else {
        content::WebContents *contents = adapter->webContents();
        if (!m_perContentsScripts.contains(contents))
            return false;
        QList<UserScript> &list(m_perContentsScripts[contents]);
        QList<UserScript>::iterator it = std::find(list.begin(), list.end(), script);
        if (it == list.end())
            return false;
        contents->Send(new RenderViewObserverHelper_RemoveScript(contents->GetRoutingID(), (*it).data()));
        list.erase(it);
    }
    return true;
}

void UserResourceControllerHost::clearAllScripts(WebContentsAdapter *adapter)
{
    if (!adapter) {
        m_profileWideScripts.clear();
        Q_FOREACH (content::RenderProcessHost *renderer, m_observedProcesses)
            renderer->Send(new UserResourceController_ClearScripts);
    } else {
        content::WebContents *contents = adapter->webContents();
        m_perContentsScripts.remove(contents);
        contents->Send(new RenderViewObserverHelper_ClearScripts(contents->GetRoutingID()));
    }
}

const QList<UserScript> UserResourceControllerHost::registeredScripts(WebContentsAdapter *adapter) const
{
    if (!adapter)
        return m_profileWideScripts;
    return m_perContentsScripts.value(adapter->webContents());
}

void UserResourceControllerHost::reserve(WebContentsAdapter *adapter, int count)
{
    if (!adapter)
        m_profileWideScripts.reserve(count);
    else
        m_perContentsScripts[adapter->webContents()].reserve(count);
}

void UserResourceControllerHost::renderProcessStartedWithHost(content::RenderProcessHost *renderer)
{
    if (m_observedProcesses.contains(renderer))
        return;

    if (m_renderProcessObserver.isNull())
        m_renderProcessObserver.reset(new RenderProcessObserverHelper(this));
    renderer->AddObserver(m_renderProcessObserver.data());
    m_observedProcesses.insert(renderer);
    Q_FOREACH (const UserScript &script, m_profileWideScripts)
        renderer->Send(new UserResourceController_AddScript(script.data()));
}

void UserResourceControllerHost::webContentsDestroyed(content::WebContents *contents)
{
    m_perContentsScripts.remove(contents);
}

UserResourceControllerHost::UserResourceControllerHost()
{
}

UserResourceControllerHost::~UserResourceControllerHost()
{
    Q_FOREACH (content::RenderProcessHost *renderer, m_observedProcesses)
        renderer->RemoveObserver(m_renderProcessObserver.data());
}

} // namespace
