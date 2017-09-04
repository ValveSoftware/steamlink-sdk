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

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists for the convenience
// of other Qt classes.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#ifndef QWINDOWSAUDIOOUTPUT_H
#define QWINDOWSAUDIOOUTPUT_H

#include "qwindowsaudioutils.h"

#include <QtCore/qdebug.h>
#include <QtCore/qtimer.h>
#include <QtCore/qstring.h>
#include <QtCore/qstringlist.h>
#include <QtCore/qdatetime.h>
#include <QtCore/qmutex.h>

#include <QtMultimedia/qaudio.h>
#include <QtMultimedia/qaudiodeviceinfo.h>
#include <QtMultimedia/qaudiosystem.h>

// For compat with 4.6
#if !defined(QT_WIN_CALLBACK)
#  if defined(Q_CC_MINGW)
#    define QT_WIN_CALLBACK CALLBACK __attribute__ ((force_align_arg_pointer))
#  else
#    define QT_WIN_CALLBACK CALLBACK
#  endif
#endif

QT_BEGIN_NAMESPACE

class QWindowsAudioOutput : public QAbstractAudioOutput
{
    Q_OBJECT
public:
    QWindowsAudioOutput(const QByteArray &device);
    ~QWindowsAudioOutput();

    qint64 write( const char *data, qint64 len );

    void setFormat(const QAudioFormat& fmt);
    QAudioFormat format() const;
    QIODevice* start();
    void start(QIODevice* device);
    void stop();
    void reset();
    void suspend();
    void resume();
    int bytesFree() const;
    int periodSize() const;
    void setBufferSize(int value);
    int bufferSize() const;
    void setNotifyInterval(int milliSeconds);
    int notifyInterval() const;
    qint64 processedUSecs() const;
    qint64 elapsedUSecs() const;
    QAudio::Error error() const;
    QAudio::State state() const;
    void setVolume(qreal);
    qreal volume() const;

    QIODevice* audioSource;
    QAudioFormat settings;
    QAudio::Error errorState;
    QAudio::State deviceState;

private slots:
    void feedback();
    bool deviceReady();

private:
    QByteArray m_device;
    bool resuming;
    int bytesAvailable;
    QTime timeStamp;
    qint64 elapsedTimeOffset;
    QTime timeStampOpened;
    qint32 buffer_size;
    qint32 period_size;
    qint64 totalTimeValue;
    bool pullMode;
    int intervalTime;
    qreal volumeCache;
    static void QT_WIN_CALLBACK waveOutProc( HWAVEOUT hWaveOut, UINT uMsg,
            DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2 );

    QMutex mutex;

    WAVEHDR* allocateBlocks(int size, int count);
    void freeBlocks(WAVEHDR* blockArray);
    bool open();
    void close();

    WAVEFORMATEXTENSIBLE wfx;
    HWAVEOUT hWaveOut;
    MMRESULT result;
    WAVEHDR header;
    WAVEHDR* waveBlocks;
    volatile bool finished;
    volatile int waveFreeBlockCount;
    int waveCurrentBlock;
    char* audioBuffer;
};

class OutputPrivate : public QIODevice
{
    Q_OBJECT
public:
    OutputPrivate(QWindowsAudioOutput* audio);
    ~OutputPrivate();

    qint64 readData( char* data, qint64 len);
    qint64 writeData(const char* data, qint64 len);

private:
    QWindowsAudioOutput *audioDevice;
};

QT_END_NAMESPACE


#endif // QWINDOWSAUDIOOUTPUT_H
