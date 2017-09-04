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

#include <QtCore/QDebug>
#include <QtCore/QBuffer>
#include "qv4jsir_p.h"
#include "qv4isel_p.h"
#include "qv4isel_util_p.h"
#include <private/qv4value_p.h>
#ifndef V4_BOOTSTRAP
#include <private/qqmlpropertycache_p.h>
#endif

#include <QString>

using namespace QV4;
using namespace QV4::IR;

EvalInstructionSelection::EvalInstructionSelection(QV4::ExecutableAllocator *execAllocator, Module *module, QV4::Compiler::JSUnitGenerator *jsGenerator, EvalISelFactory *iselFactory)
    : useFastLookups(true)
    , useTypeInference(true)
    , executableAllocator(execAllocator)
    , irModule(module)
{
    if (!jsGenerator) {
        jsGenerator = new QV4::Compiler::JSUnitGenerator(module);
        ownJSGenerator.reset(jsGenerator);
    }
    this->jsGenerator = jsGenerator;
#ifndef V4_BOOTSTRAP
    Q_ASSERT(execAllocator);
#endif
    Q_ASSERT(module);
    jsGenerator->codeGeneratorName = iselFactory->codeGeneratorName;
}

EvalInstructionSelection::~EvalInstructionSelection()
{}

EvalISelFactory::~EvalISelFactory()
{}

QQmlRefPointer<CompiledData::CompilationUnit> EvalInstructionSelection::compile(bool generateUnitData)
{
    for (int i = 0; i < irModule->functions.size(); ++i)
        run(i);

    QQmlRefPointer<QV4::CompiledData::CompilationUnit> unit = backendCompileStep();
    if (generateUnitData)
        unit->data = jsGenerator->generateUnit();
    return unit;
}

