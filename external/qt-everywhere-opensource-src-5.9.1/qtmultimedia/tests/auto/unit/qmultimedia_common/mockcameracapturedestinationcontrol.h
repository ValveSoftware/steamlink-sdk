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

#ifndef MOCKCAMERACAPTUREDESTINATIONCONTROL_H
#define MOCKCAMERACAPTUREDESTINATIONCONTROL_H

#include <QtMultimedia/qcameracapturedestinationcontrol.h>

class MockCaptureDestinationControl : public QCameraCaptureDestinationControl
{
    Q_OBJECT
public:
    MockCaptureDestinationControl(QObject *parent = 0):
            QCameraCaptureDestinationControl(parent),
            m_destination(QCameraImageCapture::CaptureToFile)
    {
    }

    bool isCaptureDestinationSupported(QCameraImageCapture::CaptureDestinations destination) const
    {
        return destination == QCameraImageCapture::CaptureToBuffer ||
               destination == QCameraImageCapture::CaptureToFile;
    }

    QCameraImageCapture::CaptureDestinations captureDestination() const
    {
        return m_destination;
    }

    void setCaptureDestination(QCameraImageCapture::CaptureDestinations destination)
    {
        if (isCaptureDestinationSupported(destination) && destination != m_destination) {
            m_destination = destination;
            emit captureDestinationChanged(m_destination);
        }
    }

private:
    QCameraImageCapture::CaptureDestinations m_destination;
};

#endif // MOCKCAMERACAPTUREDESTINATIONCONTROL_H
