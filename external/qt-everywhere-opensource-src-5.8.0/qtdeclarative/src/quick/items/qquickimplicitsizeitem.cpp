/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQuick module of the Qt Toolkit.
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

#include "qquickimplicitsizeitem_p.h"
#include "qquickimplicitsizeitem_p_p.h"

QT_BEGIN_NAMESPACE

/*!
    \internal

    The purpose of QQuickImplicitSizeItem is not immediately clear, as both
    the implicit size properties and signals exist on QQuickItem. However,
    for some items - where the implicit size has an underlying meaning (such as
    Image, where the implicit size represents the real size of the image)
    having implicit size writable is an undesirable thing.

    QQuickImplicitSizeItem redefines the properties as being readonly.
    Unfortunately, this also means they need to redefine the change signals.
    See QTBUG-30258 for more information.
*/
void QQuickImplicitSizeItemPrivate::implicitWidthChanged()
{
    Q_Q(QQuickImplicitSizeItem);
    QQuickItemPrivate::implicitWidthChanged();
    emit q->implicitWidthChanged2();
}

void QQuickImplicitSizeItemPrivate::implicitHeightChanged()
{
    Q_Q(QQuickImplicitSizeItem);
    QQuickItemPrivate::implicitHeightChanged();
    emit q->implicitHeightChanged2();
}

QQuickImplicitSizeItem::QQuickImplicitSizeItem(QQuickImplicitSizeItemPrivate &dd, QQuickItem *parent)
    : QQuickItem(dd, parent)
{
}

QT_END_NAMESPACE
