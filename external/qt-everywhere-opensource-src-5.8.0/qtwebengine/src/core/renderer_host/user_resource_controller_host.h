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

#ifndef USER_RESOURCE_CONTROLLER_HOST_H
#define USER_RESOURCE_CONTROLLER_HOST_H

#include "qtwebenginecoreglobal.h"

#include <QtCore/QSet>
#include <QtCore/QScopedPointer>
#include "user_script.h"

namespace content {
class RenderProcessHost;
class WebContents;
}

namespace QtWebEngineCore {

class WebContentsAdapter;
class WebContentsAdapterPrivate;

class QWEBENGINE_EXPORT UserResourceControllerHost {

public:
    UserResourceControllerHost();
    ~UserResourceControllerHost();

    void addUserScript(const UserScript &script, WebContentsAdapter *adapter);
    bool containsUserScript(const UserScript &script, WebContentsAdapter *adapter);
    bool removeUserScript(const UserScript &script, WebContentsAdapter *adapter);
    void clearAllScripts(WebContentsAdapter *adapter);
    void reserve(WebContentsAdapter *adapter, int count);
    const QList<UserScript> registeredScripts(WebContentsAdapter *adapter) const;

    void renderProcessStartedWithHost(content::RenderProcessHost *renderer);

private:
    Q_DISABLE_COPY(UserResourceControllerHost)
    class WebContentsObserverHelper;
    class RenderProcessObserverHelper;

    void webContentsDestroyed(content::WebContents *);

    QList<UserScript> m_profileWideScripts;
    typedef QHash<content::WebContents *, QList<UserScript>> ContentsScriptsMap;
    ContentsScriptsMap m_perContentsScripts;
    QSet<content::RenderProcessHost *> m_observedProcesses;
    QScopedPointer<RenderProcessObserverHelper> m_renderProcessObserver;
};

} // namespace

#endif // USER_RESOURCE_CONTROLLER_HOST_H
