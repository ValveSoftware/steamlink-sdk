/****************************************************************************
**
** Copyright (C) 2013 Research In Motion
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "bpsmediaplayercontrol.h"
#include "mmrenderervideowindowcontrol.h"

#include <bps/mmrenderer.h>
#include <bps/screen.h>

QT_BEGIN_NAMESPACE

BpsMediaPlayerControl::BpsMediaPlayerControl(QObject *parent)
    : MmRendererMediaPlayerControl(parent),
      m_eventMonitor(0)
{
    openConnection();
}

BpsMediaPlayerControl::~BpsMediaPlayerControl()
{
    destroy();
}

void BpsMediaPlayerControl::startMonitoring(int contextId, const QString &contextName)
{
    m_eventMonitor = mmrenderer_request_events(contextName.toLatin1().constData(), 0, contextId);
    if (!m_eventMonitor) {
        qDebug() << "Unable to request multimedia events";
        emit error(0, "Unable to request multimedia events");
    }
}

void BpsMediaPlayerControl::stopMonitoring()
{
    if (m_eventMonitor) {
        mmrenderer_stop_events(m_eventMonitor);
        m_eventMonitor = 0;
    }
}

bool BpsMediaPlayerControl::nativeEventFilter(const QByteArray &eventType, void *message, long *result)
{
    Q_UNUSED(result)
    Q_UNUSED(eventType)

    bps_event_t * const event = static_cast<bps_event_t *>(message);
    if (!event ||
        (bps_event_get_domain(event) != mmrenderer_get_domain() &&
         bps_event_get_domain(event) != screen_get_domain()))
        return false;

    if (event && bps_event_get_domain(event) == screen_get_domain()) {
        const screen_event_t screen_event = screen_event_get_event(event);
        if (MmRendererVideoWindowControl *control = videoWindowControl())
            control->screenEventHandler(screen_event);
    }

    if (bps_event_get_domain(event) == mmrenderer_get_domain()) {
        if (bps_event_get_code(event) == MMRENDERER_STATE_CHANGE) {
            const mmrenderer_state_t newState = mmrenderer_event_get_state(event);
            if (newState == MMR_STOPPED) {
                handleMmStopped();
                return false;
            }
        }

        if (bps_event_get_code(event) == MMRENDERER_STATUS_UPDATE) {
            const qint64 newPosition = QString::fromLatin1(mmrenderer_event_get_position(event)).
                                       toLongLong();
            handleMmStatusUpdate(newPosition);

            const QString status = QString::fromLatin1(mmrenderer_event_get_bufferstatus(event));
            setMmBufferStatus(status);

            const QString level = QString::fromLatin1(mmrenderer_event_get_bufferlevel(event));
            setMmBufferLevel(level);
        }
    }

    return false;
}

QT_END_NAMESPACE
