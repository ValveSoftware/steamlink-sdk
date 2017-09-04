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

#include "qdeclarativemultimediaglobal_p.h"

#include <qcamerainfo.h>
#include <qjsengine.h>

QT_BEGIN_NAMESPACE

/*!
\qmltype QtMultimedia
\inqmlmodule QtMultimedia
\ingroup multimedia_qml
\since QtMultimedia 5.4
\brief Provides a global object with useful functions from Qt Multimedia.

The \c QtMultimedia object is a global object with utility functions and properties.

It is not instantiable; to use it, call the members of the global \c QtMultimedia object directly.
For example:

\qml
Camera {
    deviceId: QtMultimedia.defaultCamera.deviceId
}
\endqml

*/

/*!
    \qmlproperty object QtMultimedia::QtMultimedia::defaultCamera
    \readonly

    The \c defaultCamera object provides information about the default camera on the system.

    Its properties are \c deviceId, \c displayName, \c position and \c orientation. See
    \l{QtMultimedia::QtMultimedia::availableCameras}{availableCameras} for a description of each
    of them.

    If there is no default camera, \c defaultCamera.deviceId will contain an empty string.

    \note This property is static; it is not updated if the system's default camera changes after the
    application started.
*/

/*!
    \qmlproperty list<object> QtMultimedia::QtMultimedia::availableCameras
    \readonly

    This property provides information about the cameras available on the system.

    Each object in the list has the following properties:

    \table
    \row
    \li \c deviceId
    \li
    This read-only property holds the unique identifier of the camera.

    You can choose which device to use with a \l Camera object by setting its
    \l{Camera::deviceId}{deviceId} property to this value.

    \row
    \li \c displayName
    \li
    This read-only property holds the human-readable name of the camera.
    You can use this property to display the name of the camera in a user interface.

    \row
    \li \c position
    \li
    This read-only property holds the physical position of the camera on the hardware system.
    Please see \l{Camera::position}{Camera.position} for more information.

    \row
    \li \c orientation
    \li
    This read-only property holds the physical orientation of the camera sensor.
    Please see \l{Camera::orientation}{Camera.orientation} for more information.

    \endtable

    \note This property is static; it is not updated when cameras are added or removed from
    the system, like USB cameras on a desktop platform.

    The following example shows how to display a list of available cameras. The user can change
    the active camera by selecting one of the items in the list.

    \qml
    Item {

        Camera {
            id: camera
        }

        VideoOutput {
            anchors.fill: parent
            source: camera
        }

        ListView {
            anchors.fill: parent

            model: QtMultimedia.availableCameras
            delegate: Text {
                text: modelData.displayName

                MouseArea {
                    anchors.fill: parent
                    onClicked: camera.deviceId = modelData.deviceId
                }
            }
        }
    }

    \endqml
*/

static QJSValue cameraInfoToJSValue(QJSEngine *jsEngine, const QCameraInfo &camera)
{
    QJSValue o = jsEngine->newObject();
    o.setProperty(QStringLiteral("deviceId"), camera.deviceName());
    o.setProperty(QStringLiteral("displayName"), camera.description());
    o.setProperty(QStringLiteral("position"), int(camera.position()));
    o.setProperty(QStringLiteral("orientation"), camera.orientation());
    return o;
}

QDeclarativeMultimediaGlobal::QDeclarativeMultimediaGlobal(QJSEngine *engine, QObject *parent)
    : QObject(parent)
    , m_engine(engine)
{
}

QJSValue QDeclarativeMultimediaGlobal::defaultCamera() const
{
    return cameraInfoToJSValue(m_engine, QCameraInfo::defaultCamera());
}

QJSValue QDeclarativeMultimediaGlobal::availableCameras() const
{
    QList<QCameraInfo> cameras = QCameraInfo::availableCameras();
    QJSValue availableCameras = m_engine->newArray(cameras.count());
    for (int i = 0; i < cameras.count(); ++i)
        availableCameras.setProperty(i, cameraInfoToJSValue(m_engine, cameras.at(i)));
    return availableCameras;
}

/*!
    \qmlmethod real QtMultimedia::QtMultimedia::convertVolume(real volume, VolumeScale from, VolumeScale to)

    Converts an audio \a volume \a from a volume scale \a to another, and returns the result.

    Depending on the context, different scales are used to represent audio volume. All Qt Multimedia
    classes that have an audio volume use a linear scale, the reason is that the loudness of a
    speaker is controlled by modulating its voltage on a linear scale. The human ear on the other
    hand, perceives loudness in a logarithmic way. Using a logarithmic scale for volume controls
    is therefore appropriate in most applications. The decibel scale is logarithmic by nature and
    is commonly used to define sound levels, it is usually used for UI volume controls in
    professional audio applications. The cubic scale is a computationally cheap approximation of a
    logarithmic scale, it provides more control over lower volume levels.

    Valid values for \a from and \a to are:
    \list
    \li QtMultimedia.LinearVolumeScale - Linear scale. \c 0.0 (0%) is silence and \c 1.0 (100%) is
        full volume. All Qt Multimedia types that have an audio volume use a linear scale.
    \li QtMultimedia.CubicVolumeScale - Cubic scale. \c 0.0 (0%) is silence and \c 1.0 (100%) is full
        volume.
    \li QtMultimedia.LogarithmicVolumeScale - Logarithmic scale. \c 0.0 (0%) is silence and \c 1.0
        (100%) is full volume. UI volume controls should usually use a logarithmic scale.
    \li QtMultimedia.DecibelVolumeScale - Decibel (dB, amplitude) logarithmic scale. \c -200 is
        silence and \c 0 is full volume.
    \endlist

    The following example shows how the volume value from a UI volume control can be converted so
    that the perceived increase in loudness is the same when increasing the volume control from 0.2
    to 0.3 as it is from 0.5 to 0.6:

    \code
    Slider {
        id: volumeSlider

        property real volume: QtMultimedia.convertVolume(volumeSlider.value,
                                                         QtMultimedia.LogarithmicVolumeScale,
                                                         QtMultimedia.LinearVolumeScale)
    }

    MediaPlayer {
        volume: volumeSlider.volume
    }
    \endcode

    \since 5.8
*/
qreal QDeclarativeMultimediaGlobal::convertVolume(qreal volume,
                                                  QDeclarativeMultimediaGlobal::VolumeScale from,
                                                  QDeclarativeMultimediaGlobal::VolumeScale to) const
{
    return QAudio::convertVolume(volume, QAudio::VolumeScale(from), QAudio::VolumeScale(to));
}

QT_END_NAMESPACE
