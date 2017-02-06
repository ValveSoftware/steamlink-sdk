/****************************************************************************
**
** Copyright (C) 2016 Klaralvdalens Datakonsult AB (KDAB).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt3D module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qorbitcameracontroller_p.h"
#include "qorbitcameracontroller.h"
#include <QtGlobal>
#include <Qt3DRender/QCamera>
#include <Qt3DInput/QAxis>
#include <Qt3DInput/QAnalogAxisInput>
#include <Qt3DInput/QButtonAxisInput>
#include <Qt3DInput/QAction>
#include <Qt3DInput/QActionInput>
#include <Qt3DInput/QLogicalDevice>
#include <Qt3DInput/QKeyboardDevice>
#include <Qt3DInput/QMouseDevice>
#include <Qt3DInput/QMouseEvent>
#include <Qt3DLogic/QFrameAction>

QT_BEGIN_NAMESPACE

namespace Qt3DExtras {

/*!
    \class Qt3DExtras::QOrbitCameraController
    \brief The QOrbitCameraController class allows controlling the scene camera along orbital path.
    \inmodule Qt3DExtras
    \since 5.7
    \inherits Qt3DCore::QEntity

    The controls are:
    \table
    \header
        \li Input
        \li Action
    \row
        \li Left mouse button
        \li While the left mouse button is pressed, mouse movement along x-axis moves the camera
        left and right and movement along y-axis moves it up and down.
    \row
        \li Right mouse button
        \li While the right mouse button is pressed, mouse movement along x-axis pans the camera
        around the camera view center and movement along y-axis tilts it around the camera
        view center.
    \row
        \li Both left and right mouse button
        \li While both the left and the right mouse button are pressed, mouse movement along y-axis
        zooms the camera in and out without changing the view center.
    \row
        \li Arrow keys
        \li Move the camera vertically and horizontally relative to camera viewport.
    \row
        \li Page up and page down keys
        \li Move the camera forwards and backwards.
    \row
        \li Shift key
        \li Changes the behavior of the up and down arrow keys to zoom the camera in and out
        without changing the view center. The other movement keys are disabled.
    \row
        \li Alt key
        \li Changes the behovior of the arrow keys to pan and tilt the camera around the view
        center. Disables the page up and page down keys.
    \endtable
*/

QOrbitCameraControllerPrivate::QOrbitCameraControllerPrivate()
    : Qt3DCore::QEntityPrivate()
    , m_camera(nullptr)
    , m_leftMouseButtonAction(new Qt3DInput::QAction())
    , m_rightMouseButtonAction(new Qt3DInput::QAction())
    , m_altButtonAction(new Qt3DInput::QAction())
    , m_shiftButtonAction(new Qt3DInput::QAction())
    , m_rxAxis(new Qt3DInput::QAxis())
    , m_ryAxis(new Qt3DInput::QAxis())
    , m_txAxis(new Qt3DInput::QAxis())
    , m_tyAxis(new Qt3DInput::QAxis())
    , m_tzAxis(new Qt3DInput::QAxis())
    , m_leftMouseButtonInput(new Qt3DInput::QActionInput())
    , m_rightMouseButtonInput(new Qt3DInput::QActionInput())
    , m_altButtonInput(new Qt3DInput::QActionInput())
    , m_shiftButtonInput(new Qt3DInput::QActionInput())
    , m_mouseRxInput(new Qt3DInput::QAnalogAxisInput())
    , m_mouseRyInput(new Qt3DInput::QAnalogAxisInput())
    , m_keyboardTxPosInput(new Qt3DInput::QButtonAxisInput())
    , m_keyboardTyPosInput(new Qt3DInput::QButtonAxisInput())
    , m_keyboardTzPosInput(new Qt3DInput::QButtonAxisInput())
    , m_keyboardTxNegInput(new Qt3DInput::QButtonAxisInput())
    , m_keyboardTyNegInput(new Qt3DInput::QButtonAxisInput())
    , m_keyboardTzNegInput(new Qt3DInput::QButtonAxisInput())
    , m_keyboardDevice(new Qt3DInput::QKeyboardDevice())
    , m_mouseDevice(new Qt3DInput::QMouseDevice())
    , m_logicalDevice(new Qt3DInput::QLogicalDevice())
    , m_frameAction(new Qt3DLogic::QFrameAction())
    , m_linearSpeed(10.0f)
    , m_lookSpeed(180.0f)
    , m_zoomInLimit(2.0f)
    , m_cameraUp(QVector3D(0.0f, 1.0f, 0.0f))
{}

