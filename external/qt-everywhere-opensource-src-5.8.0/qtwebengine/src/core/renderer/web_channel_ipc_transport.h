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

#ifndef WEB_CHANNEL_IPC_TRANSPORT_H
#define WEB_CHANNEL_IPC_TRANSPORT_H

#include "base/values.h"
#include "content/public/renderer/render_view_observer.h"
#include "content/public/renderer/render_view_observer_tracker.h"
#include <QtCore/qcompilerdetection.h>

namespace content {
class RenderFrame;
}

namespace v8 {
class Extension;
}

namespace QtWebEngineCore {

class WebChannelIPCTransport : public content::RenderViewObserver
                             , public content::RenderViewObserverTracker<WebChannelIPCTransport>
{
public:
    WebChannelIPCTransport(content::RenderView *);

    void RunScriptsAtDocumentStart(content::RenderFrame *render_frame);

private:
    void dispatchWebChannelMessage(const std::vector<char> &binaryJSON, uint worldId);
    void installWebChannel(uint worldId);
    void uninstallWebChannel(uint worldId);
    virtual bool OnMessageReceived(const IPC::Message &message) Q_DECL_OVERRIDE;
    virtual void OnDestruct() Q_DECL_OVERRIDE { }

    bool m_installed;
    uint m_installedWorldId;
};

} // namespace

#endif // WEB_CHANNEL_IPC_TRANSPORT
