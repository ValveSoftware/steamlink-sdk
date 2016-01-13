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
#ifndef QV4FUNCTION_H
#define QV4FUNCTION_H

#include "qv4global_p.h"

#include <QtCore/QVector>
#include <QtCore/QByteArray>
#include <QtCore/qurl.h>

#include "qv4value_p.h"
#include <private/qv4compileddata_p.h>
#include <private/qv4engine_p.h>

QT_BEGIN_NAMESPACE

namespace QV4 {

struct String;
struct Function;
struct Object;
struct FunctionObject;
struct ExecutionContext;
struct ExecutionEngine;
class MemoryManager;

struct ObjectPrototype;
struct StringPrototype;
struct NumberPrototype;
struct BooleanPrototype;
struct ArrayPrototype;
struct FunctionPrototype;
struct DatePrototype;
struct ErrorPrototype;
struct EvalErrorPrototype;
struct RangeErrorPrototype;
struct ReferenceErrorPrototype;
struct SyntaxErrorPrototype;
struct TypeErrorPrototype;
struct URIErrorPrototype;
struct InternalClass;
struct Lookup;

struct Q_QML_EXPORT Function {
    const CompiledData::Function *compiledFunction;
    CompiledData::CompilationUnit *compilationUnit;

    ReturnedValue (*code)(ExecutionContext *, const uchar *);
    const uchar *codeData;

    // first nArguments names in internalClass are the actual arguments
    InternalClass *internalClass;

    Function(ExecutionEngine *engine, CompiledData::CompilationUnit *unit, const CompiledData::Function *function,
             ReturnedValue (*codePtr)(ExecutionContext *, const uchar *));
    ~Function();

    inline String *name() {
        return compilationUnit->runtimeStrings[compiledFunction->nameIndex].getPointer();
    }
    inline QString sourceFile() const { return compilationUnit->fileName(); }

    inline bool usesArgumentsObject() const { return compiledFunction->flags & CompiledData::Function::UsesArgumentsObject; }
    inline bool isStrict() const { return compiledFunction->flags & CompiledData::Function::IsStrict; }
    inline bool isNamedExpression() const { return compiledFunction->flags & CompiledData::Function::IsNamedExpression; }

    inline bool needsActivation() const
    { return compiledFunction->nInnerFunctions > 0 || (compiledFunction->flags & (CompiledData::Function::HasDirectEval | CompiledData::Function::UsesArgumentsObject)); }

};

}

QT_END_NAMESPACE

#endif