void QOrbitCameraControllerPrivate::init()
{
    //// Actions

    // Left Mouse Button Action
    m_leftMouseButtonInput->setButtons(QVector<int>() << Qt::LeftButton);
    m_leftMouseButtonInput->setSourceDevice(m_mouseDevice);
    m_leftMouseButtonAction->addInput(m_leftMouseButtonInput);

    // Right Mouse Button Action
    m_rightMouseButtonInput->setButtons(QVector<int>() << Qt::RightButton);
    m_rightMouseButtonInput->setSourceDevice(m_mouseDevice);
    m_rightMouseButtonAction->addInput(m_rightMouseButtonInput);

    // Alt Button Action
    m_altButtonInput->setButtons(QVector<int>() << Qt::Key_Alt);
    m_altButtonInput->setSourceDevice(m_keyboardDevice);
    m_altButtonAction->addInput(m_altButtonInput);

    // Shift Button Action
    m_shiftButtonInput->setButtons(QVector<int>() << Qt::Key_Shift);
    m_shiftButtonInput->setSourceDevice(m_keyboardDevice);
    m_shiftButtonAction->addInput(m_shiftButtonInput);

    //// Axes

    // Mouse X
    m_mouseRxInput->setAxis(Qt3DInput::QMouseDevice::X);
    m_mouseRxInput->setSourceDevice(m_mouseDevice);
    m_rxAxis->addInput(m_mouseRxInput);

    // Mouse Y
    m_mouseRyInput->setAxis(Qt3DInput::QMouseDevice::Y);
    m_mouseRyInput->setSourceDevice(m_mouseDevice);
    m_ryAxis->addInput(m_mouseRyInput);

    // Keyboard Pos Tx
    m_keyboardTxPosInput->setButtons(QVector<int>() << Qt::Key_Right);
    m_keyboardTxPosInput->setScale(1.0f);
    m_keyboardTxPosInput->setSourceDevice(m_keyboardDevice);
    m_txAxis->addInput(m_keyboardTxPosInput);

    // Keyboard Pos Tz
    m_keyboardTzPosInput->setButtons(QVector<int>() << Qt::Key_PageUp);
    m_keyboardTzPosInput->setScale(1.0f);
    m_keyboardTzPosInput->setSourceDevice(m_keyboardDevice);
    m_tzAxis->addInput(m_keyboardTzPosInput);

    // Keyboard Pos Ty
    m_keyboardTyPosInput->setButtons(QVector<int>() << Qt::Key_Up);
    m_keyboardTyPosInput->setScale(1.0f);
    m_keyboardTyPosInput->setSourceDevice(m_keyboardDevice);
    m_tyAxis->addInput(m_keyboardTyPosInput);

    // Keyboard Neg Tx
    m_keyboardTxNegInput->setButtons(QVector<int>() << Qt::Key_Left);
    m_keyboardTxNegInput->setScale(-1.0f);
    m_keyboardTxNegInput->setSourceDevice(m_keyboardDevice);
    m_txAxis->addInput(m_keyboardTxNegInput);

    // Keyboard Neg Tz
    m_keyboardTzNegInput->setButtons(QVector<int>() << Qt::Key_PageDown);
    m_keyboardTzNegInput->setScale(-1.0f);
    m_keyboardTzNegInput->setSourceDevice(m_keyboardDevice);
    m_tzAxis->addInput(m_keyboardTzNegInput);

    // Keyboard Neg Ty
    m_keyboardTyNegInput->setButtons(QVector<int>() << Qt::Key_Down);
    m_keyboardTyNegInput->setScale(-1.0f);
    m_keyboardTyNegInput->setSourceDevice(m_keyboardDevice);
    m_tyAxis->addInput(m_keyboardTyNegInput);

    //// Logical Device

    m_logicalDevice->addAction(m_leftMouseButtonAction);
    m_logicalDevice->addAction(m_rightMouseButtonAction);
    m_logicalDevice->addAction(m_altButtonAction);
    m_logicalDevice->addAction(m_shiftButtonAction);
    m_logicalDevice->addAxis(m_rxAxis);
    m_logicalDevice->addAxis(m_ryAxis);
    m_logicalDevice->addAxis(m_txAxis);
    m_logicalDevice->addAxis(m_tyAxis);
    m_logicalDevice->addAxis(m_tzAxis);

    Q_Q(QOrbitCameraController);
    //// FrameAction

    QObject::connect(m_frameAction, SIGNAL(triggered(float)),
                     q, SLOT(_q_onTriggered(float)));

    // Disable the logical device when the entity is disabled
    QObject::connect(q, &Qt3DCore::QEntity::enabledChanged,
                     m_logicalDevice, &Qt3DInput::QLogicalDevice::setEnabled);

    q->addComponent(m_frameAction);
    q->addComponent(m_logicalDevice);
}

float clampInputs(float input1, float input2)
{
    float axisValue = input1 + input2;
    return (axisValue < -1) ? -1 : (axisValue > 1) ? 1 : axisValue;
}

float zoomDistance(QVector3D firstPoint, QVector3D secondPoint)
{
    return (secondPoint - firstPoint).lengthSquared();
}

