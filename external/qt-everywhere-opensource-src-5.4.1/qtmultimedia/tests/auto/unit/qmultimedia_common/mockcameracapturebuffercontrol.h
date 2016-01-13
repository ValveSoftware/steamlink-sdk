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

#ifndef MOCKCAMERACAPTUREBUFFERCONTROL_H
#define MOCKCAMERACAPTUREBUFFERCONTROL_H

#include "qcameracapturebufferformatcontrol.h"

class MockCaptureBufferFormatControl : public QCameraCaptureBufferFormatControl
{
    Q_OBJECT
public:
    MockCaptureBufferFormatControl(QObject *parent = 0):
            QCameraCaptureBufferFormatControl(parent),
            m_format(QVideoFrame::Format_Jpeg)
    {
    }

    QList<QVideoFrame::PixelFormat> supportedBufferFormats() const
    {
        return QList<QVideoFrame::PixelFormat>()
                << QVideoFrame::Format_Jpeg
                << QVideoFrame::Format_RGB32
                << QVideoFrame::Format_AdobeDng;
    }

    QVideoFrame::PixelFormat bufferFormat() const
    {
        return m_format;
    }

    void setBufferFormat(QVideoFrame::PixelFormat format)
    {
        if (format != m_format && supportedBufferFormats().contains(format)) {
            m_format = format;
            emit bufferFormatChanged(m_format);
        }
    }

private:
    QVideoFrame::PixelFormat m_format;
};


#endif // MOCKCAMERACAPTUREBUFFERCONTROL_H
