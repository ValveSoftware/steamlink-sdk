/****************************************************************************
**
** Copyright (C) 2016 Klaralvdalens Datakonsult AB (KDAB).
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qfirstpersoncameracontroller_p.h"
#include "qfirstpersoncameracontroller.h"
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

QFirstPersonCameraControllerPrivate::QFirstPersonCameraControllerPrivate()
    : Qt3DCore::QEntityPrivate()
    , m_camera(nullptr)
    , m_leftMouseButtonAction(new Qt3DInput::QAction())
    , m_fineMotionAction(new Qt3DInput::QAction())
    , m_rxAxis(new Qt3DInput::QAxis())
    , m_ryAxis(new Qt3DInput::QAxis())
    , m_txAxis(new Qt3DInput::QAxis())
    , m_tyAxis(new Qt3DInput::QAxis())
    , m_tzAxis(new Qt3DInput::QAxis())
    , m_leftMouseButtonInput(new Qt3DInput::QActionInput())
    , m_fineMotionKeyInput(new Qt3DInput::QActionInput())
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
    , m_firstPersonUp(QVector3D(0.0f, 1.0f, 0.0f))
{}

void QFirstPersonCameraControllerPrivate::init()
{
    //// Actions

    // Left Mouse Button Action
    m_leftMouseButtonInput->setButtons(QVector<int>() << Qt::LeftButton);
    m_leftMouseButtonInput->setSourceDevice(m_mouseDevice);
    m_leftMouseButtonAction->addInput(m_leftMouseButtonInput);

    // Fine Motion Action
    m_fineMotionKeyInput->setButtons(QVector<int>() << Qt::Key_Shift);
    m_fineMotionKeyInput->setSourceDevice(m_keyboardDevice);
    m_fineMotionAction->addInput(m_fineMotionKeyInput);

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

    // Keyboard Pos Ty
    m_keyboardTyPosInput->setButtons(QVector<int>() << Qt::Key_PageUp);
    m_keyboardTyPosInput->setScale(1.0f);
    m_keyboardTyPosInput->setSourceDevice(m_keyboardDevice);
    m_tyAxis->addInput(m_keyboardTyPosInput);

    // Keyboard Pos Tz
    m_keyboardTzPosInput->setButtons(QVector<int>() << Qt::Key_Up);
    m_keyboardTzPosInput->setScale(1.0f);
    m_keyboardTzPosInput->setSourceDevice(m_keyboardDevice);
    m_tzAxis->addInput(m_keyboardTzPosInput);

    // Keyboard Neg Tx
    m_keyboardTxNegInput->setButtons(QVector<int>() << Qt::Key_Left);
    m_keyboardTxNegInput->setScale(-1.0f);
    m_keyboardTxNegInput->setSourceDevice(m_keyboardDevice);
    m_txAxis->addInput(m_keyboardTxNegInput);

    // Keyboard Neg Ty
    m_keyboardTyNegInput->setButtons(QVector<int>() << Qt::Key_PageDown);
    m_keyboardTyNegInput->setScale(-1.0f);
    m_keyboardTyNegInput->setSourceDevice(m_keyboardDevice);
    m_tyAxis->addInput(m_keyboardTyNegInput);

    // Keyboard Neg Tz
    m_keyboardTzNegInput->setButtons(QVector<int>() << Qt::Key_Down);
    m_keyboardTzNegInput->setScale(-1.0f);
    m_keyboardTzNegInput->setSourceDevice(m_keyboardDevice);
    m_tzAxis->addInput(m_keyboardTzNegInput);

    //// Logical Device

    m_logicalDevice->addAction(m_fineMotionAction);
    m_logicalDevice->addAction(m_leftMouseButtonAction);
    m_logicalDevice->addAxis(m_rxAxis);
    m_logicalDevice->addAxis(m_ryAxis);
    m_logicalDevice->addAxis(m_txAxis);
    m_logicalDevice->addAxis(m_tyAxis);
    m_logicalDevice->addAxis(m_tzAxis);

    Q_Q(QFirstPersonCameraController);
    //// FrameAction

    QObject::connect(m_frameAction, SIGNAL(triggered(float)),
                     q, SLOT(_q_onTriggered(float)));

    // Disable the logical device when the entity is disabled
    QObject::connect(q, &Qt3DCore::QEntity::enabledChanged,
                     m_logicalDevice, &Qt3DInput::QLogicalDevice::setEnabled);

    q->addComponent(m_frameAction);
    q->addComponent(m_logicalDevice);
}

void QFirstPersonCameraControllerPrivate::_q_onTriggered(float dt)
{
    if (m_camera != nullptr) {
        m_camera->translate(QVector3D(m_txAxis->value() * m_linearSpeed,
                                      m_tyAxis->value() * m_linearSpeed,
                                      m_tzAxis->value() * m_linearSpeed) * dt);
        if (m_leftMouseButtonAction->isActive()) {
            float lookSpeed = m_lookSpeed;
            if (m_fineMotionAction->isActive())
                lookSpeed *= 0.2f;
            m_camera->pan(m_rxAxis->value() * lookSpeed * dt, m_firstPersonUp);
            m_camera->tilt(m_ryAxis->value() * lookSpeed * dt);
        }
    }
}

/*!
    \class Qt3DExtras::QFirstPersonCameraController
    \brief The QFirstPersonCameraController class allows controlling the scene camera
    from the first person perspective.
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
        \li While the left mouse button is pressed, mouse movement along x-axis pans the camera and
        movement along y-axis tilts it.
    \row
        \li Shift key
        \li Turns the fine motion control active while pressed. Makes mouse pan and tilt less
        sensitive.
    \row
        \li Arrow keys
        \li Move the camera horizontally relative to camera viewport.
    \row
        \li Page up and page down keys
        \li Move the camera vertically relative to camera viewport.
    \endtable
*/

