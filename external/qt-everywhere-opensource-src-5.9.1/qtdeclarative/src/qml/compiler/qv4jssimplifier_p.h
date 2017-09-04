/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the tools applications of the Qt Toolkit.
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
#ifndef QV4JSSIMPLIFIER
#define QV4JSSIMPLIFIER

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <private/qv4global_p.h>

#include "qqmlirbuilder_p.h"

QT_BEGIN_NAMESPACE

namespace QmlIR {
struct Document;
}

namespace QV4 {
namespace CompiledData {
struct QmlUnit;
struct Location;
}
}

class QQmlJavaScriptBindingExpressionSimplificationPass
{
public:
    QQmlJavaScriptBindingExpressionSimplificationPass(const QVector<QmlIR::Object*> &qmlObjects, QV4::IR::Module *jsModule, QV4::Compiler::JSUnitGenerator *unitGenerator);

    void reduceTranslationBindings();

private:
    void reduceTranslationBindings(int objectIndex);

    void visit(QV4::IR::Stmt *s)
    {
        switch (s->stmtKind) {
        case QV4::IR::Stmt::MoveStmt:
            visitMove(s->asMove());
            break;
        case QV4::IR::Stmt::RetStmt:
            visitRet(s->asRet());
            break;
        case QV4::IR::Stmt::CJumpStmt:
            discard();
            break;
        case QV4::IR::Stmt::ExpStmt:
            discard();
            break;
        case QV4::IR::Stmt::JumpStmt:
            break;
        case QV4::IR::Stmt::PhiStmt:
            break;
        }
    }

    void visitMove(QV4::IR::Move *move);
    void visitRet(QV4::IR::Ret *ret);

    void visitFunctionCall(const QString *name, QV4::IR::ExprList *args, QV4::IR::Temp *target);

    void discard() { _canSimplify = false; }

    bool simplifyBinding(QV4::IR::Function *function, QmlIR::Binding *binding);
    bool detectTranslationCallAndConvertBinding(QmlIR::Binding *binding);

    const QVector<QmlIR::Object*> &qmlObjects;
    QV4::IR::Module *jsModule;
    QV4::Compiler::JSUnitGenerator *unitGenerator;

    bool _canSimplify;
    const QString *_nameOfFunctionCalled;
    QVector<int> _functionParameters;
    int _functionCallReturnValue;

    QHash<int, QV4::IR::Expr*> _temps;
    int _returnValueOfBindingExpression;
    int _synthesizedConsts;

    QVector<int> irFunctionsToRemove;
};

class QQmlIRFunctionCleanser
{
public:
    QQmlIRFunctionCleanser(QV4::IR::Module *module, const QVector<QmlIR::Object*> &qmlObjects, const QVector<int> &functionsToRemove);

    void clean();

private:
    virtual void visitMove(QV4::IR::Move *s) {
        visit(s->source);
        visit(s->target);
    }

    void visit(QV4::IR::Stmt *s);
    void visit(QV4::IR::Expr *e);

private:
    QV4::IR::Module *module;
    const QVector<QmlIR::Object*> &qmlObjects;
    const QVector<int> &functionsToRemove;

    QVector<int> newFunctionIndices;
};

QT_END_NAMESPACE

#endif // QV4JSSIMPLIFIER
