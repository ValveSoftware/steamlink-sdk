/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the plugins of the Qt Toolkit.
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

#ifndef IOSAUDIOSESSIONMANAGER_H
#define IOSAUDIOSESSIONMANAGER_H

#include <QObject>
#ifdef QT_DEBUG_COREAUDIO
# include <QtCore/QDebug>
#endif

@class CoreAudioSessionObserver;

QT_BEGIN_NAMESPACE

class CoreAudioSessionManager : public QObject
{
    Q_OBJECT
public:
    enum AudioSessionCategorys {
        Ambient,
        SoloAmbient,
        Playback,
        Record,
        PlayAndRecord,
        AudioProcessing,
        MultiRoute
    };
    enum AudioSessionCategoryOptions {
        None = 0,
        MixWithOthers = 1,
        DuckOthers = 2,
        AllowBluetooth = 4,
        DefaultToSpeaker = 8
    };
    enum AudioSessionModes {
        Default,
        VoiceChat,
        GameChat,
        VideoRecording,
        Measurement,
        MoviePlayback
    };

    static CoreAudioSessionManager& instance();

    bool setActive(bool active);
    bool setCategory(AudioSessionCategorys category, AudioSessionCategoryOptions options = None);
    bool setMode(AudioSessionModes mode);

    AudioSessionCategorys category();
    AudioSessionModes mode();

    QList<QByteArray> inputDevices();
    QList<QByteArray> outputDevices();

    float currentIOBufferDuration();
    float preferredSampleRate();

signals:
    void activeChanged();
    void categoryChanged();
    void modeChanged();
    void routeChanged();
    void inputDevicesAvailableChanged();
    void outputDevicesAvailableChanged();

private:
    CoreAudioSessionManager();
    ~CoreAudioSessionManager();
    CoreAudioSessionManager(CoreAudioSessionManager const &copy);
    CoreAudioSessionManager& operator =(CoreAudioSessionManager const &copy);

    CoreAudioSessionObserver *m_sessionObserver;
};

#ifdef QT_DEBUG_COREAUDIO
QDebug operator <<(QDebug dbg, CoreAudioSessionManager::AudioSessionCategorys category);
QDebug operator <<(QDebug dbg, CoreAudioSessionManager::AudioSessionCategoryOptions option);
QDebug operator <<(QDebug dbg, CoreAudioSessionManager::AudioSessionModes mode);
#endif

QT_END_NAMESPACE

Q_DECLARE_METATYPE(CoreAudioSessionManager::AudioSessionCategorys)
Q_DECLARE_METATYPE(CoreAudioSessionManager::AudioSessionCategoryOptions)
Q_DECLARE_METATYPE(CoreAudioSessionManager::AudioSessionModes)

#endif // IOSAUDIOSESSIONMANAGER_H
