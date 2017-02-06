/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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

#include "qgstreameraudiodecodercontrol.h"
#include "qgstreameraudiodecodersession.h"

#include <QtCore/qdir.h>
#include <QtCore/qsocketnotifier.h>
#include <QtCore/qurl.h>
#include <QtCore/qdebug.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

QT_BEGIN_NAMESPACE

QGstreamerAudioDecoderControl::QGstreamerAudioDecoderControl(QGstreamerAudioDecoderSession *session, QObject *parent)
    : QAudioDecoderControl(parent)
    , m_session(session)
{
    connect(m_session, SIGNAL(bufferAvailableChanged(bool)), this, SIGNAL(bufferAvailableChanged(bool)));
    connect(m_session, SIGNAL(bufferReady()), this, SIGNAL(bufferReady()));
    connect(m_session, SIGNAL(error(int,QString)), this, SIGNAL(error(int,QString)));
    connect(m_session, SIGNAL(formatChanged(QAudioFormat)), this, SIGNAL(formatChanged(QAudioFormat)));
    connect(m_session, SIGNAL(sourceChanged()), this, SIGNAL(sourceChanged()));
    connect(m_session, SIGNAL(stateChanged(QAudioDecoder::State)), this, SIGNAL(stateChanged(QAudioDecoder::State)));
    connect(m_session, SIGNAL(finished()), this, SIGNAL(finished()));
    connect(m_session, SIGNAL(positionChanged(qint64)), this, SIGNAL(positionChanged(qint64)));
    connect(m_session, SIGNAL(durationChanged(qint64)), this, SIGNAL(durationChanged(qint64)));
}

QGstreamerAudioDecoderControl::~QGstreamerAudioDecoderControl()
{

}

QAudioDecoder::State QGstreamerAudioDecoderControl::state() const
{
    return m_session->pendingState();
}

QString QGstreamerAudioDecoderControl::sourceFilename() const
{
    return m_session->sourceFilename();
}

void QGstreamerAudioDecoderControl::setSourceFilename(const QString &fileName)
{
    m_session->setSourceFilename(fileName);
}

QIODevice* QGstreamerAudioDecoderControl::sourceDevice() const
{
    return m_session->sourceDevice();
}

void QGstreamerAudioDecoderControl::setSourceDevice(QIODevice *device)
{
    m_session->setSourceDevice(device);
}

void QGstreamerAudioDecoderControl::start()
{
    m_session->start();
}

void QGstreamerAudioDecoderControl::stop()
{
    m_session->stop();
}

QAudioFormat QGstreamerAudioDecoderControl::audioFormat() const
{
    return m_session->audioFormat();
}

void QGstreamerAudioDecoderControl::setAudioFormat(const QAudioFormat &format)
{
    m_session->setAudioFormat(format);
}

QAudioBuffer QGstreamerAudioDecoderControl::read()
{
    return m_session->read();
}

bool QGstreamerAudioDecoderControl::bufferAvailable() const
{
    return m_session->bufferAvailable();
}

qint64 QGstreamerAudioDecoderControl::position() const
{
    return m_session->position();
}

qint64 QGstreamerAudioDecoderControl::duration() const
{
    return m_session->duration();
}

QT_END_NAMESPACE
