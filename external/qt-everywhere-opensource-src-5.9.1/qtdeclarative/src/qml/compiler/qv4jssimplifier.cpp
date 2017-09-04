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

#include "qv4jssimplifier_p.h"

QT_BEGIN_NAMESPACE

QQmlJavaScriptBindingExpressionSimplificationPass::QQmlJavaScriptBindingExpressionSimplificationPass(const QVector<QmlIR::Object*> &qmlObjects, QV4::IR::Module *jsModule, QV4::Compiler::JSUnitGenerator *unitGenerator)
    : qmlObjects(qmlObjects)
    , jsModule(jsModule)
    , unitGenerator(unitGenerator)
{

}

void QQmlJavaScriptBindingExpressionSimplificationPass::reduceTranslationBindings()
{
    for (int i = 0; i < qmlObjects.count(); ++i)
        reduceTranslationBindings(i);
    if (!irFunctionsToRemove.isEmpty()) {
        QQmlIRFunctionCleanser cleanser(jsModule, qmlObjects, irFunctionsToRemove);
        cleanser.clean();
    }
}

void QQmlJavaScriptBindingExpressionSimplificationPass::reduceTranslationBindings(int objectIndex)
{
    const QmlIR::Object *obj = qmlObjects.at(objectIndex);

    for (QmlIR::Binding *binding = obj->firstBinding(); binding; binding = binding->next) {
        if (binding->type != QV4::CompiledData::Binding::Type_Script)
            continue;

        const int irFunctionIndex = obj->runtimeFunctionIndices.at(binding->value.compiledScriptIndex);
        QV4::IR::Function *irFunction = jsModule->functions.at(irFunctionIndex);
        if (simplifyBinding(irFunction, binding)) {
            irFunctionsToRemove.append(irFunctionIndex);
            jsModule->functions[irFunctionIndex] = 0;
            delete irFunction;
        }
    }
}

void QQmlJavaScriptBindingExpressionSimplificationPass::visitMove(QV4::IR::Move *move)
{
    QV4::IR::Temp *target = move->target->asTemp();
    if (!target || target->kind != QV4::IR::Temp::VirtualRegister) {
        discard();
        return;
    }

    if (QV4::IR::Call *call = move->source->asCall()) {
        if (QV4::IR::Name *n = call->base->asName()) {
            if (n->builtin == QV4::IR::Name::builtin_invalid) {
                visitFunctionCall(n->id, call->args, target);
                return;
            }
        }
        discard();
        return;
    }

    if (QV4::IR::Name *n = move->source->asName()) {
        if (n->builtin == QV4::IR::Name::builtin_qml_context
            || n->builtin == QV4::IR::Name::builtin_qml_imported_scripts_object) {
            // these are free of side-effects
            return;
        }
        discard();
        return;
    }

    if (!move->source->asTemp() && !move->source->asString() && !move->source->asConst()) {
        discard();
        return;
    }

    _temps[target->index] = move->source;
}

void QQmlJavaScriptBindingExpressionSimplificationPass::visitFunctionCall(const QString *name, QV4::IR::ExprList *args, QV4::IR::Temp *target)
{
    // more than one function call?
    if (_nameOfFunctionCalled) {
        discard();
        return;
    }

    _nameOfFunctionCalled = name;

    _functionParameters.clear();
    while (args) {
        int slot;
        if (QV4::IR::Temp *param = args->expr->asTemp()) {
            if (param->kind != QV4::IR::Temp::VirtualRegister) {
                discard();
                return;
            }
            slot = param->index;
            _functionParameters.append(slot);
        } else if (QV4::IR::Const *param = args->expr->asConst()) {
            slot = --_synthesizedConsts;
            Q_ASSERT(!_temps.contains(slot));
            _temps[slot] = param;
            _functionParameters.append(slot);
        }
        args = args->next;
    }

    _functionCallReturnValue = target->index;
}

void QQmlJavaScriptBindingExpressionSimplificationPass::visitRet(QV4::IR::Ret *ret)
{
    // nothing initialized earlier?
    if (_returnValueOfBindingExpression != -1) {
        discard();
        return;
    }
    QV4::IR::Temp *target = ret->expr->asTemp();
    if (!target || target->kind != QV4::IR::Temp::VirtualRegister) {
        discard();
        return;
    }
    _returnValueOfBindingExpression = target->index;
}

bool QQmlJavaScriptBindingExpressionSimplificationPass::simplifyBinding(QV4::IR::Function *function, QmlIR::Binding *binding)
{
    _canSimplify = true;
    _nameOfFunctionCalled = 0;
    _functionParameters.clear();
    _functionCallReturnValue = -1;
    _temps.clear();
    _returnValueOfBindingExpression = -1;
    _synthesizedConsts = 0;

    // It would seem unlikely that function with some many basic blocks (after optimization)
    // consists merely of a qsTr call or a constant value return ;-)
    if (function->basicBlockCount() > 10)
        return false;

    for (QV4::IR::BasicBlock *bb : function->basicBlocks()) {
        for (QV4::IR::Stmt *s : bb->statements()) {
            visit(s);
            if (!_canSimplify)
                return false;
        }
    }

    if (_returnValueOfBindingExpression == -1)
        return false;

    if (_nameOfFunctionCalled) {
        if (_functionCallReturnValue != _returnValueOfBindingExpression)
            return false;
        return detectTranslationCallAndConvertBinding(binding);
    }

    return false;
}

