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

#include "qcamerafocus.h"
#include "qmediaobject_p.h"

#include <qcamera.h>
#include <qcameracontrol.h>
#include <qcameraexposurecontrol.h>
#include <qcamerafocuscontrol.h>
#include <qcamerazoomcontrol.h>
#include <qmediarecordercontrol.h>
#include <qcameraimagecapturecontrol.h>
#include <qvideodeviceselectorcontrol.h>

#include <QtCore/QDebug>

QT_BEGIN_NAMESPACE

static void qRegisterCameraFocusMetaTypes()
{
    qRegisterMetaType<QCameraFocus::FocusModes>("QCameraFocus::FocusModes");
    qRegisterMetaType<QCameraFocus::FocusPointMode>("QCameraFocus::FocusPointMode");
}

Q_CONSTRUCTOR_FUNCTION(qRegisterCameraFocusMetaTypes)


class QCameraFocusFakeZoomControl : public QCameraZoomControl
{
public:
    QCameraFocusFakeZoomControl(QObject *parent) :
        QCameraZoomControl(parent) {}

    qreal maximumOpticalZoom() const { return 1.0; }
    qreal maximumDigitalZoom() const { return 1.0; }

    qreal requestedOpticalZoom() const { return 1.0; }
    qreal requestedDigitalZoom() const { return 1.0; }
    qreal currentOpticalZoom() const { return 1.0; }
    qreal currentDigitalZoom() const { return 1.0; }

    void zoomTo(qreal, qreal) { qWarning("The camera doesn't support zooming."); }
};

class QCameraFocusFakeFocusControl : public QCameraFocusControl
{
public:
    QCameraFocusFakeFocusControl(QObject *parent) :
        QCameraFocusControl(parent) {}

    QCameraFocus::FocusModes focusMode() const { return QCameraFocus::AutoFocus; }
    void setFocusMode(QCameraFocus::FocusModes) { qWarning("Focus mode selection is not supported"); }
    bool isFocusModeSupported(QCameraFocus::FocusModes) const { return false; }

    QCameraFocus::FocusPointMode focusPointMode() const { return QCameraFocus::FocusPointAuto; }
    void setFocusPointMode(QCameraFocus::FocusPointMode) { qWarning("Focus points mode selection is not supported"); }
    bool isFocusPointModeSupported(QCameraFocus::FocusPointMode) const { return false; }
    QPointF customFocusPoint() const { return QPointF(0.5,0.5); }
    void setCustomFocusPoint(const QPointF &) { qWarning("Focus points selection is not supported"); }

    QCameraFocusZoneList focusZones() const { return QCameraFocusZoneList(); }
};



class QCameraFocusZoneData : public QSharedData
{
public:
    QCameraFocusZoneData():
        status(QCameraFocusZone::Invalid)
    {

    }

    QCameraFocusZoneData(const QRectF &_area, QCameraFocusZone::FocusZoneStatus _status):
        area(_area),
        status(_status)
    {

    }


    QCameraFocusZoneData(const QCameraFocusZoneData &other):
        QSharedData(other),
        area(other.area),
        status(other.status)
    {
    }

    QCameraFocusZoneData& operator=(const QCameraFocusZoneData &other)
    {
        area = other.area;
        status = other.status;
        return *this;
    }

    QRectF area;
    QCameraFocusZone::FocusZoneStatus status;
};


/*!
    \class QCameraFocusZone

    \brief The QCameraFocusZone class provides information on zones used for autofocusing a camera.

    \inmodule QtMultimedia
    \ingroup multimedia
    \ingroup multimedia_camera

    For cameras that support autofocusing, in order for a camera to autofocus on
    part of a sensor frame, it considers different zones within the frame.  Which
    zones to use, and where the zones are located vary between different cameras.

    This class exposes what zones are used by a particular camera, and a list of the
    zones can be retrieved by a \l QCameraFocus instance.

    You can use this information to present visual feedback - for example, drawing
    rectangles around areas of the camera frame that are in focus, or changing the
    color of a zone as it comes into focus.

    \snippet multimedia-snippets/camera.cpp Camera focus zones

    \sa QCameraFocus
*/

