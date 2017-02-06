/****************************************************************************
**
** Copyright (C) 2015 Klaralvdalens Datakonsult AB (KDAB).
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
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

#include "qframeaction.h"
#include "qframeaction_p.h"

QT_BEGIN_NAMESPACE

namespace Qt3DLogic {

QFrameActionPrivate::QFrameActionPrivate()
    : QComponentPrivate()
{
}

/*!
    \class Qt3DLogic::QFrameAction
    \inmodule Qt3DLogic
    \since 5.5
    \brief Provides a way to have a synchronous function executed each frame.

    The QFrameAction provides a way to perform tasks each frame in a
    synchronized way with the Qt3D backend. This is useful to implement some
    aspects of application logic and to prototype functionality that can later
    be folded into an additional Qt3D aspect.

    For example, the QFrameAction can be used to animate a property in sync
    with the Qt3D engine where a Qt Quick animation element is not perfectly
    synchronized and may lead to stutters in some cases.

    To execute your own code each frame connect to the QFrameAction::triggered signal.
*/

/*!
    \qmltype FrameAction
    \inqmlmodule Qt3D.Logic
    \instantiates Qt3DLogic::QFrameAction
    \inherits Component3D
    \since 5.5
    \brief Provides a way to have a synchronous function executed each frame.

    The FrameAction provides a way to perform tasks each frame in a
    synchronized way with the Qt3D backend. This is useful to implement some
    aspects of application logic and to prototype functionality that can later
    be folded into an additional Qt3D aspect.

    For example, the FrameAction can be used to animate a property in sync
    with the Qt3D engine where a Qt Quick animation element is not perfectly
    synchronized and may lead to stutters in some cases.

    To execute your own code each frame connect to the FrameAction::triggered signal.
*/

/*!
    Constructs a new QFrameAction instance with parent \a parent.
 */
QFrameAction::QFrameAction(QNode *parent)
    : QComponent(*new QFrameActionPrivate, parent)
{
}

/*! \internal */
QFrameAction::~QFrameAction()
{
}

/*! \internal */
QFrameAction::QFrameAction(QFrameActionPrivate &dd, QNode *parent)
    : QComponent(dd, parent)
{
}

/*!
    \internal
    This function will be called in a synchronous manner once each frame by
    the Logic aspect.
*/
void QFrameAction::onTriggered(float dt)
{
    // Emit signal so that QML instances get the onTriggered() signal
    // handler called
    emit triggered(dt);
}

/*!
    \qmlsignal Qt3D.Logic::FrameAction::triggered(real dt)
    This signal is emitted each frame.
*/

/*!
    \fn QFrameAction::triggered(float dt)
    This signal is emitted each frame with \a dt being the time since the last triggering.
*/
} // namespace Qt3DLogic

QT_END_NAMESPACE
