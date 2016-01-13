/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "avfmediaplayerservice.h"
#include "avfmediaplayersession.h"
#include "avfmediaplayercontrol.h"
#include "avfmediaplayermetadatacontrol.h"
#if defined(Q_OS_OSX)
# include "avfvideooutput.h"
# include "avfvideorenderercontrol.h"
#endif
#ifndef QT_NO_WIDGETS
# include "avfvideowidgetcontrol.h"
#endif
#include "avfvideowindowcontrol.h"

QT_USE_NAMESPACE

AVFMediaPlayerService::AVFMediaPlayerService(QObject *parent)
    : QMediaService(parent)
    , m_videoOutput(0)
{
    m_session = new AVFMediaPlayerSession(this);
    m_control = new AVFMediaPlayerControl(this);
    m_control->setSession(m_session);
    m_playerMetaDataControl = new AVFMediaPlayerMetaDataControl(m_session, this);

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
#if defined(Q_OS_OSX)
    if (qstrcmp(name, QVideoRendererControl_iid) == 0) {
        if (!m_videoOutput)
            m_videoOutput = new AVFVideoRendererControl(this);

        m_session->setVideoOutput(qobject_cast<AVFVideoOutput*>(m_videoOutput));
        return m_videoOutput;
    }
#endif
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
#if defined(Q_OS_OSX)
        AVFVideoRendererControl *renderControl = qobject_cast<AVFVideoRendererControl*>(m_videoOutput);
        if (renderControl)
            renderControl->setSurface(0);
#endif
        m_videoOutput = 0;
        m_session->setVideoOutput(0);

        delete control;
    }
}