/*!
    \enum QCameraFocusZone::FocusZoneStatus

    \value Invalid      This zone is not valid
    \value Unused       This zone may be used for autofocusing, but is not currently.
    \value Selected     This zone is currently being used for autofocusing, but is not in focus.
    \value Focused      This zone is being used for autofocusing and is currently in focus.
*/

/*!
 * \internal
 * Creates a new, empty QCameraFocusZone.
 */
QCameraFocusZone::QCameraFocusZone()
    :d(new QCameraFocusZoneData)
{

}

/*!
 * \internal
 * Creates a new QCameraFocusZone with the supplied \a area and \a status.
 */
QCameraFocusZone::QCameraFocusZone(const QRectF &area, QCameraFocusZone::FocusZoneStatus status)
    :d(new QCameraFocusZoneData(area, status))
{
}

/*!
 * Creates a new QCameraFocusZone as a copy of \a other.
 */
QCameraFocusZone::QCameraFocusZone(const QCameraFocusZone &other)
    :d(other.d)
{

}

/*!
 * Destroys this QCameraFocusZone.
 */
QCameraFocusZone::~QCameraFocusZone()
{

}

/*!
 * Assigns \a other to this QCameraFocusZone.
 */
QCameraFocusZone& QCameraFocusZone::operator=(const QCameraFocusZone &other)
{
    d = other.d;
    return *this;
}

/*!
 * Returns true if this focus zone is the same as \a other.
 */
bool QCameraFocusZone::operator==(const QCameraFocusZone &other) const
{
    return d == other.d ||
           (d->area == other.d->area && d->status == other.d->status);
}

/*!
 * Returns true if this focus zone is not the same as \a other.
 */
bool QCameraFocusZone::operator!=(const QCameraFocusZone &other) const
{
    return !(*this == other);
}

/*!
 * Returns true if this focus zone has a valid area and status.
 */
bool QCameraFocusZone::isValid() const
{
    return d->status != Invalid && !d->area.isValid();
}

/*!
 * Returns the area of the camera frame that this focus zone encompasses.
 *
 * Coordinates are in frame relative coordinates - \c QPointF(0,0) is the top
 * left of the frame, and \c QPointF(1,1) is the bottom right.
 */
QRectF QCameraFocusZone::area() const
{
    return d->area;
}

/*!
 * Returns the current status of this focus zone.
 */
QCameraFocusZone::FocusZoneStatus QCameraFocusZone::status() const
{
    return d->status;
}

/*!
 * \internal
 * Sets the current status of this focus zone to \a status.
 */
void QCameraFocusZone::setStatus(QCameraFocusZone::FocusZoneStatus status)
{
    d->status = status;
}


/*!
    \class QCameraFocus

    \brief The QCameraFocus class provides an interface for focus and zoom related camera settings.

    \inmodule QtMultimedia
    \ingroup multimedia
    \ingroup multimedia_camera

    On hardware that supports it, this class lets you adjust the focus
    or zoom (both optical and digital).  This also includes things
    like "Macro" mode for close up work (e.g. reading barcodes, or
    recognising letters), or "touch to focus" - indicating an
    interesting area of the viewfinder for the hardware to attempt
    to focus on.

    \snippet multimedia-snippets/camera.cpp Camera custom zoom

    Zooming can be accomplished in a number of ways - usually the more
    expensive but higher quality approach is an optical zoom, which allows
    using the full extent of the camera sensor to gather image pixels. In
    addition it is possible to digitally zoom, which will generally just
    enlarge part of the sensor frame and throw away other parts.  If the
    camera hardware supports optical zoom this should generally always
    be used first.  The \l maximumOpticalZoom() method allows this to be
    checked.  The \l zoomTo() method allows changing both optical and
    digital zoom at once.

    \snippet multimedia-snippets/camera.cpp Camera combined zoom

    \section2 Some notes on autofocus
    Some hardware supports a movable focus lens assembly, and typically
    this hardware also supports automatically focusing via some heuristic.
    You can influence this via the \l FocusPointMode setting - typically
    the center of the frame is brought into focus, but some hardware
    also supports focusing on any faces detected in the frame, or on
    a specific point (usually provided by a user in a "touch to focus"
    scenario).

    This class (in combination with \l QCameraFocusZone)
    can expose information on what parts of the camera sensor image
    are in focus or are being used for autofocusing via the \l focusZones()
    property:

    \snippet multimedia-snippets/camera.cpp Camera focus zones

    \sa QCameraFocusZone
*/


