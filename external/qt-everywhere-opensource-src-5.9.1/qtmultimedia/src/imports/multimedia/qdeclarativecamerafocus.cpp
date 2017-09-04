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

#include "qdeclarativecamera_p.h"
#include "qdeclarativecamerafocus_p.h"

QT_BEGIN_NAMESPACE

/*!
    \qmltype CameraFocus
    \instantiates QDeclarativeCameraFocus
    \inqmlmodule QtMultimedia
    \brief An interface for focus related camera settings.
    \ingroup multimedia_qml
    \ingroup camera_qml

    This type allows control over manual and automatic
    focus settings, including information about any parts of the
    camera frame that are selected for autofocusing.

    It should not be constructed separately, instead the
    \c focus property of a \l Camera should be used.

    \qml

    Item {
        width: 640
        height: 360

        Camera {
            id: camera

            focus {
                focusMode: Camera.FocusMacro
                focusPointMode: Camera.FocusPointCustom
                customFocusPoint: Qt.point(0.2, 0.2) // Focus relative to top-left corner
            }
        }

        VideoOutput {
            source: camera
            anchors.fill: parent
        }
    }

    \endqml
*/

/*!
    \class QDeclarativeCameraFocus
    \internal
    \brief An interface for focus related camera settings.
*/

/*!
    Construct a declarative camera focus object using \a parent object.
 */

QDeclarativeCameraFocus::QDeclarativeCameraFocus(QCamera *camera, QObject *parent) :
    QObject(parent)
{
    m_focus = camera->focus();
    m_focusZones = new FocusZonesModel(this);

    updateFocusZones();

    connect(m_focus, SIGNAL(focusZonesChanged()), SLOT(updateFocusZones()));
}

QDeclarativeCameraFocus::~QDeclarativeCameraFocus()
{
}
/*!
    \property QDeclarativeCameraFocus::focusMode

    This property holds the current camera focus mode.

    It's possible to combine multiple QCameraFocus::FocusMode enum values,
    for example QCameraFocus::MacroFocus + QCameraFocus::ContinuousFocus.

    In automatic focusing modes, the \l focusPointMode
    and \l focusZones properties provide information and control
    over how automatic focusing is performed.
*/

/*!
    \qmlproperty enumeration CameraFocus::focusMode

    This property holds the current camera focus mode, which can be one of the following values:

    \table
     \header
      \li Value
      \li Description
     \row
      \li FocusManual
      \li Manual or fixed focus mode.
     \row
      \li FocusHyperfocal
      \li Focus to hyperfocal distance, with the maximum depth of field achieved. All objects at distances from half of this distance out to infinity will be acceptably sharp.
     \row
      \li FocusInfinity
      \li Focus strictly to infinity.
     \row
      \li FocusAuto
      \li One-shot auto focus mode.
     \row
      \li FocusContinuous
      \li Continuous auto focus mode.
     \row
      \li FocusMacro
      \li One shot auto focus to objects close to camera.
    \endtable

    It's possible to combine multiple Camera::FocusMode values,
    for example Camera.FocusMacro + Camera.FocusContinuous.

    In automatic focusing modes, the \l focusPointMode property
    and \l focusZones property provide information and control
    over how automatic focusing is performed.
*/
QDeclarativeCameraFocus::FocusMode QDeclarativeCameraFocus::focusMode() const
{
    return QDeclarativeCameraFocus::FocusMode(int(m_focus->focusMode()));
}

/*!
    \qmlmethod bool QtMultimedia::CameraFocus::isFocusModeSupported(mode) const

    Returns true if the supplied \a mode is a supported focus mode, and
    false otherwise.
*/
bool QDeclarativeCameraFocus::isFocusModeSupported(QDeclarativeCameraFocus::FocusMode mode) const
{
    return m_focus->isFocusModeSupported(QCameraFocus::FocusModes(int(mode)));
}

void QDeclarativeCameraFocus::setFocusMode(QDeclarativeCameraFocus::FocusMode mode)
{
    if (mode != focusMode()) {
        m_focus->setFocusMode(QCameraFocus::FocusModes(int(mode)));
        emit focusModeChanged(focusMode());
    }
}
/*!
    \property QDeclarativeCameraFocus::focusPointMode

    This property holds the current camera focus point mode. It is used in
    automatic focusing modes to determine what to focus on.

    If the current focus point mode is \l QCameraFocus::FocusPointCustom, the
    \l customFocusPoint property allows you to specify which part of
    the frame to focus on.
*/
/*!
    \qmlproperty enumeration CameraFocus::focusPointMode

    This property holds the current camera focus point mode. It is used in automatic
    focusing modes to determine what to focus on. If the current
    focus point mode is \c Camera.FocusPointCustom, the \l customFocusPoint
    property allows you to specify which part of the frame to focus on.

    The property can take one of the following values:
    \table
     \header
      \li Value
      \li Description
     \row
      \li FocusPointAuto
      \li Automatically select one or multiple focus points.
     \row
      \li FocusPointCenter
      \li Focus to the frame center.
     \row
      \li FocusPointFaceDetection
      \li Focus on faces in the frame.
     \row
      \li FocusPointCustom
      \li Focus to the custom point, defined by the customFocusPoint property.
    \endtable
*/
QDeclarativeCameraFocus::FocusPointMode QDeclarativeCameraFocus::focusPointMode() const
{
    return QDeclarativeCameraFocus::FocusPointMode(m_focus->focusPointMode());
}