void IRDecoder::visitMove(IR::Move *s)
{
    if (IR::Name *n = s->target->asName()) {
        if (s->source->asTemp() || s->source->asConst() || s->source->asArgLocal()) {
            setActivationProperty(s->source, *n->id);
            return;
        }
    } else if (s->target->asTemp() || s->target->asArgLocal()) {
        if (IR::Name *n = s->source->asName()) {
            if (n->id && *n->id == QLatin1String("this")) // TODO: `this' should be a builtin.
                loadThisObject(s->target);
            else if (n->builtin == IR::Name::builtin_qml_context)
                loadQmlContext(s->target);
            else if (n->builtin == IR::Name::builtin_qml_imported_scripts_object)
                loadQmlImportedScripts(s->target);
            else if (n->qmlSingleton)
                loadQmlSingleton(*n->id, s->target);
            else
                getActivationProperty(n, s->target);
            return;
        } else if (IR::Const *c = s->source->asConst()) {
            loadConst(c, s->target);
            return;
        } else if (s->source->asTemp() || s->source->asArgLocal()) {
            if (s->swap)
                swapValues(s->source, s->target);
            else
                copyValue(s->source, s->target);
            return;
        } else if (IR::String *str = s->source->asString()) {
            loadString(*str->value, s->target);
            return;
        } else if (IR::RegExp *re = s->source->asRegExp()) {
            loadRegexp(re, s->target);
            return;
        } else if (IR::Closure *clos = s->source->asClosure()) {
            initClosure(clos, s->target);
            return;
        } else if (IR::New *ctor = s->source->asNew()) {
            if (Name *func = ctor->base->asName()) {
                constructActivationProperty(func, ctor->args, s->target);
                return;
            } else if (IR::Member *member = ctor->base->asMember()) {
                constructProperty(member->base, *member->name, ctor->args, s->target);
                return;
            } else if (ctor->base->asTemp() || ctor->base->asArgLocal()) {
                constructValue(ctor->base, ctor->args, s->target);
                return;
            }
        } else if (IR::Member *m = s->source->asMember()) {
            if (m->property) {
#ifdef V4_BOOTSTRAP
                Q_UNIMPLEMENTED();
#else
                bool captureRequired = true;

                Q_ASSERT(m->kind != IR::Member::MemberOfEnum && m->kind != IR::Member::MemberOfIdObjectsArray);
                const int attachedPropertiesId = m->attachedPropertiesId;
                const bool isSingletonProperty = m->kind == IR::Member::MemberOfSingletonObject;

                if (_function && attachedPropertiesId == 0 && !m->property->isConstant() && _function->isQmlBinding) {
                    if (m->kind == IR::Member::MemberOfQmlContextObject) {
                        _function->contextObjectPropertyDependencies.insert(m->property->coreIndex(), m->property->notifyIndex());
                        captureRequired = false;
                    } else if (m->kind == IR::Member::MemberOfQmlScopeObject) {
                        _function->scopeObjectPropertyDependencies.insert(m->property->coreIndex(), m->property->notifyIndex());
                        captureRequired = false;
                    }
                }
                if (m->kind == IR::Member::MemberOfQmlScopeObject || m->kind == IR::Member::MemberOfQmlContextObject) {
                    getQmlContextProperty(m->base, (IR::Member::MemberKind)m->kind, m->property->coreIndex(), captureRequired, s->target);
                    return;
                }
                getQObjectProperty(m->base, m->property->coreIndex(), captureRequired, isSingletonProperty, attachedPropertiesId, s->target);
#endif // V4_BOOTSTRAP
                return;
            } else if (m->kind == IR::Member::MemberOfIdObjectsArray) {
                getQmlContextProperty(m->base, (IR::Member::MemberKind)m->kind, m->idIndex, /*captureRequired*/false, s->target);
                return;
            } else if (m->base->asTemp() || m->base->asConst() || m->base->asArgLocal()) {
                getProperty(m->base, *m->name, s->target);
                return;
            }
        } else if (IR::Subscript *ss = s->source->asSubscript()) {
            getElement(ss->base, ss->index, s->target);
            return;
        } else if (IR::Unop *u = s->source->asUnop()) {
            unop(u->op, u->expr, s->target);
            return;
        } else if (IR::Binop *b = s->source->asBinop()) {
            binop(b->op, b->left, b->right, s->target);
            return;
        } else if (IR::Call *c = s->source->asCall()) {
            if (c->base->asName()) {
                callBuiltin(c, s->target);
                return;
            } else if (Member *member = c->base->asMember()) {
#ifndef V4_BOOTSTRAP
                Q_ASSERT(member->kind != IR::Member::MemberOfIdObjectsArray);
                if (member->kind == IR::Member::MemberOfQmlScopeObject || member->kind == IR::Member::MemberOfQmlContextObject) {
                    callQmlContextProperty(member->base, (IR::Member::MemberKind)member->kind, member->property->coreIndex(), c->args, s->target);
                    return;
                }
#endif
                callProperty(member->base, *member->name, c->args, s->target);
                return;
            } else if (Subscript *ss = c->base->asSubscript()) {
                callSubscript(ss->base, ss->index, c->args, s->target);
                return;
            } else if (c->base->asTemp() || c->base->asArgLocal() || c->base->asConst()) {
                callValue(c->base, c->args, s->target);
                return;
            }
        } else if (IR::Convert *c = s->source->asConvert()) {
            Q_ASSERT(c->expr->asTemp() || c->expr->asArgLocal());
            convertType(c->expr, s->target);
            return;
        }
    } else if (IR::Member *m = s->target->asMember()) {
        if (m->base->asTemp() || m->base->asConst() || m->base->asArgLocal()) {
            if (s->source->asTemp() || s->source->asConst() || s->source->asArgLocal()) {
                Q_ASSERT(m->kind != IR::Member::MemberOfEnum);
                Q_ASSERT(m->kind != IR::Member::MemberOfIdObjectsArray);
                const int attachedPropertiesId = m->attachedPropertiesId;
                if (m->property && attachedPropertiesId == 0) {
#ifdef V4_BOOTSTRAP
                    Q_UNIMPLEMENTED();
#else
                    if (m->kind == IR::Member::MemberOfQmlScopeObject || m->kind == IR::Member::MemberOfQmlContextObject) {
                        setQmlContextProperty(s->source, m->base, (IR::Member::MemberKind)m->kind, m->property->coreIndex());
                        return;
                    }
                    setQObjectProperty(s->source, m->base, m->property->coreIndex());
#endif
                    return;
                } else {
                    setProperty(s->source, m->base, *m->name);
                    return;
                }
            }
        }
    } else if (IR::Subscript *ss = s->target->asSubscript()) {
        if (s->source->asTemp() || s->source->asConst() || s->source->asArgLocal()) {
            setElement(s->source, ss->base, ss->index);
            return;
        }
    }

    // For anything else...:
    Q_UNIMPLEMENTED();
    QBuffer buf;
    buf.open(QIODevice::WriteOnly);
    QTextStream qout(&buf);
    IRPrinter(&qout).print(s);
    qout << endl;
    qDebug("%s", buf.data().constData());
    Q_ASSERT(!"TODO");
}

