/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd and/or its subsidiary(-ies).
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Toolkit.
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

#include "avfmediaplayerservice.h"
#include "avfmediaplayersession.h"
#include "avfmediaplayercontrol.h"
#include "avfmediaplayermetadatacontrol.h"
#include "avfvideooutput.h"
#include "avfvideorenderercontrol.h"
#ifndef QT_NO_WIDGETS
# include "avfvideowidgetcontrol.h"
#endif
#include "avfvideowindowcontrol.h"

#if QT_MAC_PLATFORM_SDK_EQUAL_OR_ABOVE(__MAC_10_8, __IPHONE_6_0)
#import <AVFoundation/AVFoundation.h>
#endif

QT_USE_NAMESPACE

AVFMediaPlayerService::AVFMediaPlayerService(QObject *parent)
    : QMediaService(parent)
    , m_videoOutput(0)
    , m_enableRenderControl(true)
{
    m_session = new AVFMediaPlayerSession(this);
    m_control = new AVFMediaPlayerControl(this);
    m_control->setSession(m_session);
    m_playerMetaDataControl = new AVFMediaPlayerMetaDataControl(m_session, this);

#if QT_MAC_PLATFORM_SDK_EQUAL_OR_ABOVE(__MAC_10_8, __IPHONE_6_0)
    // AVPlayerItemVideoOutput is available in SDK
    #if QT_MAC_DEPLOYMENT_TARGET_BELOW(__MAC_10_8, __IPHONE_6_0)
    // might not be available at runtime
        #if defined(Q_OS_IOS) || defined(Q_OS_TVOS)
            m_enableRenderControl = [AVPlayerItemVideoOutput class] != 0;
        #endif
    #endif
#endif

    connect(m_control, SIGNAL(mediaChanged(QMediaContent)), m_playerMetaDataControl, SLOT(updateTags()));
}

AVFMediaPlayerService::~AVFMediaPlayerService()
{
#ifdef QT_DEBUG_AVF
    qDebug() << Q_FUNC_INFO;
#endif
    delete m_session;
}

QMediaControl *AVFMediaPlayerService::requestControl(const char *name)
{
#ifdef QT_DEBUG_AVF
    qDebug() << Q_FUNC_INFO << name;
#endif

    if (qstrcmp(name, QMediaPlayerControl_iid) == 0)
        return m_control;

    if (qstrcmp(name, QMetaDataReaderControl_iid) == 0)
        return m_playerMetaDataControl;


    if (m_enableRenderControl && (qstrcmp(name, QVideoRendererControl_iid) == 0)) {
        if (!m_videoOutput)
            m_videoOutput = new AVFVideoRendererControl(this);

        m_session->setVideoOutput(qobject_cast<AVFVideoOutput*>(m_videoOutput));
        return m_videoOutput;
    }

#ifndef QT_NO_WIDGETS
    if (qstrcmp(name, QVideoWidgetControl_iid) == 0) {
        if (!m_videoOutput)
            m_videoOutput = new AVFVideoWidgetControl(this);

        m_session->setVideoOutput(qobject_cast<AVFVideoOutput*>(m_videoOutput));
        return m_videoOutput;
    }
#endif
    if (qstrcmp(name, QVideoWindowControl_iid) == 0) {
        if (!m_videoOutput)
            m_videoOutput = new AVFVideoWindowControl(this);

        m_session->setVideoOutput(qobject_cast<AVFVideoOutput*>(m_videoOutput));
        return m_videoOutput;
    }
    return 0;
}

void AVFMediaPlayerService::releaseControl(QMediaControl *control)
{
#ifdef QT_DEBUG_AVF
    qDebug() << Q_FUNC_INFO << control;
#endif
    if (m_videoOutput == control) {
        AVFVideoRendererControl *renderControl = qobject_cast<AVFVideoRendererControl*>(m_videoOutput);

        if (renderControl)
            renderControl->setSurface(0);

        m_videoOutput = 0;
        m_session->setVideoOutput(0);

        delete control;
    }
}
