/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
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
