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

#include "camerahelper_p.h"

#include <QtCore/qmath.h>
#include <QtGui/QMatrix4x4>

QT_BEGIN_NAMESPACE_DATAVISUALIZATION

CameraHelper::CameraHelper(QObject *parent) :
    QObject(parent),
    m_position(0, 0.25, 3),
    m_target(0, 0, 0),
    m_up(0, 1, 0),
    m_previousMousePos(0,0),
    m_xRotation(0),
    m_yRotation(0),
    m_defaultXRotation(0),
    m_defaultYRotation(0),
    m_rotationSpeed(100)
{
}

CameraHelper::~CameraHelper()
{
}

void CameraHelper::setRotationSpeed(int speed)
{
    // increase for faster rotation
    m_rotationSpeed = speed;
}

void CameraHelper::setCameraRotation(const QPointF &rotation)
{
    m_xRotation = rotation.x();
    m_defaultXRotation = m_xRotation;
    m_yRotation = rotation.y();
    m_defaultYRotation = m_yRotation;
}

void CameraHelper::setDefaultCameraOrientation(const QVector3D &defaultPosition,
                                               const QVector3D &defaultTarget,
                                               const QVector3D &defaultUp)
{
    m_position = defaultPosition;
    m_target = defaultTarget;
    m_up = defaultUp;
}

QMatrix4x4 CameraHelper::calculateViewMatrix(const QPoint &mousePos, int zoom,
                                             int screenWidth, int screenHeight, bool showUnder)
{
    QMatrix4x4 viewMatrix;
    GLfloat lowerLimit = 0.0f;

    if (showUnder)
        lowerLimit = -90.0f;

    // Calculate mouse movement since last frame
    GLfloat mouseMoveX = GLfloat(m_previousMousePos.x() - mousePos.x())
            / (screenWidth / m_rotationSpeed);
    GLfloat mouseMoveY = GLfloat(m_previousMousePos.y() - mousePos.y())
            / (screenHeight / m_rotationSpeed);
    // Apply to rotations
    m_xRotation -= mouseMoveX;
    m_yRotation -= mouseMoveY;
    // Reset at 360 in x and limit to 0...90 in y
    if (qAbs(m_xRotation) >= 360.0f)
        m_xRotation = 0.0f;
    if (m_yRotation >= 90.0f)
        m_yRotation = 90.0f;
    else if (m_yRotation <= lowerLimit)
        m_yRotation = lowerLimit;

    // Apply to view matrix
    viewMatrix.lookAt(m_position, m_target, m_up);
    // Compensate for translation (if m_target is off origin)
    viewMatrix.translate(m_target.x(), m_target.y(), m_target.z());
    // Apply rotations
    // Handle x and z rotation when y -angle is other than 0
    viewMatrix.rotate(m_xRotation, 0, qCos(qDegreesToRadians(m_yRotation)),
                      qSin(qDegreesToRadians(m_yRotation)));
    // y rotation is always "clean"
    viewMatrix.rotate(m_yRotation, 1.0f, 0.0f, 0.0f);
    // handle zoom by scaling
    viewMatrix.scale((GLfloat)zoom / 100.0f);
    // Compensate for translation (if m_target is off origin)
    viewMatrix.translate(-m_target.x(), -m_target.y(), -m_target.z());

    m_previousMousePos = mousePos;
    return viewMatrix;
}

QVector3D CameraHelper::calculateLightPosition(const QVector3D &lightPosition,
                                               GLfloat fixedRotation, GLfloat distanceModifier)
{
    // Move light with camera
    QVector3D newLightPosition;
    GLfloat radiusFactor = lightPosition.z() * (1.5f + distanceModifier); // for making sure light is outside the scene at its lowest point
    GLfloat xAngle;
    GLfloat yAngle;
    if (!fixedRotation) {
        xAngle = qDegreesToRadians(m_xRotation);
        yAngle = qDegreesToRadians(m_yRotation);
    } else {
        xAngle = qDegreesToRadians(fixedRotation);
        yAngle = 0;
    }
    GLfloat radius = (radiusFactor + lightPosition.y()); // set radius to match the highest height of the light
    GLfloat zPos = radius * qCos(xAngle) * qCos(yAngle);
    GLfloat xPos = radius * qSin(xAngle) * qCos(yAngle);
    GLfloat yPos = (radiusFactor + lightPosition.y()) * qSin(yAngle);
    // Keep light in the set position in relation to camera
    newLightPosition = QVector3D(-xPos + lightPosition.x(),
                                 yPos + lightPosition.y(),
                                 zPos + lightPosition.z());
    return newLightPosition;
}