IRDecoder::~IRDecoder()
{
}

void IRDecoder::visitExp(IR::Exp *s)
{
    if (IR::Call *c = s->expr->asCall()) {
        // These are calls where the result is ignored.
        if (c->base->asName()) {
            callBuiltin(c, 0);
        } else if (c->base->asTemp() || c->base->asArgLocal() || c->base->asConst()) {
            callValue(c->base, c->args, 0);
        } else if (Member *member = c->base->asMember()) {
            Q_ASSERT(member->base->asTemp() || member->base->asArgLocal());
#ifndef V4_BOOTSTRAP
            Q_ASSERT(member->kind != IR::Member::MemberOfIdObjectsArray);
            if (member->kind == IR::Member::MemberOfQmlScopeObject || member->kind == IR::Member::MemberOfQmlContextObject) {
                callQmlContextProperty(member->base, (IR::Member::MemberKind)member->kind, member->property->coreIndex(), c->args, 0);
                return;
            }
#endif
            callProperty(member->base, *member->name, c->args, 0);
        } else if (Subscript *s = c->base->asSubscript()) {
            callSubscript(s->base, s->index, c->args, 0);
        } else {
            Q_UNREACHABLE();
        }
    } else {
        Q_UNREACHABLE();
    }
}

void IRDecoder::callBuiltin(IR::Call *call, Expr *result)
{
    IR::Name *baseName = call->base->asName();
    Q_ASSERT(baseName != 0);

    switch (baseName->builtin) {
    case IR::Name::builtin_invalid:
        callBuiltinInvalid(baseName, call->args, result);
        return;

    case IR::Name::builtin_typeof: {
        if (IR::Member *member = call->args->expr->asMember()) {
#ifndef V4_BOOTSTRAP
            Q_ASSERT(member->kind != IR::Member::MemberOfIdObjectsArray);
            if (member->kind == IR::Member::MemberOfQmlScopeObject || member->kind == IR::Member::MemberOfQmlContextObject) {
                callBuiltinTypeofQmlContextProperty(member->base,
                                                    IR::Member::MemberKind(member->kind),
                                                    member->property->coreIndex(), result);
                return;
            }
#endif
            callBuiltinTypeofMember(member->base, *member->name, result);
            return;
        } else if (IR::Subscript *ss = call->args->expr->asSubscript()) {
            callBuiltinTypeofSubscript(ss->base, ss->index, result);
            return;
        } else if (IR::Name *n = call->args->expr->asName()) {
            callBuiltinTypeofName(*n->id, result);
            return;
        } else if (call->args->expr->asTemp() ||
                   call->args->expr->asConst() ||
                   call->args->expr->asArgLocal()) {
            callBuiltinTypeofValue(call->args->expr, result);
            return;
        }
    } break;

    case IR::Name::builtin_delete: {
        if (IR::Member *m = call->args->expr->asMember()) {
            callBuiltinDeleteMember(m->base, *m->name, result);
            return;
        } else if (IR::Subscript *ss = call->args->expr->asSubscript()) {
            callBuiltinDeleteSubscript(ss->base, ss->index, result);
            return;
        } else if (IR::Name *n = call->args->expr->asName()) {
            callBuiltinDeleteName(*n->id, result);
            return;
        } else if (call->args->expr->asTemp() ||
                   call->args->expr->asArgLocal()) {
            // TODO: should throw in strict mode
            callBuiltinDeleteValue(result);
            return;
        }
    } break;

    case IR::Name::builtin_throw: {
        IR::Expr *arg = call->args->expr;
        Q_ASSERT(arg->asTemp() || arg->asConst() || arg->asArgLocal());
        callBuiltinThrow(arg);
    } return;

    case IR::Name::builtin_rethrow: {
        callBuiltinReThrow();
    } return;

    case IR::Name::builtin_unwind_exception: {
        callBuiltinUnwindException(result);
    } return;

    case IR::Name::builtin_push_catch_scope: {
        IR::String *s = call->args->expr->asString();
        Q_ASSERT(s);
        callBuiltinPushCatchScope(*s->value);
    } return;

    case IR::Name::builtin_foreach_iterator_object: {
        IR::Expr *arg = call->args->expr;
        Q_ASSERT(arg != 0);
        callBuiltinForeachIteratorObject(arg, result);
    } return;

    case IR::Name::builtin_foreach_next_property_name: {
        IR::Expr *arg = call->args->expr;
        Q_ASSERT(arg != 0);
        callBuiltinForeachNextPropertyname(arg, result);
    } return;
    case IR::Name::builtin_push_with_scope: {
        if (call->args->expr->asTemp() || call->args->expr->asArgLocal())
            callBuiltinPushWithScope(call->args->expr);
        else
            Q_UNIMPLEMENTED();
    } return;

    case IR::Name::builtin_pop_scope:
        callBuiltinPopScope();
        return;

    case IR::Name::builtin_declare_vars: {
        if (!call->args)
            return;
        IR::Const *deletable = call->args->expr->asConst();
        Q_ASSERT(deletable->type == IR::BoolType);
        for (IR::ExprList *it = call->args->next; it; it = it->next) {
            IR::Name *arg = it->expr->asName();
            Q_ASSERT(arg != 0);
            callBuiltinDeclareVar(deletable->value != 0, *arg->id);
        }
    } return;

    case IR::Name::builtin_define_array:
        callBuiltinDefineArray(result, call->args);
        return;

    case IR::Name::builtin_define_object_literal: {
        IR::ExprList *args = call->args;
        const int keyValuePairsCount = args->expr->asConst()->value;
        args = args->next;

        IR::ExprList *keyValuePairs = args;
        for (int i = 0; i < keyValuePairsCount; ++i) {
            args = args->next; // name
            bool isData = args->expr->asConst()->value;
            args = args->next; // isData flag
            args = args->next; // value or getter
            if (!isData)
                args = args->next; // setter
        }

        IR::ExprList *arrayEntries = args;
        bool needSparseArray = false;
        for (IR::ExprList *it = arrayEntries; it; it = it->next) {
            uint index = it->expr->asConst()->value;
            if (index > 16)  {
                needSparseArray = true;
                break;
            }
            it = it->next;
            bool isData = it->expr->asConst()->value;
            it = it->next;
            if (!isData)
                it = it->next;
        }

        callBuiltinDefineObjectLiteral(result, keyValuePairsCount, keyValuePairs, arrayEntries, needSparseArray);
    } return;

    case IR::Name::builtin_setup_argument_object:
        callBuiltinSetupArgumentObject(result);
        return;

    case IR::Name::builtin_convert_this_to_object:
        callBuiltinConvertThisToObject();
        return;

    default:
        break;
    }

    Q_UNIMPLEMENTED();
    QBuffer buf;
    buf.open(QIODevice::WriteOnly);
    QTextStream qout(&buf);
    IRPrinter(&qout).print(call); qout << endl;
    qDebug("%s", buf.data().constData());
    Q_UNREACHABLE();
}