class QCameraFocusPrivate : public QMediaObjectPrivate
{
    Q_DECLARE_NON_CONST_PUBLIC(QCameraFocus)
public:
    void initControls();

    QCameraFocus *q_ptr;

    QCamera *camera;

    QCameraFocusControl *focusControl;
    QCameraZoomControl *zoomControl;
    bool available;
};


void QCameraFocusPrivate::initControls()
{
    Q_Q(QCameraFocus);

    focusControl = 0;
    zoomControl = 0;

    QMediaService *service = camera->service();
    if (service) {
        focusControl = qobject_cast<QCameraFocusControl *>(service->requestControl(QCameraFocusControl_iid));
        zoomControl = qobject_cast<QCameraZoomControl *>(service->requestControl(QCameraZoomControl_iid));
    }

    available = focusControl != 0;

    if (!focusControl)
        focusControl = new QCameraFocusFakeFocusControl(q);

    if (!zoomControl)
        zoomControl = new QCameraFocusFakeZoomControl(q);

    q->connect(focusControl, SIGNAL(focusZonesChanged()), q, SIGNAL(focusZonesChanged()));

    q->connect(zoomControl, SIGNAL(currentOpticalZoomChanged(qreal)),
               q, SIGNAL(opticalZoomChanged(qreal)));
    q->connect(zoomControl, SIGNAL(currentDigitalZoomChanged(qreal)),
               q, SIGNAL(digitalZoomChanged(qreal)));
    q->connect(zoomControl, SIGNAL(maximumOpticalZoomChanged(qreal)),
               q, SIGNAL(maximumOpticalZoomChanged(qreal)));
    q->connect(zoomControl, SIGNAL(maximumDigitalZoomChanged(qreal)),
               q, SIGNAL(maximumDigitalZoomChanged(qreal)));
}

/*!
    \internal
    Construct a QCameraFocus for \a camera.
*/

QCameraFocus::QCameraFocus(QCamera *camera):
    QObject(camera), d_ptr(new QCameraFocusPrivate)
{
    Q_D(QCameraFocus);
    d->camera = camera;
    d->q_ptr = this;
    d->initControls();
}


/*!
    Destroys the camera focus object.
*/

QCameraFocus::~QCameraFocus()
{
    delete d_ptr;
}

/*!
    Returns true if focus related settings are supported by this camera.

    You may need to also check if any specific features are supported.
*/
bool QCameraFocus::isAvailable() const
{
    return d_func()->available;
}

/*!
  \property QCameraFocus::focusMode
  \brief the current camera focus mode.


  This controls the way the camera lens assembly is configured.

  \sa QCameraFocus::isFocusModeSupported()
*/

QCameraFocus::FocusModes QCameraFocus::focusMode() const
{
    return d_func()->focusControl->focusMode();
}

void QCameraFocus::setFocusMode(QCameraFocus::FocusModes mode)
{
    d_func()->focusControl->setFocusMode(mode);
}

/*!
    Returns true if the focus \a mode is supported by camera.
*/

bool QCameraFocus::isFocusModeSupported(FocusModes mode) const
{
    return d_func()->focusControl->isFocusModeSupported(mode);
}

/*!
  \property QCameraFocus::focusPointMode
  \brief the current camera focus point selection mode.

  If the camera focus mode is set to use an autofocusing mode,
  this property controls the way the camera will select areas
  of the frame to use for autofocusing.

  \sa QCameraFocus::isFocusPointModeSupported()
*/

QCameraFocus::FocusPointMode QCameraFocus::focusPointMode() const
{
    return d_func()->focusControl->focusPointMode();
}

void QCameraFocus::setFocusPointMode(QCameraFocus::FocusPointMode mode)
{
    d_func()->focusControl->setFocusPointMode(mode);
}

/*!
  Returns true if focus point \a mode is supported.
 */
bool QCameraFocus::isFocusPointModeSupported(QCameraFocus::FocusPointMode mode) const
{
    return d_func()->focusControl->isFocusPointModeSupported(mode);
}

