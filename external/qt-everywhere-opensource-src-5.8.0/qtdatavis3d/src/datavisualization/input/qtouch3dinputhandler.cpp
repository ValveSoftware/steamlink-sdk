/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Data Visualization module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL$
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
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qtouch3dinputhandler_p.h"
#include <QtCore/QTimer>
#include <QtCore/qmath.h>

QT_BEGIN_NAMESPACE_DATAVISUALIZATION

static const float maxTapAndHoldJitter = 20.0f;
static const int maxPinchJitter = 10;
#if defined (Q_OS_ANDROID) || defined(Q_OS_IOS)
static const int maxSelectionJitter = 10;
#else
static const int maxSelectionJitter = 5;
#endif
static const int tapAndHoldTime = 250;
static const float rotationSpeed = 200.0f;
static const float touchZoomDrift = 0.02f;

/*!
 * \class QTouch3DInputHandler
 * \inmodule QtDataVisualization
 * \brief Basic touch display based input handler.
 * \since QtDataVisualization 1.0
 *
 * QTouch3DInputHandler is the basic input handler for touch screen devices.
 *
 * Default touch input handler has the following functionalty:
 * \table
 *      \header
 *          \li Gesture
 *          \li Action
 *      \row
 *          \li Touch-And-Move
 *          \li Rotate graph within limits set for Q3DCamera
 *      \row
 *          \li Tap
 *          \li Select the item tapped or remove selection if none.
 *              May open the secondary view depending on the
 *              \l {QAbstract3DGraph::selectionMode}{selection mode}.
 *      \row
 *          \li Tap-And-Hold
 *          \li Same as tap.
 *      \row
 *          \li Pinch
 *          \li Zoom in/out within the allowable zoom range set for Q3DCamera.
 *      \row
 *          \li Tap on the primary view when the secondary view is visible
 *          \li Closes the secondary view.
 *          \note Secondary view is available only for Q3DBars and Q3DSurface graphs.
 * \endtable
 *
 * Rotation, zoom, and selection can each be individually disabled using
 * corresponding Q3DInputHandler properties.
 */

/*!
 * \qmltype TouchInputHandler3D
 * \inqmlmodule QtDataVisualization
 * \since QtDataVisualization 1.2
 * \ingroup datavisualization_qml
 * \instantiates QTouch3DInputHandler
 * \inherits InputHandler3D
 * \brief Basic touch display based input handler.
 *
 * TouchInputHandler3D is the basic input handler for touch screen devices.
 *
 * See QTouch3DInputHandler documentation for more details.
 */

/*!
 * Constructs the basic touch display input handler. An optional \a parent parameter can be given
 * and is then passed to QObject constructor.
 */
QTouch3DInputHandler::QTouch3DInputHandler(QObject *parent)
    : Q3DInputHandler(parent),
      d_ptr(new QTouch3DInputHandlerPrivate(this))
{
}

/*!
 *  Destroys the input handler.
 */
QTouch3DInputHandler::~QTouch3DInputHandler()
{
}

/*!
 * Override this to change handling of touch events.
 * Touch event is given in the \a event.
 */
void QTouch3DInputHandler::touchEvent(QTouchEvent *event)
{
    QList<QTouchEvent::TouchPoint> points;
    points = event->touchPoints();

    if (!scene()->isSlicingActive() && points.count() == 2) {
        d_ptr->m_holdTimer->stop();
        QPointF distance = points.at(0).pos() - points.at(1).pos();
        QPoint midPoint = ((points.at(0).pos() + points.at(1).pos()) / 2.0).toPoint();
        d_ptr->handlePinchZoom(distance.manhattanLength(), midPoint);
    } else if (points.count() == 1) {
        QPointF pointerPos = points.at(0).pos();
        if (event->type() == QEvent::TouchBegin) {
            // Flush input state
            d_ptr->m_inputState = QAbstract3DInputHandlerPrivate::InputStateNone;
            if (scene()->isSlicingActive()) {
                if (isSelectionEnabled()) {
                    if (scene()->isPointInPrimarySubView(pointerPos.toPoint()))
                        setInputView(InputViewOnPrimary);
                    else if (scene()->isPointInSecondarySubView(pointerPos.toPoint()))
                        setInputView(InputViewOnSecondary);
                    else
                        setInputView(InputViewNone);
                }
            } else {
                // Handle possible tap-and-hold selection
                if (isSelectionEnabled()) {
                    d_ptr->m_startHoldPos = pointerPos;
                    d_ptr->m_touchHoldPos = d_ptr->m_startHoldPos;
                    d_ptr->m_holdTimer->start();
                    setInputView(InputViewOnPrimary);
                }
                // Start rotating
                if (isRotationEnabled()) {
                    d_ptr->m_inputState = QAbstract3DInputHandlerPrivate::InputStateRotating;
                    setInputPosition(pointerPos.toPoint());
                    setInputView(InputViewOnPrimary);
                }
            }
        } else if (event->type() == QEvent::TouchEnd) {
            setInputView(InputViewNone);
            d_ptr->m_holdTimer->stop();
            // Handle possible selection
            if (!scene()->isSlicingActive()
                    && QAbstract3DInputHandlerPrivate::InputStatePinching
                    != d_ptr->m_inputState) {
                d_ptr->handleSelection(pointerPos);
            }
        } else if (event->type() == QEvent::TouchUpdate) {
            if (!scene()->isSlicingActive()) {
                d_ptr->m_touchHoldPos = pointerPos;
                // Handle rotation
                d_ptr->handleRotation(pointerPos);
            }
        }
    } else {
        d_ptr->m_holdTimer->stop();
    }
}

