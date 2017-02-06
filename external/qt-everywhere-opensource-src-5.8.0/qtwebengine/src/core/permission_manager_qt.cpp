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

#include "permission_manager_qt.h"

#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/public/browser/permission_type.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"

#include "type_conversion.h"
#include "web_contents_delegate_qt.h"

namespace QtWebEngineCore {

BrowserContextAdapter::PermissionType toQt(content::PermissionType type)
{
    switch (type) {
    case content::PermissionType::GEOLOCATION:
        return BrowserContextAdapter::GeolocationPermission;
    case content::PermissionType::AUDIO_CAPTURE:
        return BrowserContextAdapter::AudioCapturePermission;
    case content::PermissionType::VIDEO_CAPTURE:
        return BrowserContextAdapter::VideoCapturePermission;
    case content::PermissionType::NOTIFICATIONS:
    case content::PermissionType::MIDI_SYSEX:
    case content::PermissionType::PUSH_MESSAGING:
    case content::PermissionType::PROTECTED_MEDIA_IDENTIFIER:
    case content::PermissionType::MIDI:
    case content::PermissionType::DURABLE_STORAGE:
    case content::PermissionType::BACKGROUND_SYNC:
    case content::PermissionType::NUM:
        break;
    }
    return BrowserContextAdapter::UnsupportedPermission;
}

PermissionManagerQt::PermissionManagerQt()
    : m_requestIdCount(0)
    , m_subscriberIdCount(0)
{
}

PermissionManagerQt::~PermissionManagerQt()
{
}

void PermissionManagerQt::permissionRequestReply(const QUrl &origin, BrowserContextAdapter::PermissionType type, bool reply)
{
    QPair<QUrl, BrowserContextAdapter::PermissionType> key(origin, type);
    m_permissions[key] = reply;
    blink::mojom::PermissionStatus status = reply ? blink::mojom::PermissionStatus::GRANTED : blink::mojom::PermissionStatus::DENIED;
    auto it = m_requests.begin();
    while (it != m_requests.end()) {
        if (it->origin == origin && it->type == type) {
            it->callback.Run(status);
            it = m_requests.erase(it);
        } else
            ++it;
    }
    Q_FOREACH (const RequestOrSubscription &subscriber, m_subscribers) {
        if (subscriber.origin == origin && subscriber.type == type)
            subscriber.callback.Run(status);
    }
}

bool PermissionManagerQt::checkPermission(const QUrl &origin, BrowserContextAdapter::PermissionType type)
{
    QPair<QUrl, BrowserContextAdapter::PermissionType> key(origin, type);
    return m_permissions.contains(key) && m_permissions[key];
}

int PermissionManagerQt::RequestPermission(content::PermissionType permission,
                                            content::RenderFrameHost *frameHost,
                                            const GURL& requesting_origin,
                                            const base::Callback<void(blink::mojom::PermissionStatus)>& callback)
{
    int request_id = ++m_requestIdCount;
    BrowserContextAdapter::PermissionType permissionType = toQt(permission);
    if (permissionType == BrowserContextAdapter::UnsupportedPermission) {
        callback.Run(blink::mojom::PermissionStatus::DENIED);
        return kNoPendingOperation;
    }
    // Audio and video-capture should not come this way currently
    Q_ASSERT(permissionType != BrowserContextAdapter::AudioCapturePermission
          && permissionType != BrowserContextAdapter::VideoCapturePermission);

    content::WebContents *webContents = frameHost->GetRenderViewHost()->GetDelegate()->GetAsWebContents();
    WebContentsDelegateQt* contentsDelegate = static_cast<WebContentsDelegateQt*>(webContents->GetDelegate());
    Q_ASSERT(contentsDelegate);
    RequestOrSubscription request = {
        permissionType,
        toQt(requesting_origin),
        callback
    };
    m_requests.insert(request_id, request);
    if (permissionType == BrowserContextAdapter::GeolocationPermission)
        contentsDelegate->requestGeolocationPermission(request.origin);
    return request_id;
}

int PermissionManagerQt::RequestPermissions(const std::vector<content::PermissionType>& permissions,
                                            content::RenderFrameHost* frameHost,
                                            const GURL& requesting_origin,
                                            const base::Callback<void(const std::vector<blink::mojom::PermissionStatus>&)>& callback)
{
    NOTIMPLEMENTED() << "RequestPermissions has not been implemented in QtWebEngine";
    Q_UNUSED(frameHost);

    std::vector<blink::mojom::PermissionStatus> result(permissions.size());
    for (content::PermissionType permission : permissions) {
        const BrowserContextAdapter::PermissionType permissionType = toQt(permission);
        if (permissionType == BrowserContextAdapter::UnsupportedPermission)
            result.push_back(blink::mojom::PermissionStatus::DENIED);
        else {
            QPair<QUrl, BrowserContextAdapter::PermissionType> key(toQt(requesting_origin), permissionType);
            // TODO: Request permission from UI
            if (m_permissions.contains(key) && m_permissions[key])
                result.push_back(blink::mojom::PermissionStatus::GRANTED);
            else
                result.push_back(blink::mojom::PermissionStatus::DENIED);
        }
    }

    callback.Run(result);
    return kNoPendingOperation;
}

void PermissionManagerQt::CancelPermissionRequest(int request_id)
{
    // Should we add API to cancel permissions in the UI level?
    m_requests.remove(request_id);
}

blink::mojom::PermissionStatus PermissionManagerQt::GetPermissionStatus(
    content::PermissionType permission,
    const GURL& requesting_origin,
    const GURL& /*embedding_origin*/)
{
    const BrowserContextAdapter::PermissionType permissionType = toQt(permission);
    if (permissionType == BrowserContextAdapter::UnsupportedPermission)
        return blink::mojom::PermissionStatus::DENIED;

    QPair<QUrl, BrowserContextAdapter::PermissionType> key(toQt(requesting_origin), permissionType);
    if (!m_permissions.contains(key))
        return blink::mojom::PermissionStatus::ASK;
    if (m_permissions[key])
        return blink::mojom::PermissionStatus::GRANTED;
    return blink::mojom::PermissionStatus::DENIED;
}

void PermissionManagerQt::ResetPermission(
    content::PermissionType permission,
    const GURL& requesting_origin,
    const GURL& /*embedding_origin*/)
{
    const BrowserContextAdapter::PermissionType permissionType = toQt(permission);
    if (permissionType == BrowserContextAdapter::UnsupportedPermission)
        return;

    QPair<QUrl, BrowserContextAdapter::PermissionType> key(toQt(requesting_origin), permissionType);
    m_permissions.remove(key);
}

void PermissionManagerQt::RegisterPermissionUsage(
    content::PermissionType /*permission*/,
    const GURL& /*requesting_origin*/,
    const GURL& /*embedding_origin*/)
{
    // We do not currently track which permissions are used.
}

int PermissionManagerQt::SubscribePermissionStatusChange(
    content::PermissionType permission,
    const GURL& requesting_origin,
    const GURL& /*embedding_origin*/,
    const base::Callback<void(blink::mojom::PermissionStatus)>& callback)
{
    int subscriber_id = ++m_subscriberIdCount;
    RequestOrSubscription subscriber = {
        toQt(permission),
        toQt(requesting_origin),
        callback
    };
    m_subscribers.insert(subscriber_id, subscriber);
    return subscriber_id;
}

void PermissionManagerQt::UnsubscribePermissionStatusChange(int subscription_id)
{
    if (!m_subscribers.remove(subscription_id))
        qWarning() << "PermissionManagerQt::UnsubscribePermissionStatusChange called on unknown subscription id" << subscription_id;
}

} // namespace QtWebEngineCore
