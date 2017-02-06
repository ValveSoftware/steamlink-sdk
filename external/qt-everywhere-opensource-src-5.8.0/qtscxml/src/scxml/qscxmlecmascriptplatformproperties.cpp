/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtScxml module of the Qt Toolkit.
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

#include "qscxmlecmascriptplatformproperties_p.h"
#include "qscxmlstatemachine.h"

#include <QJSEngine>

QT_BEGIN_NAMESPACE
class QScxmlPlatformProperties::Data
{
public:
    Data()
        : m_stateMachine(Q_NULLPTR)
    {}

    QScxmlStateMachine *m_stateMachine;
    QJSValue m_jsValue;
};

QScxmlPlatformProperties::QScxmlPlatformProperties(QObject *parent)
    : QObject(parent)
    , data(new Data)
{}

QScxmlPlatformProperties *QScxmlPlatformProperties::create(QJSEngine *engine, QScxmlStateMachine *stateMachine)
{
    QScxmlPlatformProperties *pp = new QScxmlPlatformProperties(engine);
    pp->data->m_stateMachine = stateMachine;
    pp->data->m_jsValue = engine->newQObject(pp);
    return pp;
}

QScxmlPlatformProperties::~QScxmlPlatformProperties()
{
    delete data;
}

QJSEngine *QScxmlPlatformProperties::engine() const
{
    return qobject_cast<QJSEngine *>(parent());
}

QScxmlStateMachine *QScxmlPlatformProperties::stateMachine() const
{
    return data->m_stateMachine;
}

QJSValue QScxmlPlatformProperties::jsValue() const
{
    return data->m_jsValue;
}

/// _x.marks === "the spot"
QString QScxmlPlatformProperties::marks() const
{
    return QStringLiteral("the spot");
}

bool QScxmlPlatformProperties::inState(const QString &stateName)
{
    return stateMachine()->isActive(stateName);
}

QT_END_NAMESPACE
