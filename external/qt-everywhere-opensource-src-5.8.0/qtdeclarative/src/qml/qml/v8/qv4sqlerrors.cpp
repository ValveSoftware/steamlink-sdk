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

#include "qv4sqlerrors_p.h"
#include "private/qv4engine_p.h"
#include "private/qv4object_p.h"

QT_BEGIN_NAMESPACE

using namespace QV4;

void qt_add_sqlexceptions(QV4::ExecutionEngine *engine)
{
    Scope scope(engine);
    ScopedObject sqlexception(scope, engine->newObject());
    sqlexception->defineReadonlyProperty(QStringLiteral("UNKNOWN_ERR"), Primitive::fromInt32(SQLEXCEPTION_UNKNOWN_ERR));
    sqlexception->defineReadonlyProperty(QStringLiteral("DATABASE_ERR"), Primitive::fromInt32(SQLEXCEPTION_DATABASE_ERR));
    sqlexception->defineReadonlyProperty(QStringLiteral("VERSION_ERR"), Primitive::fromInt32(SQLEXCEPTION_VERSION_ERR));
    sqlexception->defineReadonlyProperty(QStringLiteral("TOO_LARGE_ERR"), Primitive::fromInt32(SQLEXCEPTION_TOO_LARGE_ERR));
    sqlexception->defineReadonlyProperty(QStringLiteral("QUOTA_ERR"), Primitive::fromInt32(SQLEXCEPTION_QUOTA_ERR));
    sqlexception->defineReadonlyProperty(QStringLiteral("SYNTAX_ERR"), Primitive::fromInt32(SQLEXCEPTION_SYNTAX_ERR));
    sqlexception->defineReadonlyProperty(QStringLiteral("CONSTRAINT_ERR"), Primitive::fromInt32(SQLEXCEPTION_CONSTRAINT_ERR));
    sqlexception->defineReadonlyProperty(QStringLiteral("TIMEOUT_ERR"), Primitive::fromInt32(SQLEXCEPTION_TIMEOUT_ERR));
    engine->globalObject->defineDefaultProperty(QStringLiteral("SQLException"), sqlexception);
}

QT_END_NAMESPACE
