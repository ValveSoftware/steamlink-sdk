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

#include "qv4domerrors_p.h"
#include "qv4object_p.h"

QT_BEGIN_NAMESPACE

using namespace QV4;

void qt_add_domexceptions(ExecutionEngine *e)
{
    Scope scope(e);
    ScopedObject domexception(scope, e->newObject());
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
