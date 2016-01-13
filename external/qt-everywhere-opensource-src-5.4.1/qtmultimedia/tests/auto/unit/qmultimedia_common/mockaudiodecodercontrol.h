/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
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

#ifndef MOCKAUDIODECODERCONTROL_H
#define MOCKAUDIODECODERCONTROL_H

#include "qmediacontrol.h"
#include "qaudiodecodercontrol.h"

#include <QtCore/qpair.h>

#include "qaudiobuffer.h"
#include <QTimer>
#include <QIODevice>

#define MOCK_DECODER_MAX_BUFFERS 10

QT_BEGIN_NAMESPACE

class MockAudioDecoderControl : public QAudioDecoderControl
{
    Q_OBJECT

public:
    MockAudioDecoderControl(QObject *parent = 0)
        : QAudioDecoderControl(parent)
        , mState(QAudioDecoder::StoppedState)
        , mDevice(0)
        , mPosition(-1)
        , mSerial(0)
    {
        mFormat.setChannelCount(1);
        mFormat.setSampleSize(8);
        mFormat.setSampleRate(1000);
        mFormat.setCodec("audio/x-raw");
        mFormat.setSampleType(QAudioFormat::UnSignedInt);
    }

    QAudioDecoder::State state() const
    {
        return mState;
    }

    QString sourceFilename() const
    {
        return mSource;
    }

    QAudioFormat audioFormat() const
    {
        return mFormat;
    }

    void setAudioFormat(const QAudioFormat &format)
    {
        if (mFormat != format) {
            mFormat = format;
            emit formatChanged(mFormat);
        }
    }

    void setSourceFilename(const QString &fileName)
    {
        mSource = fileName;
        mDevice = 0;
        stop();
    }

    QIODevice* sourceDevice() const
    {
        return mDevice;
    }

    void setSourceDevice(QIODevice *device)
    {
        mDevice = device;
        mSource.clear();
        stop();
    }

    // When decoding we decode to first buffer, then second buffer
    // we then stop until the first is read again and so on, for
    // 5 buffers
    void start()
    {
        if (mState == QAudioDecoder::StoppedState) {
            if (!mSource.isEmpty()) {
                mState = QAudioDecoder::DecodingState;
                emit stateChanged(mState);
                emit durationChanged(duration());

                QTimer::singleShot(50, this, SLOT(pretendDecode()));
            } else {
                emit error(QAudioDecoder::ResourceError, "No source set");
            }
        }
    }

    void stop()
    {
        if (mState != QAudioDecoder::StoppedState) {
            mState = QAudioDecoder::StoppedState;
            mSerial = 0;
            mPosition = 0;
            mBuffers.clear();
            emit stateChanged(mState);
            emit bufferAvailableChanged(false);
        }
    }

    QAudioBuffer read()
    {
        QAudioBuffer a;
        if (mBuffers.length() > 0) {
            a = mBuffers.takeFirst();
            mPosition = a.startTime() / 1000;
            emit positionChanged(mPosition);

            if (mBuffers.isEmpty())
                emit bufferAvailableChanged(false);

            if (mBuffers.isEmpty() && mSerial >= MOCK_DECODER_MAX_BUFFERS) {
                mState = QAudioDecoder::StoppedState;
                emit finished();
                emit stateChanged(mState);
            } else
                QTimer::singleShot(50, this, SLOT(pretendDecode()));
        }

        return a;
    }

    bool bufferAvailable() const
    {
        return mBuffers.length() > 0;
    }

    qint64 position() const
    {
        return mPosition;
    }

    qint64 duration() const
    {
        return (sizeof(mSerial) * MOCK_DECODER_MAX_BUFFERS * qint64(1000)) / (mFormat.sampleRate() * mFormat.channelCount());
    }

private slots:
    void pretendDecode()
    {
        // Check if we've reached end of stream
        if (mSerial >= MOCK_DECODER_MAX_BUFFERS)
            return;

        // We just keep the length of mBuffers to 3 or less.
        if (mBuffers.length() < 3) {
            QByteArray b(sizeof(mSerial), 0);
            memcpy(b.data(), &mSerial, sizeof(mSerial));
            qint64 position = (sizeof(mSerial) * mSerial * qint64(1000000)) / (mFormat.sampleRate() * mFormat.channelCount());
            mSerial++;
            mBuffers.push_back(QAudioBuffer(b, mFormat, position));
            emit bufferReady();
            if (mBuffers.count() == 1)
                emit bufferAvailableChanged(true);
        }
    }

public:
    QAudioDecoder::State mState;
    QString mSource;
    QIODevice *mDevice;
    QAudioFormat mFormat;
    qint64 mPosition;

    int mSerial;
    QList<QAudioBuffer> mBuffers;
};

QT_END_NAMESPACE

#endif  // QAUDIODECODERCONTROL_H