void QOrbitCameraControllerPrivate::_q_onTriggered(float dt)
{
    if (m_camera != nullptr) {
        // Mouse input
        if (m_leftMouseButtonAction->isActive()) {
            if (m_rightMouseButtonAction->isActive()) {
                if ( zoomDistance(m_camera->position(), m_camera->viewCenter()) > m_zoomInLimit * m_zoomInLimit) {
                    // Dolly up to limit
                    m_camera->translate(QVector3D(0, 0, m_ryAxis->value()), m_camera->DontTranslateViewCenter);
                } else {
                    m_camera->translate(QVector3D(0, 0, -0.5), m_camera->DontTranslateViewCenter);
                }
            } else {
                // Translate
                m_camera->translate(QVector3D(clampInputs(m_rxAxis->value(), m_txAxis->value()) * m_linearSpeed,
                                              clampInputs(m_ryAxis->value(), m_tyAxis->value()) * m_linearSpeed,
                                              0) * dt);
            }
            return;
        }
        else if (m_rightMouseButtonAction->isActive()) {
            // Orbit
            m_camera->panAboutViewCenter((m_rxAxis->value() * m_lookSpeed) * dt, m_cameraUp);
            m_camera->tiltAboutViewCenter((m_ryAxis->value() * m_lookSpeed) * dt);
        }
        // Keyboard Input
        if (m_altButtonAction->isActive()) {
            // Orbit
            m_camera->panAboutViewCenter((m_txAxis->value() * m_lookSpeed) * dt, m_cameraUp);
            m_camera->tiltAboutViewCenter((m_tyAxis->value() * m_lookSpeed) * dt);
        } else if (m_shiftButtonAction->isActive()) {
            if (zoomDistance(m_camera->position(), m_camera->viewCenter()) > m_zoomInLimit * m_zoomInLimit) {
                // Dolly
                m_camera->translate(QVector3D(0, 0, m_tyAxis->value()), m_camera->DontTranslateViewCenter);
            } else {
                m_camera->translate(QVector3D(0, 0, -0.5), m_camera->DontTranslateViewCenter);
            }
        } else {
            // Translate
            m_camera->translate(QVector3D(clampInputs(m_leftMouseButtonAction->isActive() ? m_rxAxis->value() : 0, m_txAxis->value()) * m_linearSpeed,
                                          clampInputs(m_leftMouseButtonAction->isActive() ? m_ryAxis->value() : 0, m_tyAxis->value()) * m_linearSpeed,
                                          m_tzAxis->value() * m_linearSpeed) * dt);
        }
    }
}

QOrbitCameraController::QOrbitCameraController(Qt3DCore::QNode *parent)
    : Qt3DCore::QEntity(*new QOrbitCameraControllerPrivate, parent)
{
    Q_D(QOrbitCameraController);
    d->init();
}

QOrbitCameraController::~QOrbitCameraController()
{
}

/*!
    \property QOrbitCameraController::camera

    Holds the currently controlled camera.
*/
Qt3DRender::QCamera *QOrbitCameraController::camera() const
{
    Q_D(const QOrbitCameraController);
    return d->m_camera;
}

/*!
    \property QOrbitCameraController::linearSpeed

    Holds the current linear speed of the camera controller. Linear speed determines the
    movement speed of the camera.
*/
float QOrbitCameraController::linearSpeed() const
{
    Q_D(const QOrbitCameraController);
    return d->m_linearSpeed;
}

/*!
    \property QOrbitCameraController::lookSpeed

    Holds the current look speed of the camera controller. The look speed determines the turn rate
    of the camera pan and tilt.
*/
float QOrbitCameraController::lookSpeed() const
{
    Q_D(const QOrbitCameraController);
    return d->m_lookSpeed;
}

/*!
    \property QOrbitCameraController::zoomInLimit

    Holds the current zoom-in limit. The zoom-in limit determines how close to the view center
    the camera can be zoomed.
*/
float QOrbitCameraController::zoomInLimit() const
{
    Q_D(const QOrbitCameraController);
    return d->m_zoomInLimit;
}

void QOrbitCameraController::setCamera(Qt3DRender::QCamera *camera)
{
    Q_D(QOrbitCameraController);
    if (d->m_camera != camera) {

        if (d->m_camera)
            d->unregisterDestructionHelper(d->m_camera);

        if (camera && !camera->parent())
            camera->setParent(this);

        d->m_camera = camera;

        // Ensures proper bookkeeping
        if (d->m_camera)
            d->registerDestructionHelper(d->m_camera, &QOrbitCameraController::setCamera, d->m_camera);

        emit cameraChanged();
    }
}

void QOrbitCameraController::setLinearSpeed(float linearSpeed)
{
    Q_D(QOrbitCameraController);
    if (d->m_linearSpeed != linearSpeed) {
        d->m_linearSpeed = linearSpeed;
        emit linearSpeedChanged();
    }
}

void QOrbitCameraController::setLookSpeed(float lookSpeed)
{
    Q_D(QOrbitCameraController);
    if (d->m_lookSpeed != lookSpeed) {
        d->m_lookSpeed = lookSpeed;
        emit lookSpeedChanged();
    }
}

void QOrbitCameraController::setZoomInLimit(float zoomInLimit)
{
    Q_D(QOrbitCameraController);
    if (d->m_zoomInLimit != zoomInLimit) {
        d->m_zoomInLimit = zoomInLimit;
        emit zoomInLimitChanged();
    }
}

} // Qt3DExtras

QT_END_NAMESPACE

#include "moc_qorbitcameracontroller.cpp"
