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

#ifndef USER_RESOURCE_CONTROLLER_H
#define USER_RESOURCE_CONTROLLER_H

#include "content/public/renderer/render_thread_observer.h"

#include "common/user_script_data.h"

#include <QtCore/qcompilerdetection.h>
#include <QtCore/QHash>
#include <QtCore/QSet>

namespace blink {
class WebLocalFrame;
}

namespace content {
class RenderFrame;
class RenderView;
}

class UserResourceController : public content::RenderThreadObserver {

public:
    static UserResourceController *instance();
    UserResourceController();
    void renderViewCreated(content::RenderView *);
    void renderViewDestroyed(content::RenderView *);
    void addScriptForView(const UserScriptData &, content::RenderView *);
    void removeScriptForView(const UserScriptData &, content::RenderView *);
    void clearScriptsForView(content::RenderView *);

    void RunScriptsAtDocumentStart(content::RenderFrame *render_frame);
    void RunScriptsAtDocumentEnd(content::RenderFrame *render_frame);

private:
    Q_DISABLE_COPY(UserResourceController)

    class RenderViewObserverHelper;

    // RenderProcessObserver implementation.
    bool OnControlMessageReceived(const IPC::Message &message) override;

    void onAddScript(const UserScriptData &);
    void onRemoveScript(const UserScriptData &);
    void onClearScripts();

    void runScripts(UserScriptData::InjectionPoint, blink::WebLocalFrame *);

    typedef QSet<uint64_t> UserScriptSet;
    typedef QHash<const content::RenderView *, UserScriptSet> ViewUserScriptMap;
    ViewUserScriptMap m_viewUserScriptMap;
    QHash<uint64_t, UserScriptData> m_scripts;

    friend class RenderViewObserverHelper;
};

#endif // USER_RESOURCE_CONTROLLER_H