void CameraHelper::updateMousePos(const QPoint &mousePos)
{
    m_previousMousePos = mousePos;
    // if mouse position is set to (0, 0), reset rotations
    if (QPoint(0, 0) == mousePos) {
        m_xRotation = m_defaultXRotation;
        m_yRotation = m_defaultYRotation;
    }
}

QPointF CameraHelper::getCameraRotations()
{
    QPointF rotations(m_xRotation, m_yRotation);
    return rotations;
}

void CameraHelper::setCameraPreset(Q3DCamera::CameraPreset preset)
{
    switch (preset) {
    case Q3DCamera::CameraPresetFrontLow: {
        CameraHelper::setCameraRotation(QPointF(0.0f, 0.0f));
        break;
    }
    case Q3DCamera::CameraPresetFront: {
        CameraHelper::setCameraRotation(QPointF(0.0f, 22.5f));
        break;
    }
    case Q3DCamera::CameraPresetFrontHigh: {
        CameraHelper::setCameraRotation(QPointF(0.0f, 45.0f));
        break;
    }
    case Q3DCamera::CameraPresetLeftLow: {
        CameraHelper::setCameraRotation(QPointF(90.0f, 0.0f));
        break;
    }
    case Q3DCamera::CameraPresetLeft: {
        CameraHelper::setCameraRotation(QPointF(90.0f, 22.5f));
        break;
    }
    case Q3DCamera::CameraPresetLeftHigh: {
        CameraHelper::setCameraRotation(QPointF(90.0f, 45.0f));
        break;
    }
    case Q3DCamera::CameraPresetRightLow: {
        CameraHelper::setCameraRotation(QPointF(-90.0f, 0.0f));
        break;
    }
    case Q3DCamera::CameraPresetRight: {
        CameraHelper::setCameraRotation(QPointF(-90.0f, 22.5f));
        break;
    }
    case Q3DCamera::CameraPresetRightHigh: {
        CameraHelper::setCameraRotation(QPointF(-90.0f, 45.0f));
        break;
    }
    case Q3DCamera::CameraPresetBehindLow: {
        CameraHelper::setCameraRotation(QPointF(180.0f, 0.0f));
        break;
    }
    case Q3DCamera::CameraPresetBehind: {
        CameraHelper::setCameraRotation(QPointF(180.0f, 22.5f));
        break;
    }
    case Q3DCamera::CameraPresetBehindHigh: {
        CameraHelper::setCameraRotation(QPointF(180.0f, 45.0f));
        break;
    }
    case Q3DCamera::CameraPresetIsometricLeft: {
        CameraHelper::setCameraRotation(QPointF(45.0f, 22.5f));
        break;
    }
    case Q3DCamera::CameraPresetIsometricLeftHigh: {
        CameraHelper::setCameraRotation(QPointF(45.0f, 45.0f));
        break;
    }
    case Q3DCamera::CameraPresetIsometricRight: {
        CameraHelper::setCameraRotation(QPointF(-45.0f, 22.5f));
        break;
    }
    case Q3DCamera::CameraPresetIsometricRightHigh: {
        CameraHelper::setCameraRotation(QPointF(-45.0f, 45.0f));
        break;
    }
    case Q3DCamera::CameraPresetDirectlyAbove: {
        CameraHelper::setCameraRotation(QPointF(0.0f, 90.0f));
        break;
    }
    case Q3DCamera::CameraPresetDirectlyAboveCW45: {
        CameraHelper::setCameraRotation(QPointF(-45.0f, 90.0f));
        break;
    }
    case Q3DCamera::CameraPresetDirectlyAboveCCW45: {
        CameraHelper::setCameraRotation(QPointF(45.0f, 90.0f));
        break;
    }
    case Q3DCamera::CameraPresetFrontBelow: {
        CameraHelper::setCameraRotation(QPointF(0.0f, -45.0f));
        break;
    }
    case Q3DCamera::CameraPresetLeftBelow: {
        CameraHelper::setCameraRotation(QPointF(90.0f, -45.0f));
        break;
    }
    case Q3DCamera::CameraPresetRightBelow: {
        CameraHelper::setCameraRotation(QPointF(-90.0f, -45.0f));
        break;
    }
    case Q3DCamera::CameraPresetBehindBelow: {
        CameraHelper::setCameraRotation(QPointF(180.0f, -45.0f));
        break;
    }
    case Q3DCamera::CameraPresetDirectlyBelow: {
        CameraHelper::setCameraRotation(QPointF(0.0f, -90.0f));
        break;
    }
    default:
        break;
    }
}

QT_END_NAMESPACE_DATAVISUALIZATION