QTouch3DInputHandlerPrivate::QTouch3DInputHandlerPrivate(QTouch3DInputHandler *q)
    : Q3DInputHandlerPrivate(q),
      q_ptr(q),
      m_holdTimer(0),
      m_inputState(QAbstract3DInputHandlerPrivate::InputStateNone)
{
    m_holdTimer = new QTimer();
    m_holdTimer->setSingleShot(true);
    m_holdTimer->setInterval(tapAndHoldTime);
    connect(m_holdTimer, &QTimer::timeout, this, &QTouch3DInputHandlerPrivate::handleTapAndHold);
}

QTouch3DInputHandlerPrivate::~QTouch3DInputHandlerPrivate()
{
    m_holdTimer->stop();
    delete m_holdTimer;
}

void QTouch3DInputHandlerPrivate::handlePinchZoom(float distance, const QPoint &pos)
{
    if (q_ptr->isZoomEnabled()) {
        int newDistance = distance;
        int prevDist = q_ptr->prevDistance();
        if (prevDist > 0 && qAbs(prevDist - newDistance) < maxPinchJitter)
            return;
        m_inputState = QAbstract3DInputHandlerPrivate::InputStatePinching;
        Q3DCamera *camera = q_ptr->scene()->activeCamera();
        int zoomLevel = int(camera->zoomLevel());
        const int minZoomLevel = int(camera->minZoomLevel());
        const int maxZoomLevel = int(camera->maxZoomLevel());
        float zoomRate = qSqrt(qSqrt(zoomLevel));
        if (newDistance > prevDist)
            zoomLevel += zoomRate;
        else
            zoomLevel -= zoomRate;
        zoomLevel = qBound(minZoomLevel, zoomLevel, maxZoomLevel);

        if (q_ptr->isZoomAtTargetEnabled()) {
            q_ptr->scene()->setGraphPositionQuery(pos);
            m_zoomAtTargetPending = true;
            // If zoom at target is enabled, we don't want to zoom yet, as that causes
            // jitter. Instead, we zoom next frame, when we apply the camera position.
            m_requestedZoomLevel = zoomLevel;
            m_driftMultiplier = touchZoomDrift;
        } else {
            camera->setZoomLevel(zoomLevel);
        }

        q_ptr->setPrevDistance(newDistance);
    }
}

void QTouch3DInputHandlerPrivate::handleTapAndHold()
{
    if (q_ptr->isSelectionEnabled()) {
        QPointF distance = m_startHoldPos - m_touchHoldPos;
        if (distance.manhattanLength() < maxTapAndHoldJitter) {
            q_ptr->setInputPosition(m_touchHoldPos.toPoint());
            q_ptr->scene()->setSelectionQueryPosition(m_touchHoldPos.toPoint());
            m_inputState = QAbstract3DInputHandlerPrivate::InputStateSelecting;
        }
    }
}

void QTouch3DInputHandlerPrivate::handleSelection(const QPointF &position)
{
    if (q_ptr->isSelectionEnabled()) {
        QPointF distance = m_startHoldPos - position;
        if (distance.manhattanLength() < maxSelectionJitter) {
            m_inputState = QAbstract3DInputHandlerPrivate::InputStateSelecting;
            q_ptr->scene()->setSelectionQueryPosition(position.toPoint());
        } else {
            m_inputState = QAbstract3DInputHandlerPrivate::InputStateNone;
            q_ptr->setInputView(QAbstract3DInputHandler::InputViewNone);
        }
        q_ptr->setPreviousInputPos(position.toPoint());
    }
}

void QTouch3DInputHandlerPrivate::handleRotation(const QPointF &position)
{
    if (q_ptr->isRotationEnabled()
            && QAbstract3DInputHandlerPrivate::InputStateRotating == m_inputState) {
        Q3DScene *scene = q_ptr->scene();
        Q3DCamera *camera = scene->activeCamera();
        float xRotation = camera->xRotation();
        float yRotation = camera->yRotation();
        QPointF inputPos = q_ptr->inputPosition();
        float mouseMoveX = float(inputPos.x() - position.x())
                / (scene->viewport().width() / rotationSpeed);
        float mouseMoveY = float(inputPos.y() - position.y())
                / (scene->viewport().height() / rotationSpeed);
        xRotation -= mouseMoveX;
        yRotation -= mouseMoveY;
        camera->setXRotation(xRotation);
        camera->setYRotation(yRotation);

        q_ptr->setPreviousInputPos(inputPos.toPoint());
        q_ptr->setInputPosition(position.toPoint());
    }
}

QT_END_NAMESPACE_DATAVISUALIZATION
