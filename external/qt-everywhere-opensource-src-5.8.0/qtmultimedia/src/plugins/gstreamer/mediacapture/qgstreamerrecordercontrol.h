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


#ifndef QGSTREAMERRECORDERCONTROL_H
#define QGSTREAMERRECORDERCONTROL_H

#include <QtCore/QDir>

#include <qmediarecordercontrol.h>
#include "qgstreamercapturesession.h"

QT_BEGIN_NAMESPACE

class QGstreamerRecorderControl : public QMediaRecorderControl
{
    Q_OBJECT

public:
    QGstreamerRecorderControl(QGstreamerCaptureSession *session);
    virtual ~QGstreamerRecorderControl();

    QUrl outputLocation() const;
    bool setOutputLocation(const QUrl &sink);

    QMediaRecorder::State state() const;
    QMediaRecorder::Status status() const;

    qint64 duration() const;

    bool isMuted() const;
    qreal volume() const;

    void applySettings();

public slots:
    void setState(QMediaRecorder::State state);
    void record();
    void pause();
    void stop();
    void setMuted(bool);
    void setVolume(qreal volume);

private slots:
    void updateStatus();
    void handleSessionError(int code, const QString &description);

private:
    QDir defaultDir() const;
    QString generateFileName(const QDir &dir, const QString &ext) const;

    QUrl m_outputLocation;
    QGstreamerCaptureSession *m_session;
    QMediaRecorder::State m_state;
    QMediaRecorder::Status m_status;
    bool m_hasPreviewState;
};

QT_END_NAMESPACE

#endif // QGSTREAMERCAPTURECORNTROL_H
