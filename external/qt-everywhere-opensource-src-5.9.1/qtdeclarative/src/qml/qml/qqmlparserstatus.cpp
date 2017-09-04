/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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

#include "qqmlparserstatus.h"

QT_BEGIN_NAMESPACE

/*!
    \class QQmlParserStatus
    \since 5.0
    \inmodule QtQml
    \brief The QQmlParserStatus class provides updates on the QML parser state.

    QQmlParserStatus provides a mechanism for classes instantiated by
    a QQmlEngine to receive notification at key points in their creation.

    This class is often used for optimization purposes, as it allows you to defer an
    expensive operation until after all the properties have been set on an
    object. For example, QML's \l {Text} element uses the parser status
    to defer text layout until all of its properties have been set (we
    don't want to layout when the \c text is assigned, and then relayout
    when the \c font is assigned, and relayout again when the \c width is assigned,
    and so on).

    Be aware that QQmlParserStatus methods are only called when a class is instantiated
    by a QQmlEngine. If you create the same class directly from C++, these methods will
    not be called automatically. To avoid this problem, it is recommended that you start
    deferring operations from classBegin instead of from the initial creation of your class.
    This will still prevent multiple revaluations during initial binding assignment in QML,
    but will not defer operations invoked from C++.

    To use QQmlParserStatus, you must inherit both a QObject-derived class
    and QQmlParserStatus, and use the Q_INTERFACES() macro.

    \code
    class MyObject : public QObject, public QQmlParserStatus
    {
        Q_OBJECT
        Q_INTERFACES(QQmlParserStatus)

    public:
        MyObject(QObject *parent = 0);
        ...
        void classBegin();
        void componentComplete();
    }
    \endcode

    The \l {Qt Quick 1} version of this class is named QDeclarativeParserStatus.
*/

/*! \internal */
QQmlParserStatus::QQmlParserStatus()
: d(0)
{
}

/*! \internal */
QQmlParserStatus::~QQmlParserStatus()
{
    if(d)
        (*d) = 0;
}

/*!
    \fn void QQmlParserStatus::classBegin()

    Invoked after class creation, but before any properties have been set.
*/

/*!
    \fn void QQmlParserStatus::componentComplete()

    Invoked after the root component that caused this instantiation has
    completed construction.  At this point all static values and binding values
    have been assigned to the class.
*/

QT_END_NAMESPACE
