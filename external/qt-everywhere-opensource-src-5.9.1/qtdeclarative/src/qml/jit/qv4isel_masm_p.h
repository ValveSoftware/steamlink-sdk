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
#ifndef QV4ISEL_MASM_P_H
#define QV4ISEL_MASM_P_H

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

#include "private/qv4global_p.h"
#include "private/qv4jsir_p.h"
#include "private/qv4isel_p.h"
#include "private/qv4isel_util_p.h"
#include "private/qv4util_p.h"
#include "private/qv4value_p.h"
#include "private/qv4lookup_p.h"

#include <QtCore/QHash>
#include <QtCore/QStack>
#include <config.h>
#include <wtf/Vector.h>

#include "qv4assembler_p.h"

#if ENABLE(ASSEMBLER)

QT_BEGIN_NAMESPACE

namespace QV4 {
namespace JIT {

template <typename JITAssembler = Assembler<DefaultAssemblerTargetConfiguration>>
class Q_QML_EXPORT InstructionSelection:
        protected IR::IRDecoder,
        public EvalInstructionSelection
{
public:
    InstructionSelection(QQmlEnginePrivate *qmlEngine, QV4::ExecutableAllocator *execAllocator, IR::Module *module, QV4::Compiler::JSUnitGenerator *jsGenerator, EvalISelFactory *iselFactory);
    ~InstructionSelection();

    void run(int functionIndex) override;

protected:
    QQmlRefPointer<QV4::CompiledData::CompilationUnit> backendCompileStep() override;

    void callBuiltinInvalid(IR::Name *func, IR::ExprList *args, IR::Expr *result) override;
    void callBuiltinTypeofQmlContextProperty(IR::Expr *base, IR::Member::MemberKind kind, int propertyIndex, IR::Expr *result) override;
    void callBuiltinTypeofMember(IR::Expr *base, const QString &name, IR::Expr *result) override;
    void callBuiltinTypeofSubscript(IR::Expr *base, IR::Expr *index, IR::Expr *result) override;
    void callBuiltinTypeofName(const QString &name, IR::Expr *result) override;
    void callBuiltinTypeofValue(IR::Expr *value, IR::Expr *result) override;
    void callBuiltinDeleteMember(IR::Expr *base, const QString &name, IR::Expr *result) override;
    void callBuiltinDeleteSubscript(IR::Expr *base, IR::Expr *index, IR::Expr *result) override;
    void callBuiltinDeleteName(const QString &name, IR::Expr *result) override;
    void callBuiltinDeleteValue(IR::Expr *result) override;
    void callBuiltinThrow(IR::Expr *arg) override;
    void callBuiltinReThrow() override;
    void callBuiltinUnwindException(IR::Expr *) override;
    void callBuiltinPushCatchScope(const QString &exceptionName) override;
    void callBuiltinForeachIteratorObject(IR::Expr *arg, IR::Expr *result) override;
    void callBuiltinForeachNextPropertyname(IR::Expr *arg, IR::Expr *result) override;
    void callBuiltinPushWithScope(IR::Expr *arg) override;
    void callBuiltinPopScope() override;
    void callBuiltinDeclareVar(bool deletable, const QString &name) override;
    void callBuiltinDefineArray(IR::Expr *result, IR::ExprList *args) override;
    void callBuiltinDefineObjectLiteral(IR::Expr *result, int keyValuePairCount, IR::ExprList *keyValuePairs, IR::ExprList *arrayEntries, bool needSparseArray) override;
    void callBuiltinSetupArgumentObject(IR::Expr *result) override;
    void callBuiltinConvertThisToObject() override;
    void callValue(IR::Expr *value, IR::ExprList *args, IR::Expr *result) override;
    void callQmlContextProperty(IR::Expr *base, IR::Member::MemberKind kind, int propertyIndex, IR::ExprList *args, IR::Expr *result) override;
    void callProperty(IR::Expr *base, const QString &name, IR::ExprList *args, IR::Expr *result) override;
    void callSubscript(IR::Expr *base, IR::Expr *index, IR::ExprList *args, IR::Expr *result) override;
    void convertType(IR::Expr *source, IR::Expr *target) override;
    void loadThisObject(IR::Expr *temp) override;
    void loadQmlContext(IR::Expr *target) override;
    void loadQmlImportedScripts(IR::Expr *target) override;
    void loadQmlSingleton(const QString &name, IR::Expr *target) override;
    void loadConst(IR::Const *sourceConst, IR::Expr *target) override;
    void loadString(const QString &str, IR::Expr *target) override;
    void loadRegexp(IR::RegExp *sourceRegexp, IR::Expr *target) override;
    void getActivationProperty(const IR::Name *name, IR::Expr *target) override;
    void setActivationProperty(IR::Expr *source, const QString &targetName) override;
    void initClosure(IR::Closure *closure, IR::Expr *target) override;
    void getProperty(IR::Expr *base, const QString &name, IR::Expr *target) override;
    void getQmlContextProperty(IR::Expr *source, IR::Member::MemberKind kind, int index, bool captureRequired, IR::Expr *target) override;
    void getQObjectProperty(IR::Expr *base, int propertyIndex, bool captureRequired, bool isSingleton, int attachedPropertiesId, IR::Expr *target) override;
    void setProperty(IR::Expr *source, IR::Expr *targetBase, const QString &targetName) override;
    void setQmlContextProperty(IR::Expr *source, IR::Expr *targetBase, IR::Member::MemberKind kind, int propertyIndex) override;
    void setQObjectProperty(IR::Expr *source, IR::Expr *targetBase, int propertyIndex) override;
    void getElement(IR::Expr *base, IR::Expr *index, IR::Expr *target) override;
    void setElement(IR::Expr *source, IR::Expr *targetBase, IR::Expr *targetIndex) override;
    void copyValue(IR::Expr *source, IR::Expr *target) override;
    void swapValues(IR::Expr *source, IR::Expr *target) override;
    void unop(IR::AluOp oper, IR::Expr *sourceTemp, IR::Expr *target) override;
    void binop(IR::AluOp oper, IR::Expr *leftSource, IR::Expr *rightSource, IR::Expr *target) override;

    using Address = typename JITAssembler::Address;
    using Pointer = typename JITAssembler::Pointer;
    using PointerToValue = typename JITAssembler::PointerToValue;
    using RegisterID = typename JITAssembler::RegisterID;
    using FPRegisterID = typename JITAssembler::FPRegisterID;
    using ResultCondition = typename JITAssembler::ResultCondition;
    using TrustedImm32 = typename JITAssembler::TrustedImm32;
    using TrustedImm64 = typename JITAssembler::TrustedImm64;
    using Label = typename JITAssembler::Label;
    using Jump = typename JITAssembler::Jump;
    using StringToIndex = typename JITAssembler::StringToIndex;
    using Reference = typename JITAssembler::Reference;
    using RelationalCondition = typename JITAssembler::RelationalCondition;
    using BranchTruncateType = typename JITAssembler::BranchTruncateType;
    using RuntimeCall = typename JITAssembler::RuntimeCall;

    using JITTargetPlatform = typename JITAssembler::JITTargetPlatform;

    Address addressForArgument(int index) const
    {
        // FramePointerRegister points to its old value on the stack, and above
        // it we have the return address, hence the need to step over two
        // values before reaching the first argument.
        return Address(JITTargetPlatform::FramePointerRegister, (index + 2) * JITTargetPlatform::RegisterSize);
    }

    Pointer baseAddressForCallArguments()
    {
        return _as->stackLayout().argumentAddressForCall(0);
    }

    Pointer baseAddressForCallData()
    {
        return _as->stackLayout().callDataAddress();
    }

    void constructActivationProperty(IR::Name *func, IR::ExprList *args, IR::Expr *result) override;
    void constructProperty(IR::Expr *base, const QString &name, IR::ExprList *args, IR::Expr*result) override;
    void constructValue(IR::Expr *value, IR::ExprList *args, IR::Expr *result) override;

    void visitJump(IR::Jump *) override;
    void visitCJump(IR::CJump *) override;
    void visitRet(IR::Ret *) override;

    bool visitCJumpDouble(IR::AluOp op, IR::Expr *left, IR::Expr *right,
                          IR::BasicBlock *iftrue, IR::BasicBlock *iffalse);
    bool visitCJumpSInt32(IR::AluOp op, IR::Expr *left, IR::Expr *right,
                          IR::BasicBlock *iftrue, IR::BasicBlock *iffalse);
    void visitCJumpStrict(IR::Binop *binop, IR::BasicBlock *trueBlock, IR::BasicBlock *falseBlock);
    bool visitCJumpStrictNull(IR::Binop *binop, IR::BasicBlock *trueBlock, IR::BasicBlock *falseBlock);
    bool visitCJumpStrictUndefined(IR::Binop *binop, IR::BasicBlock *trueBlock, IR::BasicBlock *falseBlock);
    bool visitCJumpStrictBool(IR::Binop *binop, IR::BasicBlock *trueBlock, IR::BasicBlock *falseBlock);
    bool visitCJumpNullUndefined(IR::Type nullOrUndef, IR::Binop *binop,
                                 IR::BasicBlock *trueBlock, IR::BasicBlock *falseBlock);
    void visitCJumpEqual(IR::Binop *binop, IR::BasicBlock *trueBlock, IR::BasicBlock *falseBlock);

private:
    void convertTypeSlowPath(IR::Expr *source, IR::Expr *target);
    void convertTypeToDouble(IR::Expr *source, IR::Expr *target);
    void convertTypeToBool(IR::Expr *source, IR::Expr *target);
    void convertTypeToSInt32(IR::Expr *source, IR::Expr *target);
    void convertTypeToUInt32(IR::Expr *source, IR::Expr *target);

    void convertIntToDouble(IR::Expr *source, IR::Expr *target)
    {
        if (IR::Temp *targetTemp = target->asTemp()) {
            if (targetTemp->kind == IR::Temp::PhysicalRegister) {
                if (IR::Temp *sourceTemp = source->asTemp()) {
                    if (sourceTemp->kind == IR::Temp::PhysicalRegister) {
                        _as->convertInt32ToDouble((RegisterID) sourceTemp->index,
                                                  (FPRegisterID) targetTemp->index);
                    } else {
                        _as->convertInt32ToDouble(_as->loadAddress(JITTargetPlatform::ReturnValueRegister, sourceTemp),
                                                  (FPRegisterID) targetTemp->index);
                    }
                } else {
                    _as->convertInt32ToDouble(_as->toInt32Register(source, JITTargetPlatform::ScratchRegister),
                                              (FPRegisterID) targetTemp->index);
                }

                return;
            }
        }

        _as->convertInt32ToDouble(_as->toInt32Register(source, JITTargetPlatform::ScratchRegister),
                                  JITTargetPlatform::FPGpr0);
        _as->storeDouble(JITTargetPlatform::FPGpr0, _as->loadAddress(JITTargetPlatform::ReturnValueRegister, target));
    }

    void convertUIntToDouble(IR::Expr *source, IR::Expr *target)
    {
        RegisterID tmpReg = JITTargetPlatform::ScratchRegister;
        RegisterID reg = _as->toInt32Register(source, tmpReg);

        if (IR::Temp *targetTemp = target->asTemp()) {
            if (targetTemp->kind == IR::Temp::PhysicalRegister) {
                _as->convertUInt32ToDouble(reg, (FPRegisterID) targetTemp->index, tmpReg);
                return;
            }
        }

        _as->convertUInt32ToDouble(_as->toUInt32Register(source, tmpReg),
                                   JITTargetPlatform::FPGpr0, tmpReg);
        _as->storeDouble(JITTargetPlatform::FPGpr0, _as->loadAddress(tmpReg, target));
    }

    void convertIntToBool(IR::Expr *source, IR::Expr *target)
    {
        RegisterID reg = JITTargetPlatform::ScratchRegister;

        if (IR::Temp *targetTemp = target->asTemp())
            if (targetTemp->kind == IR::Temp::PhysicalRegister)
                reg = (RegisterID) targetTemp->index;
        _as->move(_as->toInt32Register(source, reg), reg);
        _as->compare32(RelationalCondition::NotEqual, reg, TrustedImm32(0), reg);
        _as->storeBool(reg, target);
    }

    int prepareVariableArguments(IR::ExprList* args);
    int prepareCallData(IR::ExprList* args, IR::Expr *thisObject);

    void calculateRegistersToSave(const RegisterInformation &used);

    template <typename Retval, typename Arg1, typename Arg2, typename Arg3>
    void generateLookupCall(Retval retval, uint index, uint getterSetterOffset, Arg1 arg1, Arg2 arg2, Arg3 arg3)
    {
        // Note: using the return value register is intentional: for ABIs where the first parameter
        // goes into the same register as the return value (currently only ARM), the prepareCall
        // will combine loading the looupAddr into the register and calculating the indirect call
        // address.
        Pointer lookupAddr(JITTargetPlatform::ReturnValueRegister, index * sizeof(QV4::Lookup));

         _as->generateFunctionCallImp(true, retval, "lookup getter/setter",
                                      typename JITAssembler::LookupCall(lookupAddr, getterSetterOffset), lookupAddr,
                                      arg1, arg2, arg3);
    }

    template <typename Retval, typename Arg1, typename Arg2>
    void generateLookupCall(Retval retval, uint index, uint getterSetterOffset, Arg1 arg1, Arg2 arg2)
    {
        generateLookupCall(retval, index, getterSetterOffset, arg1, arg2, typename JITAssembler::VoidType());
    }

    IR::BasicBlock *_block;
    BitVector _removableJumps;
    JITAssembler* _as;

    QScopedPointer<CompilationUnit> compilationUnit;
    QQmlEnginePrivate *qmlEngine;
    RegisterInformation regularRegistersToSave;
    RegisterInformation fpRegistersToSave;
};

template <typename JITAssembler = Assembler<DefaultAssemblerTargetConfiguration>>
class Q_QML_EXPORT ISelFactory: public EvalISelFactory
{
public:
    ISelFactory() : EvalISelFactory(QStringLiteral("jit")) {}
    virtual ~ISelFactory() {}
    EvalInstructionSelection *create(QQmlEnginePrivate *qmlEngine, QV4::ExecutableAllocator *execAllocator, IR::Module *module, QV4::Compiler::JSUnitGenerator *jsGenerator) Q_DECL_OVERRIDE Q_DECL_FINAL
    { return new InstructionSelection<JITAssembler>(qmlEngine, execAllocator, module, jsGenerator, this); }
    bool jitCompileRegexps() const Q_DECL_OVERRIDE Q_DECL_FINAL
    { return true; }
    QQmlRefPointer<CompiledData::CompilationUnit> createUnitForLoading() Q_DECL_OVERRIDE Q_DECL_FINAL;
};

} // end of namespace JIT
} // end of namespace QV4

QT_END_NAMESPACE

#endif // ENABLE(ASSEMBLER)

#endif // QV4ISEL_MASM_P_H
