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

#include "qabstract3dinputhandler_p.h"

QT_BEGIN_NAMESPACE_DATAVISUALIZATION

/*!
 * \class QAbstract3DInputHandler
 * \inmodule QtDataVisualization
 * \brief The base class for implementations of input handlers.
 * \since QtDataVisualization 1.0
 *
 * QAbstract3DInputHandler is the base class that is subclassed by different input handling implementations
 * that take input events and translate those to camera and light movements. Input handlers also translate
 * raw input events to slicing and selection events in the scene.
 */

/*!
 * \enum QAbstract3DInputHandler::InputView
 *
 * Predefined input views for mouse and touch based input handlers.
 *
 * \value InputViewNone
 *        Mouse or touch not on a view.
 * \value InputViewOnPrimary
 *        Mouse or touch input received on the primary view area. If secondary view is displayed when
 *        inputView becomes InputViewOnPrimary, secondary view is closed.
 * \value InputViewOnSecondary
 *        Mouse or touch input received on the secondary view area.
 */

/*!
 * \qmltype AbstractInputHandler3D
 * \inqmlmodule QtDataVisualization
 * \since QtDataVisualization 1.0
 * \ingroup datavisualization_qml
 * \instantiates QAbstract3DInputHandler
 * \brief Base type for all QtDataVisualization input handlers.
 *
 * This type is uncreatable.
 *
 * For AbstractInputHandler3D enums, see \l{QAbstract3DInputHandler::InputView}.
 */

/*!
 * Constructs the base class. An optional \a parent parameter can be given
 * and is then passed to QObject constructor.
 */
QAbstract3DInputHandler::QAbstract3DInputHandler(QObject *parent) :
    QObject(parent),
    d_ptr(new QAbstract3DInputHandlerPrivate(this))
{
}

/*!
 *  Destroys the base class.
 */
QAbstract3DInputHandler::~QAbstract3DInputHandler()
{
}

// Input event listeners
/*!
 * Override this to handle mouse double click events.
 * Mouse double click event is given in the \a event.
 */
void QAbstract3DInputHandler::mouseDoubleClickEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
}

/*!
 * Override this to handle touch input events.
 * Touch event is given in the \a event.
 */
void QAbstract3DInputHandler::touchEvent(QTouchEvent *event)
{
    Q_UNUSED(event);
}

/*!
 * Override this to handle mouse press events.
 * Mouse press event is given in the \a event and the mouse position in \a mousePos.
 */
void QAbstract3DInputHandler::mousePressEvent(QMouseEvent *event, const QPoint &mousePos)
{
    Q_UNUSED(event);
    Q_UNUSED(mousePos);
}

/*!
 * Override this to handle mouse release events.
 * Mouse release event is given in the \a event and the mouse position in \a mousePos.
 */
void QAbstract3DInputHandler::mouseReleaseEvent(QMouseEvent *event, const QPoint &mousePos)
{
    Q_UNUSED(event);
    Q_UNUSED(mousePos);
}

/*!
 * Override this to handle mouse move events.
 * Mouse move event is given in the \a event and the mouse position in \a mousePos.
 */
void QAbstract3DInputHandler::mouseMoveEvent(QMouseEvent *event, const QPoint &mousePos)
{
    Q_UNUSED(event);
    Q_UNUSED(mousePos);
}

/*!
 * Override this to handle wheel events.
 * Wheel event is given in the \a event.
 */
void QAbstract3DInputHandler::wheelEvent(QWheelEvent *event)
{
    Q_UNUSED(event);
}

// Property get/set
/*!
 * \property QAbstract3DInputHandler::inputView
 *
 * Current enumerated input view based on the view of the processed input events.
 * When the view changes \c inputViewChanged signal is emitted.
 */
QAbstract3DInputHandler::InputView QAbstract3DInputHandler::inputView() const
{
    return d_ptr->m_inputView;
}

void QAbstract3DInputHandler::setInputView(InputView inputView)
{
    if (inputView != d_ptr->m_inputView) {
        d_ptr->m_inputView = inputView;
        emit inputViewChanged(inputView);
    }
}

/*!
 * \property QAbstract3DInputHandler::inputPosition
 *
 * Last input position based on the processed input events.
 */
QPoint QAbstract3DInputHandler::inputPosition() const
{
    return d_ptr->m_inputPosition;
}

void QAbstract3DInputHandler::setInputPosition(const QPoint &position)
{
    if (position != d_ptr->m_inputPosition) {
        d_ptr->m_inputPosition = position;
        emit positionChanged(position);
    }
}

/*!
 * \return the manhattan length between last two input positions.
 */
int QAbstract3DInputHandler::prevDistance() const
{
    return d_ptr->m_prevDistance;
}

/*!
 * Sets the \a distance (manhattan length) between last two input positions.
 */
void QAbstract3DInputHandler::setPrevDistance(int distance)
{
    d_ptr->m_prevDistance = distance;
}

/*!
 * \property QAbstract3DInputHandler::scene
 *
 * The 3D scene this abstract input handler is controlling. Only one scene can
 * be controlled by one input handler. Setting a \a scene to an input handler doesn't
 * transfer the ownership of the \a scene.
 */
Q3DScene *QAbstract3DInputHandler::scene() const
{
    return d_ptr->m_scene;
}

void QAbstract3DInputHandler::setScene(Q3DScene *scene)
{
    if (scene != d_ptr->m_scene) {
        d_ptr->m_scene = scene;
        emit sceneChanged(scene);
    }
}

/*!
 * Sets the previous input position to the point given by \a position.
 */
void QAbstract3DInputHandler::setPreviousInputPos(const QPoint &position)
{
    d_ptr->m_previousInputPos = position;
}

/*!
 * Returns the previous input position.
 * \return Previous input position.
 */
QPoint QAbstract3DInputHandler::previousInputPos() const
{
    return d_ptr->m_previousInputPos;
}

QAbstract3DInputHandlerPrivate::QAbstract3DInputHandlerPrivate(QAbstract3DInputHandler *q) :
    q_ptr(q),
    m_prevDistance(0),
    m_previousInputPos(QPoint(0,0)),
    m_inputView(QAbstract3DInputHandler::InputViewNone),
    m_inputPosition(QPoint(0,0)),
    m_scene(0),
    m_isDefaultHandler(false)
{
}

QAbstract3DInputHandlerPrivate::~QAbstract3DInputHandlerPrivate()
{

}

QT_END_NAMESPACE_DATAVISUALIZATION