/*!
  \property QCameraFocus::customFocusPoint

  This property represents the position of the custom focus point, in relative frame coordinates:
  QPointF(0,0) points to the left top frame point, QPointF(0.5,0.5) points to the frame center.

  The custom focus point property is used only in \c FocusPointCustom focus mode.
 */

QPointF QCameraFocus::customFocusPoint() const
{
    return d_func()->focusControl->customFocusPoint();
}

void QCameraFocus::setCustomFocusPoint(const QPointF &point)
{
    d_func()->focusControl->setCustomFocusPoint(point);
}

/*!
  \property QCameraFocus::focusZones

  Returns the list of active focus zones.

  If QCamera::FocusPointAuto or QCamera::FocusPointFaceDetection focus mode is selected
  this method returns the list of zones the camera is actually focused on.

  The coordinates system is the same as for custom focus points:
  QPointF(0,0) points to the left top frame point, QPointF(0.5,0.5) points to the frame center.
 */
QCameraFocusZoneList QCameraFocus::focusZones() const
{
    return d_func()->focusControl->focusZones();
}

/*!
    Returns the maximum optical zoom.

    This will be \c 1.0 on cameras that do not support optical zoom.
*/

qreal QCameraFocus::maximumOpticalZoom() const
{
    return d_func()->zoomControl->maximumOpticalZoom();
}

/*!
    Returns the maximum digital zoom

    This will be \c 1.0 on cameras that do not support digital zoom.
*/

qreal QCameraFocus::maximumDigitalZoom() const
{
    return d_func()->zoomControl->maximumDigitalZoom();
}

/*!
  \property QCameraFocus::opticalZoom
  \brief the current optical zoom value.

  \sa QCameraFocus::digitalZoom
*/

qreal QCameraFocus::opticalZoom() const
{
    return d_func()->zoomControl->currentOpticalZoom();
}

/*!
  \property QCameraFocus::digitalZoom
  \brief the current digital zoom value.

  \sa QCameraFocus::opticalZoom
*/
qreal QCameraFocus::digitalZoom() const
{
    return d_func()->zoomControl->currentDigitalZoom();
}


/*!
    Set the camera \a optical and \a digital zoom values.

    Since there may be a physical component to move, the change in
    zoom value may not be instantaneous.

*/
void QCameraFocus::zoomTo(qreal optical, qreal digital)
{
    d_func()->zoomControl->zoomTo(optical, digital);
}

/*!
    \enum QCameraFocus::FocusMode

    \value ManualFocus          Manual or fixed focus mode.
    \value HyperfocalFocus      Focus to hyperfocal distance, with the maximum depth of field achieved.
                                All objects at distances from half of this
                                distance out to infinity will be acceptably sharp.
    \value InfinityFocus        Focus strictly to infinity.
    \value AutoFocus            One-shot auto focus mode.
    \value ContinuousFocus      Continuous auto focus mode.
    \value MacroFocus           One shot auto focus to objects close to camera.
*/

/*!
    \enum QCameraFocus::FocusPointMode

    \value FocusPointAuto       Automatically select one or multiple focus points.
    \value FocusPointCenter     Focus to the frame center.
    \value FocusPointFaceDetection Focus on faces in the frame.
    \value FocusPointCustom     Focus to the custom point, defined by QCameraFocus::customFocusPoint property.
*/

/*!
    \fn void QCameraFocus::opticalZoomChanged(qreal value)

    Signal emitted when optical zoom value changes to new \a value.
*/

/*!
    \fn void QCameraFocus::digitalZoomChanged(qreal value)

    Signal emitted when digital zoom value changes to new \a value.
*/

/*!
    \fn void QCameraFocus::maximumOpticalZoomChanged(qreal zoom)

    Signal emitted when the maximum supported optical \a zoom value changed.
*/

/*!
    \fn void QCameraFocus::maximumDigitalZoomChanged(qreal zoom)

    Signal emitted when the maximum supported digital \a zoom value changed.

    The maximum supported zoom value can depend on other camera settings,
    like capture mode or resolution.
*/



/*!
  \fn QCameraFocus::focusZonesChanged()

  This signal is emitted when the set of zones used in autofocusing is changed.

  This can change when a zone is focused or loses focus, or new focus zones
  have been detected.
*/


#include "moc_qcamerafocus.cpp"
QT_END_NAMESPACE
