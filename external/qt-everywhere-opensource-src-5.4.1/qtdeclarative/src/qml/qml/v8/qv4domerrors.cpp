/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtQml module of the Qt Toolkit.
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

#include "qv4domerrors_p.h"
#include "qv4object_p.h"

QT_BEGIN_NAMESPACE

using namespace QV4;

void qt_add_domexceptions(ExecutionEngine *e)
{
    Scope scope(e);
    Scoped<Object> domexception(scope, e->newObject());
    domexception->defineReadonlyProperty(QStringLiteral("INDEX_SIZE_ERR"), Primitive::fromInt32(DOMEXCEPTION_INDEX_SIZE_ERR));
    domexception->defineReadonlyProperty(QStringLiteral("DOMSTRING_SIZE_ERR"), Primitive::fromInt32(DOMEXCEPTION_DOMSTRING_SIZE_ERR));
    domexception->defineReadonlyProperty(QStringLiteral("HIERARCHY_REQUEST_ERR"), Primitive::fromInt32(DOMEXCEPTION_HIERARCHY_REQUEST_ERR));
    domexception->defineReadonlyProperty(QStringLiteral("WRONG_DOCUMENT_ERR"), Primitive::fromInt32(DOMEXCEPTION_WRONG_DOCUMENT_ERR));
    domexception->defineReadonlyProperty(QStringLiteral("INVALID_CHARACTER_ERR"), Primitive::fromInt32(DOMEXCEPTION_INVALID_CHARACTER_ERR));
    domexception->defineReadonlyProperty(QStringLiteral("NO_DATA_ALLOWED_ERR"), Primitive::fromInt32(DOMEXCEPTION_NO_DATA_ALLOWED_ERR));
    domexception->defineReadonlyProperty(QStringLiteral("NO_MODIFICATION_ALLOWED_ERR"), Primitive::fromInt32(DOMEXCEPTION_NO_MODIFICATION_ALLOWED_ERR));
    domexception->defineReadonlyProperty(QStringLiteral("NOT_FOUND_ERR"), Primitive::fromInt32(DOMEXCEPTION_NOT_FOUND_ERR));
    domexception->defineReadonlyProperty(QStringLiteral("NOT_SUPPORTED_ERR"), Primitive::fromInt32(DOMEXCEPTION_NOT_SUPPORTED_ERR));
    domexception->defineReadonlyProperty(QStringLiteral("INUSE_ATTRIBUTE_ERR"), Primitive::fromInt32(DOMEXCEPTION_INUSE_ATTRIBUTE_ERR));
    domexception->defineReadonlyProperty(QStringLiteral("INVALID_STATE_ERR"), Primitive::fromInt32(DOMEXCEPTION_INVALID_STATE_ERR));
    domexception->defineReadonlyProperty(QStringLiteral("SYNTAX_ERR"), Primitive::fromInt32(DOMEXCEPTION_SYNTAX_ERR));
    domexception->defineReadonlyProperty(QStringLiteral("INVALID_MODIFICATION_ERR"), Primitive::fromInt32(DOMEXCEPTION_INVALID_MODIFICATION_ERR));
    domexception->defineReadonlyProperty(QStringLiteral("NAMESPACE_ERR"), Primitive::fromInt32(DOMEXCEPTION_NAMESPACE_ERR));
    domexception->defineReadonlyProperty(QStringLiteral("INVALID_ACCESS_ERR"), Primitive::fromInt32(DOMEXCEPTION_INVALID_ACCESS_ERR));
    domexception->defineReadonlyProperty(QStringLiteral("VALIDATION_ERR"), Primitive::fromInt32(DOMEXCEPTION_VALIDATION_ERR));
    domexception->defineReadonlyProperty(QStringLiteral("TYPE_MISMATCH_ERR"), Primitive::fromInt32(DOMEXCEPTION_TYPE_MISMATCH_ERR));
    e->globalObject->defineDefaultProperty(QStringLiteral("DOMException"), domexception);
}

QT_END_NAMESPACE
