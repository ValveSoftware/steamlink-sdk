/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Mobility Components.
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

/* Camera snippets */

#include "qcamera.h"
#include "qcamerainfo.h"
#include "qcameraviewfinder.h"
#include "qcameraviewfindersettings.h"
#include "qmediarecorder.h"
#include "qcameraimagecapture.h"
#include "qcameraimageprocessing.h"
#include "qabstractvideosurface.h"
#include <QtGui/qscreen.h>
#include <QtGui/qguiapplication.h>
#include <QtGui/qimage.h>

/* Globals so that everything is consistent. */
QCamera *camera = 0;
QCameraViewfinder *viewfinder = 0;
QMediaRecorder *recorder = 0;
QCameraImageCapture *imageCapture = 0;

//! [Camera overview check]
bool checkCameraAvailability()
{
    if (QCameraInfo::availableCameras().count() > 0)
        return true;
    else
        return false;
}
//! [Camera overview check]

void overview_viewfinder()
{
    //! [Camera overview viewfinder]
    camera = new QCamera;
    viewfinder = new QCameraViewfinder;
    camera->setViewfinder(viewfinder);
    viewfinder->show();

    camera->start(); // to start the viewfinder
    //! [Camera overview viewfinder]
}

void overview_camera_by_position()
{
    //! [Camera overview position]
    camera = new QCamera(QCamera::FrontFace);
    //! [Camera overview position]
}

// -.-
class MyVideoSurface : public QAbstractVideoSurface
{
    QList<QVideoFrame::PixelFormat> supportedPixelFormats(QAbstractVideoBuffer::HandleType handleType) const
    {
        Q_UNUSED(handleType);
        return QList<QVideoFrame::PixelFormat>();
    }
    bool present(const QVideoFrame &frame)
    {
        Q_UNUSED(frame);
        return true;
    }
};

void overview_surface()
{
    MyVideoSurface *mySurface;
    //! [Camera overview surface]
    camera = new QCamera;
    mySurface = new MyVideoSurface;
    camera->setViewfinder(mySurface);

    camera->start();
    // MyVideoSurface::present(..) will be called with viewfinder frames
    //! [Camera overview surface]
}

void overview_viewfinder_orientation()
{
    QCamera camera;

    //! [Camera overview viewfinder orientation]
    // Assuming a QImage has been created from the QVideoFrame that needs to be presented
    QImage videoFrame;
    QCameraInfo cameraInfo(camera); // needed to get the camera sensor position and orientation

    // Get the current display orientation
    const QScreen *screen = QGuiApplication::primaryScreen();
    const int screenAngle = screen->angleBetween(screen->nativeOrientation(), screen->orientation());

    int rotation;
    if (cameraInfo.position() == QCamera::BackFace) {
        rotation = (cameraInfo.orientation() - screenAngle) % 360;
    } else {
        // Front position, compensate the mirror
        rotation = (360 - cameraInfo.orientation() + screenAngle) % 360;
    }

    // Rotate the frame so it always shows in the correct orientation
    videoFrame = videoFrame.transformed(QTransform().rotate(rotation));
    //! [Camera overview viewfinder orientation]
}

void overview_still()
{
    //! [Camera overview capture]
    imageCapture = new QCameraImageCapture(camera);

    camera->setCaptureMode(QCamera::CaptureStillImage);
    camera->start(); // Viewfinder frames start flowing

    //on half pressed shutter button
    camera->searchAndLock();

    //on shutter button pressed
    imageCapture->capture();

    //on shutter button released
    camera->unlock();
    //! [Camera overview capture]
}

void overview_movie()
{
    //! [Camera overview movie]
    camera = new QCamera;
    recorder = new QMediaRecorder(camera);

    camera->setCaptureMode(QCamera::CaptureVideo);
    camera->start();

    //on shutter button pressed
    recorder->record();

    // sometime later, or on another press
    recorder->stop();
    //! [Camera overview movie]
}

void camera_listing()
{
    //! [Camera listing]
    QList<QCameraInfo> cameras = QCameraInfo::availableCameras();
    foreach (const QCameraInfo &cameraInfo, cameras)
        qDebug() << cameraInfo.deviceName();
    //! [Camera listing]
}

void camera_selection()
{
    //! [Camera selection]
    QList<QCameraInfo> cameras = QCameraInfo::availableCameras();
    foreach (const QCameraInfo &cameraInfo, cameras) {
        if (cameraInfo.deviceName() == "mycamera")
            camera = new QCamera(cameraInfo);
    }
    //! [Camera selection]
}

void camera_info()
{
    //! [Camera info]
    QCamera myCamera;
    QCameraInfo cameraInfo(myCamera);

    if (cameraInfo.position() == QCamera::FrontFace)
        qDebug() << "The camera is on the front face of the hardware system.";
    else if (cameraInfo.position() == QCamera::BackFace)
        qDebug() << "The camera is on the back face of the hardware system.";

    qDebug() << "The camera sensor orientation is " << cameraInfo.orientation() << " degrees.";
    //! [Camera info]
}

void camera_blah()
{
    //! [Camera]
    camera = new QCamera;

    viewfinder = new QCameraViewfinder();
    viewfinder->show();

    camera->setViewfinder(viewfinder);

    imageCapture = new QCameraImageCapture(camera);

    camera->setCaptureMode(QCamera::CaptureStillImage);
    camera->start();
    //! [Camera]

    //! [Camera keys]
    //on half pressed shutter button
    camera->searchAndLock();

    //on shutter button pressed
    imageCapture->capture();

    //on shutter button released
    camera->unlock();
    //! [Camera keys]
}

void cameraimageprocessing()
{
    //! [Camera image whitebalance]
    camera = new QCamera;
    QCameraImageProcessing *imageProcessing = camera->imageProcessing();

    if (imageProcessing->isAvailable()) {
        imageProcessing->setWhiteBalanceMode(QCameraImageProcessing::WhiteBalanceFluorescent);
    }
    //! [Camera image whitebalance]

    //! [Camera image denoising]
    imageProcessing->setDenoisingLevel(-0.3); //reduce the amount of denoising applied
    //! [Camera image denoising]
}

void camerafocus()
{
    //! [Camera custom zoom]
    QCameraFocus *focus = camera->focus();
    focus->setFocusPointMode(QCameraFocus::FocusPointCustom);
    focus->setCustomFocusPoint(QPointF(0.25f, 0.75f)); // A point near the bottom left, 25% away from the corner, near that shiny vase
    //! [Camera custom zoom]

    //! [Camera combined zoom]
    focus->zoomTo(3.0, 4.0); // Super zoom!
    //! [Camera combined zoom]

    //! [Camera focus zones]
    focus->setFocusPointMode(QCameraFocus::FocusPointAuto);
    QList<QCameraFocusZone> zones = focus->focusZones();
    foreach (QCameraFocusZone zone, zones) {
        if (zone.status() == QCameraFocusZone::Focused) {
            // Draw a green box at zone.area()
        } else if (zone.status() == QCameraFocusZone::Selected) {
            // This area is selected for autofocusing, but is not in focus
            // Draw a yellow box at zone.area()
        }
    }
    //! [Camera focus zones]
}

void camera_viewfindersettings()
{
    //! [Camera viewfinder settings]
    QCameraViewfinderSettings viewfinderSettings;
    viewfinderSettings.setResolution(640, 480);
    viewfinderSettings.setMinimumFrameRate(15.0);
    viewfinderSettings.setMaximumFrameRate(30.0);

    camera->setViewfinderSettings(viewfinderSettings);
    //! [Camera viewfinder settings]
}
