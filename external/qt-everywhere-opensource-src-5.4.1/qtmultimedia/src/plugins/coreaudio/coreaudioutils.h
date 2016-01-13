/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the plugins of the Qt Toolkit.
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

#ifndef IOSAUDIOUTILS_H
#define IOSAUDIOUTILS_H

#include <CoreAudio/CoreAudioTypes.h>

#include <QtMultimedia/QAudioFormat>
#include <QtCore/qglobal.h>

QT_BEGIN_NAMESPACE

class CoreAudioUtils
{
public:
    static quint64 currentTime();
    static double frequency();
    static QAudioFormat toQAudioFormat(const AudioStreamBasicDescription& streamFormat);
    static AudioStreamBasicDescription toAudioStreamBasicDescription(QAudioFormat const& audioFormat);

private:
    static void initialize();
    static double sFrequency;
    static bool sIsInitialized;
};

class CoreAudioRingBuffer
{
public:
    typedef QPair<char*, int> Region;

    CoreAudioRingBuffer(int bufferSize);
    ~CoreAudioRingBuffer();

    Region acquireReadRegion(int size);
    void releaseReadRegion(Region const& region);
    Region acquireWriteRegion(int size);
    void releaseWriteRegion(Region const& region);

    int used() const;
    int free() const;
    int size() const;

    void reset();

private:
    int     m_bufferSize;
    int     m_readPos;
    int     m_writePos;
    char*   m_buffer;
    QAtomicInt  m_bufferUsed;
};

QT_END_NAMESPACE

#endif // IOSAUDIOUTILS_H