QFirstPersonCameraController::QFirstPersonCameraController(Qt3DCore::QNode *parent)
    : Qt3DCore::QEntity(*new QFirstPersonCameraControllerPrivate, parent)
{
    Q_D(QFirstPersonCameraController);
    d->init();
}

QFirstPersonCameraController::~QFirstPersonCameraController()
{
}

/*!
    \property QFirstPersonCameraController::camera

    Holds the currently controlled camera.
*/
Qt3DRender::QCamera *QFirstPersonCameraController::camera() const
{
    Q_D(const QFirstPersonCameraController);
    return d->m_camera;
}

/*!
    \property QFirstPersonCameraController::linearSpeed

    Holds the current linear speed of the camera controller. Linear speed determines the
    movement speed of the camera.
*/
float QFirstPersonCameraController::linearSpeed() const
{
    Q_D(const QFirstPersonCameraController);
    return d->m_linearSpeed;
}

/*!
    \property QFirstPersonCameraController::lookSpeed

    Holds the current look speed of the camera controller. The look speed determines the turn rate
    of the camera pan and tilt.
*/
float QFirstPersonCameraController::lookSpeed() const
{
    Q_D(const QFirstPersonCameraController);
    return d->m_lookSpeed;
}

void QFirstPersonCameraController::setCamera(Qt3DRender::QCamera *camera)
{
    Q_D(QFirstPersonCameraController);
    if (d->m_camera != camera) {

        if (d->m_camera)
            d->unregisterDestructionHelper(d->m_camera);

        if (camera && !camera->parent())
            camera->setParent(this);

        d->m_camera = camera;

        // Ensures proper bookkeeping
        if (d->m_camera)
            d->registerDestructionHelper(d->m_camera, &QFirstPersonCameraController::setCamera, d->m_camera);

        emit cameraChanged();
    }
}

void QFirstPersonCameraController::setLinearSpeed(float linearSpeed)
{
    Q_D(QFirstPersonCameraController);
    if (d->m_linearSpeed != linearSpeed) {
        d->m_linearSpeed = linearSpeed;
        emit linearSpeedChanged();
    }
}

void QFirstPersonCameraController::setLookSpeed(float lookSpeed)
{
    Q_D(QFirstPersonCameraController);
    if (d->m_lookSpeed != lookSpeed) {
        d->m_lookSpeed = lookSpeed;
        emit lookSpeedChanged();
    }
}

} // Qt3DExtras

QT_END_NAMESPACE

#include "moc_qfirstpersoncameracontroller.cpp"
