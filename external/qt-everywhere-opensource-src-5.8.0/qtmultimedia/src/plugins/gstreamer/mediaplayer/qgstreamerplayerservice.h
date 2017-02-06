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

#ifndef QGSTREAMERPLAYERSERVICE_H
#define QGSTREAMERPLAYERSERVICE_H

#include <QtCore/qobject.h>
#include <QtCore/qiodevice.h>

#include <qmediaservice.h>

QT_BEGIN_NAMESPACE
class QMediaPlayerControl;
class QMediaPlaylist;
class QMediaPlaylistNavigator;

class QGstreamerMetaData;
class QGstreamerPlayerControl;
class QGstreamerPlayerSession;
class QGstreamerMetaDataProvider;
class QGstreamerStreamsControl;
class QGstreamerVideoRenderer;
class QGstreamerVideoWindow;
class QGstreamerVideoWidgetControl;
class QGStreamerAvailabilityControl;
class QGstreamerAudioProbeControl;
class QGstreamerVideoProbeControl;

class QGstreamerPlayerService : public QMediaService
{
    Q_OBJECT
public:
    QGstreamerPlayerService(QObject *parent = 0);
    ~QGstreamerPlayerService();

    QMediaControl *requestControl(const char *name);
    void releaseControl(QMediaControl *control);

private:
    QGstreamerPlayerControl *m_control;
    QGstreamerPlayerSession *m_session;
    QGstreamerMetaDataProvider *m_metaData;
    QGstreamerStreamsControl *m_streamsControl;
    QGStreamerAvailabilityControl *m_availabilityControl;

    QGstreamerAudioProbeControl *m_audioProbeControl;
    QGstreamerVideoProbeControl *m_videoProbeControl;

    QMediaControl *m_videoOutput;
    QMediaControl *m_videoRenderer;
    QGstreamerVideoWindow *m_videoWindow;
#if defined(HAVE_WIDGETS)
    QGstreamerVideoWidgetControl *m_videoWidget;
#endif

    void increaseVideoRef();
    void decreaseVideoRef();
    int m_videoReferenceCount;
};

QT_END_NAMESPACE

#endif
