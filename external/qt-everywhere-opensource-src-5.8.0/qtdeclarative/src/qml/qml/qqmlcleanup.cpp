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

#include "qqmlcleanup_p.h"

#include "qqmlengine_p.h"

QT_BEGIN_NAMESPACE

/*!
\internal
\class QQmlCleanup
\brief The QQmlCleanup provides a callback when a QQmlEngine is deleted.

Any object that needs cleanup to occur before the QQmlEngine's V8 engine is
destroyed should inherit from QQmlCleanup.  The clear() virtual method will be
called by QQmlEngine just before it destroys the context.
*/


/*
Create a QQmlCleanup that is not associated with any engine.
*/
QQmlCleanup::QQmlCleanup()
: prev(0), next(0), engine(0)
{
}

/*!
Create a QQmlCleanup for \a engine
*/
QQmlCleanup::QQmlCleanup(QQmlEngine *engine)
: prev(0), next(0), engine(0)
{
    if (!engine)
        return;

    addToEngine(engine);
}

/*!
Adds this object to \a engine's cleanup list.  hasEngine() must be false
before calling this method.
*/
void QQmlCleanup::addToEngine(QQmlEngine *engine)
{
    Q_ASSERT(engine);
    Q_ASSERT(QQmlEnginePrivate::isEngineThread(engine));

    this->engine = engine;

    QQmlEnginePrivate *p = QQmlEnginePrivate::get(engine);

    if (p->cleanup) next = p->cleanup;
    p->cleanup = this;
    prev = &p->cleanup;
    if (next) next->prev = &next;
}

/*!
\fn bool QQmlCleanup::hasEngine() const

Returns true if this QQmlCleanup is associated with an engine, otherwise false.
*/

/*!
\internal
*/
QQmlCleanup::~QQmlCleanup()
{
    Q_ASSERT(!prev || engine);
    Q_ASSERT(!prev || QQmlEnginePrivate::isEngineThread(engine));

    if (prev) *prev = next;
    if (next) next->prev = prev;
    prev = 0;
    next = 0;
}

QT_END_NAMESPACE