bool QQmlJavaScriptBindingExpressionSimplificationPass::detectTranslationCallAndConvertBinding(QmlIR::Binding *binding)
{
    if (*_nameOfFunctionCalled == QLatin1String("qsTr")) {
        QString translation;
        QV4::CompiledData::TranslationData translationData;
        translationData.number = -1;
        translationData.commentIndex = 0; // empty string

        QVector<int>::ConstIterator param = _functionParameters.constBegin();
        QVector<int>::ConstIterator end = _functionParameters.constEnd();
        if (param == end)
            return false;

        QV4::IR::String *stringParam = _temps[*param]->asString();
        if (!stringParam)
            return false;

        translation = *stringParam->value;

        ++param;
        if (param != end) {
            stringParam = _temps[*param]->asString();
            if (!stringParam)
                return false;
            translationData.commentIndex = unitGenerator->registerString(*stringParam->value);
            ++param;

            if (param != end) {
                QV4::IR::Const *constParam = _temps[*param]->asConst();
                if (!constParam || constParam->type != QV4::IR::SInt32Type)
                    return false;

                translationData.number = int(constParam->value);
                ++param;
            }
        }

        if (param != end)
            return false;

        binding->type = QV4::CompiledData::Binding::Type_Translation;
        binding->stringIndex = unitGenerator->registerString(translation);
        binding->value.translationData = translationData;
        return true;
    } else if (*_nameOfFunctionCalled == QLatin1String("qsTrId")) {
        QString id;
        QV4::CompiledData::TranslationData translationData;
        translationData.number = -1;
        translationData.commentIndex = 0; // empty string, but unused

        QVector<int>::ConstIterator param = _functionParameters.constBegin();
        QVector<int>::ConstIterator end = _functionParameters.constEnd();
        if (param == end)
            return false;

        QV4::IR::String *stringParam = _temps[*param]->asString();
        if (!stringParam)
            return false;

        id = *stringParam->value;

        ++param;
        if (param != end) {
            QV4::IR::Const *constParam = _temps[*param]->asConst();
            if (!constParam || constParam->type != QV4::IR::SInt32Type)
                return false;

            translationData.number = int(constParam->value);
            ++param;
        }

        if (param != end)
            return false;

        binding->type = QV4::CompiledData::Binding::Type_TranslationById;
        binding->stringIndex = unitGenerator->registerString(id);
        binding->value.translationData = translationData;
        return true;
    } else if (*_nameOfFunctionCalled == QLatin1String("QT_TR_NOOP") || *_nameOfFunctionCalled == QLatin1String("QT_TRID_NOOP")) {
        QVector<int>::ConstIterator param = _functionParameters.constBegin();
        QVector<int>::ConstIterator end = _functionParameters.constEnd();
        if (param == end)
            return false;

        QV4::IR::String *stringParam = _temps[*param]->asString();
        if (!stringParam)
            return false;

        ++param;
        if (param != end)
            return false;

        binding->type = QV4::CompiledData::Binding::Type_String;
        binding->stringIndex = unitGenerator->registerString(*stringParam->value);
        return true;
    } else if (*_nameOfFunctionCalled == QLatin1String("QT_TRANSLATE_NOOP")) {
        QVector<int>::ConstIterator param = _functionParameters.constBegin();
        QVector<int>::ConstIterator end = _functionParameters.constEnd();
        if (param == end)
            return false;

        ++param;
        if (param == end)
            return false;

        QV4::IR::String *stringParam = _temps[*param]->asString();
        if (!stringParam)
            return false;

        ++param;
        if (param != end)
            return false;

        binding->type = QV4::CompiledData::Binding::Type_String;
        binding->stringIndex = unitGenerator->registerString(*stringParam->value);
        return true;
    }
    return false;
}

QQmlIRFunctionCleanser::QQmlIRFunctionCleanser(QV4::IR::Module *module, const QVector<QmlIR::Object *> &qmlObjects, const QVector<int> &functionsToRemove)
    : module(module)
    , qmlObjects(qmlObjects)
    , functionsToRemove(functionsToRemove)
{
}

void QQmlIRFunctionCleanser::clean()
{
    QVector<QV4::IR::Function*> newFunctions;
    newFunctions.reserve(module->functions.count() - functionsToRemove.count());

    newFunctionIndices.resize(module->functions.count());

    for (int i = 0; i < module->functions.count(); ++i) {
        QV4::IR::Function *f = module->functions.at(i);
        Q_ASSERT(f || functionsToRemove.contains(i));
        if (f) {
            newFunctionIndices[i] = newFunctions.count();
            newFunctions << f;
        }
    }

    module->functions = newFunctions;

    for (QV4::IR::Function *function : qAsConst(module->functions)) {
        for (QV4::IR::BasicBlock *block : function->basicBlocks()) {
            for (QV4::IR::Stmt *s : block->statements()) {
                visit(s);
            }
        }
    }

    for (QmlIR::Object *obj : qmlObjects) {
        for (int i = 0; i < obj->runtimeFunctionIndices.count; ++i)
            obj->runtimeFunctionIndices[i] = newFunctionIndices[obj->runtimeFunctionIndices.at(i)];
    }
}

void QQmlIRFunctionCleanser::visit(QV4::IR::Stmt *s)
{

    switch (s->stmtKind) {
    case QV4::IR::Stmt::PhiStmt:
        // nothing to do
        break;
    default:
        STMT_VISIT_ALL_KINDS(s);
        break;
    }
}

void QQmlIRFunctionCleanser::visit(QV4::IR::Expr *e)
{
    switch (e->exprKind) {
    case QV4::IR::Expr::ClosureExpr: {
        auto closure = e->asClosure();
        closure->value = newFunctionIndices.at(closure->value);
    } break;
    default:
        EXPR_VISIT_ALL_KINDS(e);
        break;
    }
}

QT_END_NAMESPACE
