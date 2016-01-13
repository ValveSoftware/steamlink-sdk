/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Quick Controls module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qquickabstractstyle_p.h"

QT_BEGIN_NAMESPACE

/*!
    \qmltype AbstractStyle
    \instantiates QQuickAbstractStyle
    \qmlabstract
    \internal
*/

/*!
    \qmlpropertygroup QtQuick.Controls.Styles::AbstractStyle::padding
    \qmlproperty int AbstractStyle::padding.top
    \qmlproperty int AbstractStyle::padding.left
    \qmlproperty int AbstractStyle::padding.right
    \qmlproperty int AbstractStyle::padding.bottom

    This grouped property holds the \c top, \c left, \c right and \c bottom padding.
*/

QQuickAbstractStyle::QQuickAbstractStyle(QObject *parent) : QObject(parent)
{
}

QQmlListProperty<QObject> QQuickAbstractStyle::data()
{
    return QQmlListProperty<QObject>(this, 0, &QQuickAbstractStyle::data_append, &QQuickAbstractStyle::data_count,
                                     &QQuickAbstractStyle::data_at, &QQuickAbstractStyle::data_clear);
}

void QQuickAbstractStyle::data_append(QQmlListProperty<QObject> *list, QObject *object)
{
    if (QQuickAbstractStyle *style = qobject_cast<QQuickAbstractStyle *>(list->object))
        style->m_data.append(object);
}

int QQuickAbstractStyle::data_count(QQmlListProperty<QObject> *list)
{
    if (QQuickAbstractStyle *style = qobject_cast<QQuickAbstractStyle *>(list->object))
        return style->m_data.count();
    return 0;
}

QObject *QQuickAbstractStyle::data_at(QQmlListProperty<QObject> *list, int index)
{
    if (QQuickAbstractStyle *style = qobject_cast<QQuickAbstractStyle *>(list->object))
        return style->m_data.at(index);
    return 0;
}

void QQuickAbstractStyle::data_clear(QQmlListProperty<QObject> *list)
{
    if (QQuickAbstractStyle *style = qobject_cast<QQuickAbstractStyle *>(list->object))
        style->m_data.clear();
}

QT_END_NAMESPACE
