/****************************************************************************
**
** Copyright (C) 2016 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQml module of the Qt Toolkit.
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

#include "qqmlloggingcategory_p.h"

#include <QtQml/qqmlinfo.h>

/*!
    \qmltype LoggingCategory
    \ingroup qml-utility-elements
    \inqmlmodule QtQml
    \brief Defines a logging category in QML
    \since 5.8

    A logging category can be passed to console.log() and friends as the first argument.
    If supplied to to the logger the LoggingCategory's name will be used as Logging Category
    otherwise the default logging category will be used.

    \qml
    import QtQuick 2.8

    Item {
        LoggingCategory {
            id: category
            name: "com.qt.category"
        }

        Component.onCompleted: {
          console.log(category, "message");
        }
    }
    \endqml

    \note As the creation of objects is expensive, it is encouraged to put the needed
    LoggingCategory definitions into a singleton and import this where needed.

    \sa QLoggingCategory
*/

/*!
    \qmlproperty string QtQml::LoggingCategory::name

    Holds the name of the logging category.

    \note This property needs to be set when declaring the LoggingCategory
    and cannot be changed later.

    \sa QLoggingCategory::categoryName()
*/

QQmlLoggingCategory::QQmlLoggingCategory(QObject *parent)
    : QObject(parent)
    , m_initialized(false)
{
}

QQmlLoggingCategory::~QQmlLoggingCategory()
{
}

QString QQmlLoggingCategory::name() const
{
    return QString::fromUtf8(m_name);
}

QLoggingCategory *QQmlLoggingCategory::category() const
{
    return m_category.data();
}

void QQmlLoggingCategory::classBegin()
{
}

void QQmlLoggingCategory::componentComplete()
{
    m_initialized = true;
    if (m_name.isNull())
        qmlInfo(this) << QString(QLatin1String("Declaring the name of the LoggingCategory is mandatory and cannot be changed later !"));
}

void QQmlLoggingCategory::setName(const QString &name)
{
    if (m_initialized) {
        qmlInfo(this) << QString(QLatin1String("The name of a LoggingCategory cannot be changed after the Item is created"));
        return;
    }

    m_name = name.toUtf8();
    QScopedPointer<QLoggingCategory> category(new QLoggingCategory(m_name.constData()));
    m_category.swap(category);
}
