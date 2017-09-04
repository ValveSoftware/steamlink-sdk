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

#include "axesinputhandler.h"
#include <QtCore/qmath.h>

AxesInputHandler::AxesInputHandler(QAbstract3DGraph *graph, QObject *parent) :
    Q3DInputHandler(parent),
    m_mousePressed(false),
    m_state(StateNormal),
    m_axisX(0),
    m_axisZ(0),
    m_axisY(0),
    m_speedModifier(15.0f)
{
    //! [3]
    // Connect to the item selection signal from graph
    connect(graph, &QAbstract3DGraph::selectedElementChanged, this,
            &AxesInputHandler::handleElementSelected);
    //! [3]
}

//! [0]
void AxesInputHandler::mousePressEvent(QMouseEvent *event, const QPoint &mousePos)
{
    Q3DInputHandler::mousePressEvent(event, mousePos);
    if (Qt::LeftButton == event->button())
        m_mousePressed = true;
}
//! [0]

//! [2]
void AxesInputHandler::mouseMoveEvent(QMouseEvent *event, const QPoint &mousePos)
{
    //! [5]
    // Check if we're trying to drag axis label
    if (m_mousePressed && m_state != StateNormal) {
        //! [5]
        setPreviousInputPos(inputPosition());
        setInputPosition(mousePos);
        handleAxisDragging();
    } else {
        Q3DInputHandler::mouseMoveEvent(event, mousePos);
    }
}
//! [2]

//! [1]
void AxesInputHandler::mouseReleaseEvent(QMouseEvent *event, const QPoint &mousePos)
{
    Q3DInputHandler::mouseReleaseEvent(event, mousePos);
    m_mousePressed = false;
    m_state = StateNormal;
}
//! [1]

void AxesInputHandler::handleElementSelected(QAbstract3DGraph::ElementType type)
{
    //! [4]
    switch (type) {
    case QAbstract3DGraph::ElementAxisXLabel:
        m_state = StateDraggingX;
        break;
    case QAbstract3DGraph::ElementAxisYLabel:
        m_state = StateDraggingY;
        break;
    case QAbstract3DGraph::ElementAxisZLabel:
        m_state = StateDraggingZ;
        break;
    default:
        m_state = StateNormal;
        break;
    }
    //! [4]
}

void AxesInputHandler::handleAxisDragging()
{
    float distance = 0.0f;

    //! [6]
    // Get scene orientation from active camera
    float xRotation = scene()->activeCamera()->xRotation();
    float yRotation = scene()->activeCamera()->yRotation();
    //! [6]

    //! [7]
    // Calculate directional drag multipliers based on rotation
    float xMulX = qCos(qDegreesToRadians(xRotation));
    float xMulY = qSin(qDegreesToRadians(xRotation));
    float zMulX = qSin(qDegreesToRadians(xRotation));
    float zMulY = qCos(qDegreesToRadians(xRotation));
    //! [7]

    //! [8]
    // Get the drag amount
    QPoint move = inputPosition() - previousInputPos();

    // Flip the effect of y movement if we're viewing from below
    float yMove = (yRotation < 0) ? -move.y() : move.y();
    //! [8]

    //! [9]
    // Adjust axes
    switch (m_state) {
    case StateDraggingX:
        distance = (move.x() * xMulX - yMove * xMulY) / m_speedModifier;
        m_axisX->setRange(m_axisX->min() - distance, m_axisX->max() - distance);
        break;
    case StateDraggingZ:
        distance = (move.x() * zMulX + yMove * zMulY) / m_speedModifier;
        m_axisZ->setRange(m_axisZ->min() + distance, m_axisZ->max() + distance);
        break;
    case StateDraggingY:
        distance = move.y() / m_speedModifier; // No need to use adjusted y move here
        m_axisY->setRange(m_axisY->min() + distance, m_axisY->max() + distance);
        break;
    default:
        break;
    }
    //! [9]
}