void QDeclarativeCameraFocus::setFocusPointMode(QDeclarativeCameraFocus::FocusPointMode mode)
{
    if (mode != focusPointMode()) {
        m_focus->setFocusPointMode(QCameraFocus::FocusPointMode(mode));
        emit focusPointModeChanged(focusPointMode());
    }
}

/*!
    \qmlmethod bool QtMultimedia::CameraFocus::isFocusPointModeSupported(mode) const

    Returns true if the supplied \a mode is a supported focus point mode, and
    false otherwise.
*/
bool QDeclarativeCameraFocus::isFocusPointModeSupported(QDeclarativeCameraFocus::FocusPointMode mode) const
{
    return m_focus->isFocusPointModeSupported(QCameraFocus::FocusPointMode(mode));
}
/*!
  \property QDeclarativeCameraFocus::customFocusPoint

  This property holds the position of the custom focus point in relative
  frame coordinates. For example, QPointF(0,0) pointing to the left top corner of the frame, and QPointF(0.5,0.5)
  pointing to the center of the frame.

  Custom focus point is used only in QCameraFocus::FocusPointCustom focus mode.
*/

/*!
  \qmlproperty point QtMultimedia::CameraFocus::customFocusPoint

  This property holds the position of custom focus point, in relative frame coordinates:
  QPointF(0,0) points to the left top frame point, QPointF(0.5,0.5)
  points to the frame center.

  Custom focus point is used only in FocusPointCustom focus mode.
*/

QPointF QDeclarativeCameraFocus::customFocusPoint() const
{
    return m_focus->customFocusPoint();
}

void QDeclarativeCameraFocus::setCustomFocusPoint(const QPointF &point)
{
    if (point != customFocusPoint()) {
        m_focus->setCustomFocusPoint(point);
        emit customFocusPointChanged(customFocusPoint());
    }
}
/*!
  \property QDeclarativeCameraFocus::focusZones

  This property holds the list of current camera focus zones,
  each including \c area specified in the same coordinates as \l customFocusPoint, and zone \c status as one of the following values:
    \table
    \header \li Value \li Description
    \row \li QCameraFocusZone::Unused  \li This focus point area is currently unused in autofocusing.
    \row \li QCameraFocusZone::Selected    \li This focus point area is used in autofocusing, but is not in focus.
    \row \li QCameraFocusZone::Focused  \li This focus point is used in autofocusing, and is in focus.
    \endtable
*/
/*!
  \qmlproperty list<focusZone> QtMultimedia::CameraFocus::focusZones

  This property holds the list of current camera focus zones,
  each including \c area specified in the same coordinates as \l customFocusPoint,
  and zone \c status as one of the following values:

    \table
    \header \li Value \li Description
    \row \li Camera.FocusAreaUnused  \li This focus point area is currently unused in autofocusing.
    \row \li Camera.FocusAreaSelected    \li This focus point area is used in autofocusing, but is not in focus.
    \row \li Camera.FocusAreaFocused  \li This focus point is used in autofocusing, and is in focus.
    \endtable

  \qml

  VideoOutput {
      id: viewfinder
      source: camera

      //display focus areas on camera viewfinder:
      Repeater {
            model: camera.focus.focusZones

            Rectangle {
                border {
                    width: 2
                    color: status == Camera.FocusAreaFocused ? "green" : "white"
                }
                color: "transparent"

                // Map from the relative, normalized frame coordinates
                property variant mappedRect: viewfinder.mapNormalizedRectToItem(area);

                x: mappedRect.x
                y: mappedRect.y
                width: mappedRect.width
                height: mappedRect.height
            }
      }
  }
  \endqml
*/

QAbstractListModel *QDeclarativeCameraFocus::focusZones() const
{
    return m_focusZones;
}

/*! \internal */
void QDeclarativeCameraFocus::updateFocusZones()
{
    m_focusZones->setFocusZones(m_focus->focusZones());
}


FocusZonesModel::FocusZonesModel(QObject *parent)
    :QAbstractListModel(parent)
{
    QHash<int, QByteArray> roles;
    roles[StatusRole] = "status";
    roles[AreaRole] = "area";
    setRoleNames(roles);
}

int FocusZonesModel::rowCount(const QModelIndex &parent) const
{
    if (parent == QModelIndex())
        return m_focusZones.count();

    return 0;
}

QVariant FocusZonesModel::data(const QModelIndex &index, int role) const
{
    if (index.row() < 0 || index.row() > m_focusZones.count())
        return QVariant();

    QCameraFocusZone zone = m_focusZones.value(index.row());

    if (role == StatusRole)
        return zone.status();

    if (role == AreaRole)
        return zone.area();

    return QVariant();
}

void FocusZonesModel::setFocusZones(const QCameraFocusZoneList &zones)
{
    beginResetModel();
    m_focusZones = zones;
    endResetModel();
}

QT_END_NAMESPACE

#include "moc_qdeclarativecamerafocus_p.cpp"
