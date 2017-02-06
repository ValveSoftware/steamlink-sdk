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

#include "qquicktoolseparator_p.h"

#include "qquickcontrol_p_p.h"

QT_BEGIN_NAMESPACE

/*!
    \qmltype ToolSeparator
    \inherits Control
    \instantiates QQuickToolSeparator
    \inqmlmodule QtQuick.Controls
    \since 5.8
    \ingroup qtquickcontrols2-separators
    \brief Separates a group of items in a toolbar from adjacent items.

    ToolSeparator is used to visually distinguish between groups of items in a
    toolbar by separating them with a line. It can be used in horizontal or
    vertical toolbars by setting the \l orientation property to \c Qt.Vertical
    or \c Qt.Horizontal, respectively.

    \image qtquickcontrols2-toolseparator.png

    \snippet qtquickcontrols2-toolseparator.qml 1

    \sa {Customizing ToolSeparator}, {Separator Controls}
*/

class QQuickToolSeparatorPrivate : public QQuickControlPrivate
{
    Q_DECLARE_PUBLIC(QQuickToolSeparator)

public:
    QQuickToolSeparatorPrivate() :
        orientation(Qt::Vertical)
    {
    }

    Qt::Orientation orientation;
};

QQuickToolSeparator::QQuickToolSeparator(QQuickItem *parent) :
    QQuickControl(*(new QQuickToolSeparatorPrivate), parent)
{
}

/*!
    \qmlproperty enumeration QtQuick.Controls::ToolSeparator::orientation

    This property holds the orientation of the tool separator.

    Possible values:
    \value Qt.Horizontal A horizontal separator is used in a vertical toolbar.
    \value Qt.Vertical A vertical separator is used in a horizontal toolbar. (default)
*/
Qt::Orientation QQuickToolSeparator::orientation() const
{
    Q_D(const QQuickToolSeparator);
    return d->orientation;
}

void QQuickToolSeparator::setOrientation(Qt::Orientation orientation)
{
    Q_D(QQuickToolSeparator);
    if (d->orientation == orientation)
        return;

    d->orientation = orientation;
    emit orientationChanged();
}

/*!
    \readonly
    \qmlproperty bool QtQuick.Controls::ToolSeparator::horizontal

    This property holds whether \l orientation is equal to \c Qt.Horizontal.

    It is useful for \l {Customizing ToolSeparator}{customizing ToolSeparator}.

    \sa orientation, vertical
*/
bool QQuickToolSeparator::isHorizontal() const
{
    Q_D(const QQuickToolSeparator);
    return d->orientation == Qt::Horizontal;
}

/*!
    \readonly
    \qmlproperty bool QtQuick.Controls::ToolSeparator::vertical

    This property holds whether \l orientation is equal to \c Qt.Vertical.

    It is useful for \l {Customizing ToolSeparator}{customizing ToolSeparator}.

    \sa orientation, horizontal
*/
bool QQuickToolSeparator::isVertical() const
{
    Q_D(const QQuickToolSeparator);
    return d->orientation == Qt::Vertical;
}

#ifndef QT_NO_ACCESSIBILITY
QAccessible::Role QQuickToolSeparator::accessibleRole() const
{
    return QAccessible::Separator;
}
#endif

QT_END_NAMESPACE
