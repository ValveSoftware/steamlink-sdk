/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Quick Templates 2 module of the Qt Toolkit.
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

#include "qquickframe_p.h"
#include "qquickframe_p_p.h"

QT_BEGIN_NAMESPACE

/*!
    \qmltype Frame
    \inherits Pane
    \instantiates QQuickFrame
    \inqmlmodule QtQuick.Controls
    \since 5.7
    \ingroup qtquickcontrols2-containers
    \brief Visual frame for a logical group of controls.

    Frame is used to layout a logical group of controls together within a
    visual frame. Frame does not provide a layout of its own, but requires
    you to position its contents, for instance by creating a \l RowLayout
    or a \l ColumnLayout.

    Items declared as children of a Frame are automatically parented to the
    Frame's \l {Control::}{contentItem}. Items created dynamically need to be
    explicitly parented to the contentItem.

    If only a single item is used within a Frame, it will resize to fit the
    implicit size of its contained item. This makes it particularly suitable
    for use together with layouts.

    \image qtquickcontrols2-frame.png

    \snippet qtquickcontrols2-frame.qml 1

    \sa {Customizing Frame}, {Container Controls}
*/

QQuickFrame::QQuickFrame(QQuickItem *parent) :
    QQuickPane(*(new QQuickFramePrivate), parent)
{
}

QQuickFrame::QQuickFrame(QQuickFramePrivate &dd, QQuickItem *parent) :
    QQuickPane(dd, parent)
{
}

#ifndef QT_NO_ACCESSIBILITY
QAccessible::Role QQuickFrame::accessibleRole() const
{
    return QAccessible::Border;
}
#endif // QT_NO_ACCESSIBILITY

QT_END_NAMESPACE
