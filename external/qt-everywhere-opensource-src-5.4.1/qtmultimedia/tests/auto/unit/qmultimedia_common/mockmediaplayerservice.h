/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the test suite of the Qt Toolkit.
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

#ifndef MOCKPLAYERSERVICE_H
#define MOCKPLAYERSERVICE_H

#include "qmediaservice.h"

#include "mockmediaplayercontrol.h"
#include "mockmediastreamscontrol.h"
#include "mockmedianetworkaccesscontrol.h"
#include "mockvideorenderercontrol.h"
#include "mockvideoprobecontrol.h"
#include "mockvideowindowcontrol.h"

class MockMediaPlayerService : public QMediaService
{
    Q_OBJECT

public:
    MockMediaPlayerService():QMediaService(0)
    {
        mockControl = new MockMediaPlayerControl;
        mockStreamsControl = new MockStreamsControl;
        mockNetworkControl = new MockNetworkAccessControl;
        rendererControl = new MockVideoRendererControl;
        rendererRef = 0;
        mockVideoProbeControl = new MockVideoProbeControl;
        windowControl = new MockVideoWindowControl;
        windowRef = 0;
    }

    ~MockMediaPlayerService()
    {
        delete mockControl;
        delete mockStreamsControl;
        delete mockNetworkControl;
        delete rendererControl;
        delete mockVideoProbeControl;
        delete windowControl;
    }

    QMediaControl* requestControl(const char *iid)
    {
        if (qstrcmp(iid, QMediaPlayerControl_iid) == 0) {
            return mockControl;
        } else if (qstrcmp(iid, QVideoRendererControl_iid) == 0) {
            if (rendererRef == 0) {
                rendererRef += 1;
                return rendererControl;
            }
        } else if (qstrcmp(iid, QMediaVideoProbeControl_iid) == 0) {
            return mockVideoProbeControl;
        }
        if (qstrcmp(iid, QVideoWindowControl_iid) == 0) {
            if (windowRef == 0) {
                windowRef += 1;
                return windowControl;
            }
        }

        if (qstrcmp(iid, QMediaNetworkAccessControl_iid) == 0)
            return mockNetworkControl;
        return 0;
    }

    void releaseControl(QMediaControl *control)
    {
        if (control == rendererControl)
            rendererRef -= 1;
        if (control == windowControl)
            windowRef -= 1;
    }

    void setState(QMediaPlayer::State state) { emit mockControl->stateChanged(mockControl->_state = state); }
    void setState(QMediaPlayer::State state, QMediaPlayer::MediaStatus status) {
        mockControl->_state = state;
        mockControl->_mediaStatus = status;
        emit mockControl->mediaStatusChanged(status);
        emit mockControl->stateChanged(state);
    }
    void setMediaStatus(QMediaPlayer::MediaStatus status) { emit mockControl->mediaStatusChanged(mockControl->_mediaStatus = status); }
    void setIsValid(bool isValid) { mockControl->_isValid = isValid; }
    void setMedia(QMediaContent media) { mockControl->_media = media; }
    void setDuration(qint64 duration) { mockControl->_duration = duration; }
    void setPosition(qint64 position) { mockControl->_position = position; }
    void setSeekable(bool seekable) { mockControl->_isSeekable = seekable; }
    void setVolume(int volume) { mockControl->_volume = volume; }
    void setMuted(bool muted) { mockControl->_muted = muted; }
    void setVideoAvailable(bool videoAvailable) { mockControl->_videoAvailable = videoAvailable; }
    void setBufferStatus(int bufferStatus) { mockControl->_bufferStatus = bufferStatus; }
    void setPlaybackRate(qreal playbackRate) { mockControl->_playbackRate = playbackRate; }
    void setError(QMediaPlayer::Error error) { mockControl->_error = error; emit mockControl->error(mockControl->_error, mockControl->_errorString); }
    void setErrorString(QString errorString) { mockControl->_errorString = errorString; emit mockControl->error(mockControl->_error, mockControl->_errorString); }

    void selectCurrentConfiguration(QNetworkConfiguration config) { mockNetworkControl->setCurrentConfiguration(config); }

    void reset()
    {
        mockControl->_state = QMediaPlayer::StoppedState;
        mockControl->_mediaStatus = QMediaPlayer::UnknownMediaStatus;
        mockControl->_error = QMediaPlayer::NoError;
        mockControl->_duration = 0;
        mockControl->_position = 0;
        mockControl->_volume = 0;
        mockControl->_muted = false;
        mockControl->_bufferStatus = 0;
        mockControl->_videoAvailable = false;
        mockControl->_isSeekable = false;
        mockControl->_playbackRate = 0.0;
        mockControl->_media = QMediaContent();
        mockControl->_stream = 0;
        mockControl->_isValid = false;
        mockControl->_errorString = QString();

        mockNetworkControl->_current = QNetworkConfiguration();
        mockNetworkControl->_configurations = QList<QNetworkConfiguration>();
    }

    MockMediaPlayerControl *mockControl;
    MockStreamsControl *mockStreamsControl;
    MockNetworkAccessControl *mockNetworkControl;
    MockVideoRendererControl *rendererControl;
    MockVideoProbeControl *mockVideoProbeControl;
    MockVideoWindowControl *windowControl;
    int windowRef;
    int rendererRef;
};



#endif // MOCKPLAYERSERVICE_H
