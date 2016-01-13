/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the plugins of the Qt Toolkit.
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

#include "coreaudioutils.h"
#include <mach/mach_time.h>

QT_BEGIN_NAMESPACE

double CoreAudioUtils::sFrequency = 0.0;
bool CoreAudioUtils::sIsInitialized = false;

void CoreAudioUtils::initialize()
{
    struct mach_timebase_info timeBaseInfo;
    mach_timebase_info(&timeBaseInfo);
    sFrequency = static_cast<double>(timeBaseInfo.denom) / static_cast<double>(timeBaseInfo.numer);
    sFrequency *= 1000000000.0;

    sIsInitialized = true;
}


quint64 CoreAudioUtils::currentTime()
{
    return mach_absolute_time();
}

double CoreAudioUtils::frequency()
{
    if (!sIsInitialized)
        initialize();
    return sFrequency;
}

QAudioFormat CoreAudioUtils::toQAudioFormat(AudioStreamBasicDescription const& sf)
{
    QAudioFormat    audioFormat;

    audioFormat.setSampleRate(sf.mSampleRate);
    audioFormat.setChannelCount(sf.mChannelsPerFrame);
    audioFormat.setSampleSize(sf.mBitsPerChannel);
    audioFormat.setCodec(QString::fromLatin1("audio/pcm"));
    audioFormat.setByteOrder((sf.mFormatFlags & kAudioFormatFlagIsBigEndian) != 0 ? QAudioFormat::BigEndian : QAudioFormat::LittleEndian);
    QAudioFormat::SampleType type = QAudioFormat::UnSignedInt;
    if ((sf.mFormatFlags & kAudioFormatFlagIsSignedInteger) != 0)
        type = QAudioFormat::SignedInt;
    else if ((sf.mFormatFlags & kAudioFormatFlagIsFloat) != 0)
        type = QAudioFormat::Float;
    audioFormat.setSampleType(type);

    return audioFormat;
}

AudioStreamBasicDescription CoreAudioUtils::toAudioStreamBasicDescription(QAudioFormat const& audioFormat)
{
    AudioStreamBasicDescription sf;

    sf.mFormatFlags         = kAudioFormatFlagIsPacked;
    sf.mSampleRate          = audioFormat.sampleRate();
    sf.mFramesPerPacket     = 1;
    sf.mChannelsPerFrame    = audioFormat.channelCount();
    sf.mBitsPerChannel      = audioFormat.sampleSize();
    sf.mBytesPerFrame       = sf.mChannelsPerFrame * (sf.mBitsPerChannel / 8);
    sf.mBytesPerPacket      = sf.mFramesPerPacket * sf.mBytesPerFrame;
    sf.mFormatID            = kAudioFormatLinearPCM;

    switch (audioFormat.sampleType()) {
    case QAudioFormat::SignedInt: sf.mFormatFlags |= kAudioFormatFlagIsSignedInteger; break;
    case QAudioFormat::UnSignedInt: /* default */ break;
    case QAudioFormat::Float: sf.mFormatFlags |= kAudioFormatFlagIsFloat; break;
    case QAudioFormat::Unknown:  default: break;
    }

    if (audioFormat.byteOrder() == QAudioFormat::BigEndian)
        sf.mFormatFlags |= kAudioFormatFlagIsBigEndian;

    return sf;
}

// QAudioRingBuffer
CoreAudioRingBuffer::CoreAudioRingBuffer(int bufferSize):
    m_bufferSize(bufferSize)
{
    m_buffer = new char[m_bufferSize];
    reset();
}

CoreAudioRingBuffer::~CoreAudioRingBuffer()
{
    delete[] m_buffer;
}

CoreAudioRingBuffer::Region CoreAudioRingBuffer::acquireReadRegion(int size)
{
    const int used = m_bufferUsed.fetchAndAddAcquire(0);

    if (used > 0) {
        const int readSize = qMin(size, qMin(m_bufferSize - m_readPos, used));

        return readSize > 0 ? Region(m_buffer + m_readPos, readSize) : Region(0, 0);
    }

    return Region(0, 0);
}

void CoreAudioRingBuffer::releaseReadRegion(const CoreAudioRingBuffer::Region &region)
{
    m_readPos = (m_readPos + region.second) % m_bufferSize;

    m_bufferUsed.fetchAndAddRelease(-region.second);
}

CoreAudioRingBuffer::Region CoreAudioRingBuffer::acquireWriteRegion(int size)
{
    const int free = m_bufferSize - m_bufferUsed.fetchAndAddAcquire(0);

    Region output;

    if (free > 0) {
        const int writeSize = qMin(size, qMin(m_bufferSize - m_writePos, free));
        output =  writeSize > 0 ? Region(m_buffer + m_writePos, writeSize) : Region(0, 0);
    } else {
        output = Region(0, 0);
    }
#ifdef QT_DEBUG_COREAUDIO
    qDebug("acquireWriteRegion(%d) free: %d returning Region(%p, %d)", size, free, output.first, output.second);
#endif
    return output;
}
void CoreAudioRingBuffer::releaseWriteRegion(const CoreAudioRingBuffer::Region &region)
{
    m_writePos = (m_writePos + region.second) % m_bufferSize;

    m_bufferUsed.fetchAndAddRelease(region.second);
#ifdef QT_DEBUG_COREAUDIO
    qDebug("releaseWriteRegion(%p,%d): m_writePos:%d", region.first, region.second, m_writePos);
#endif
}

int CoreAudioRingBuffer::used() const
{
    return m_bufferUsed.load();
}

int CoreAudioRingBuffer::free() const
{
    return m_bufferSize - m_bufferUsed.load();
}

int CoreAudioRingBuffer::size() const
{
    return m_bufferSize;
}

void CoreAudioRingBuffer::reset()
{
    m_readPos = 0;
    m_writePos = 0;
    m_bufferUsed.store(0);
}

QT_END_NAMESPACE
