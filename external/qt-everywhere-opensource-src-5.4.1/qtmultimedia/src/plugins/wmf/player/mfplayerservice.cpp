/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Mobility Components.
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

#include "qmediacontent.h"

#include <QtCore/qdebug.h>

#include "mfplayercontrol.h"
#if defined(HAVE_WIDGETS) && !defined(Q_WS_SIMULATOR)
#include "evr9videowindowcontrol.h"
#endif
#include "mfvideorenderercontrol.h"
#include "mfaudioendpointcontrol.h"
#include "mfaudioprobecontrol.h"
#include "mfvideoprobecontrol.h"
#include "mfplayerservice.h"
#include "mfplayersession.h"
#include "mfmetadatacontrol.h"

MFPlayerService::MFPlayerService(QObject *parent)
    : QMediaService(parent)
    , m_session(0)
#ifndef Q_WS_SIMULATOR
    , m_videoWindowControl(0)
#endif
    , m_videoRendererControl(0)
{
    m_audioEndpointControl = new MFAudioEndpointControl(this);
    m_session = new MFPlayerSession(this);
    m_player = new MFPlayerControl(m_session);
    m_metaDataControl = new MFMetaDataControl(this);
}

MFPlayerService::~MFPlayerService()
{
    m_session->close();

#ifndef Q_WS_SIMULATOR
    if (m_videoWindowControl)
        delete m_videoWindowControl;
#endif

    if (m_videoRendererControl)
        delete m_videoRendererControl;

    m_session->Release();
}

QMediaControl* MFPlayerService::requestControl(const char *name)
{
    if (qstrcmp(name, QMediaPlayerControl_iid) == 0) {
        return m_player;
    } else if (qstrcmp(name, QAudioOutputSelectorControl_iid) == 0) {
        return m_audioEndpointControl;
    } else if (qstrcmp(name, QMetaDataReaderControl_iid) == 0) {
        return m_metaDataControl;
    } else if (qstrcmp(name, QVideoRendererControl_iid) == 0) {
#if defined(HAVE_WIDGETS) && !defined(Q_WS_SIMULATOR)
        if (!m_videoRendererControl && !m_videoWindowControl) {
#else
        if (!m_videoRendererControl) {
#endif
            m_videoRendererControl = new MFVideoRendererControl;
            return m_videoRendererControl;
        }
#if defined(HAVE_WIDGETS) && !defined(Q_WS_SIMULATOR)
    } else if (qstrcmp(name, QVideoWindowControl_iid) == 0) {
        if (!m_videoRendererControl && !m_videoWindowControl) {
            m_videoWindowControl = new Evr9VideoWindowControl;
            return m_videoWindowControl;
        }
#endif
    } else if (qstrcmp(name,QMediaAudioProbeControl_iid) == 0) {
        if (m_session) {
            MFAudioProbeControl *probe = new MFAudioProbeControl(this);
            m_session->addProbe(probe);
            return probe;
        }
        return 0;
    } else if (qstrcmp(name,QMediaVideoProbeControl_iid) == 0) {
        if (m_session) {
            MFVideoProbeControl *probe = new MFVideoProbeControl(this);
            m_session->addProbe(probe);
            return probe;
        }
        return 0;
    }

    return 0;
}

void MFPlayerService::releaseControl(QMediaControl *control)
{
    if (!control) {
        qWarning("QMediaService::releaseControl():"
                " Attempted release of null control");
    } else if (control == m_videoRendererControl) {
        m_videoRendererControl->setSurface(0);
        delete m_videoRendererControl;
        m_videoRendererControl = 0;
        return;
#if defined(HAVE_WIDGETS) && !defined(Q_WS_SIMULATOR)
    } else if (control == m_videoWindowControl) {
        delete m_videoWindowControl;
        m_videoWindowControl = 0;
        return;
#endif
    }

    MFAudioProbeControl* audioProbe = qobject_cast<MFAudioProbeControl*>(control);
    if (audioProbe) {
        if (m_session)
            m_session->removeProbe(audioProbe);
        delete audioProbe;
        return;
    }

    MFVideoProbeControl* videoProbe = qobject_cast<MFVideoProbeControl*>(control);
    if (videoProbe) {
        if (m_session)
            m_session->removeProbe(videoProbe);
        delete videoProbe;
        return;
    }
}

MFAudioEndpointControl* MFPlayerService::audioEndpointControl() const
{
    return m_audioEndpointControl;
}

MFVideoRendererControl* MFPlayerService::videoRendererControl() const
{
    return m_videoRendererControl;
}

#if defined(HAVE_WIDGETS) && !defined(Q_WS_SIMULATOR)
Evr9VideoWindowControl* MFPlayerService::videoWindowControl() const
{
    return m_videoWindowControl;
}
#endif

MFMetaDataControl* MFPlayerService::metaDataControl() const
{
    return m_metaDataControl;
}
