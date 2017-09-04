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

#ifndef QV4ISEL_MOTH_P_H
#define QV4ISEL_MOTH_P_H

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
#include <private/qv4isel_p.h>
#include <private/qv4isel_util_p.h>
#include <private/qv4util_p.h>
#include <private/qv4jsir_p.h>
#include <private/qv4value_p.h>
#include "qv4instr_moth_p.h"

#if !defined(V4_BOOTSTRAP)
QT_REQUIRE_CONFIG(qml_interpreter);
#endif

QT_BEGIN_NAMESPACE

namespace QV4 {
namespace Moth {

struct CompilationUnit : public QV4::CompiledData::CompilationUnit
{
    virtual ~CompilationUnit();
#if !defined(V4_BOOTSTRAP)
    void linkBackendToEngine(QV4::ExecutionEngine *engine) Q_DECL_OVERRIDE;
    bool memoryMapCode(QString *errorString) Q_DECL_OVERRIDE;
#endif
    void prepareCodeOffsetsForDiskStorage(CompiledData::Unit *unit) Q_DECL_OVERRIDE;
    bool saveCodeToDisk(QIODevice *device, const CompiledData::Unit *unit, QString *errorString) Q_DECL_OVERRIDE;

    QVector<QByteArray> codeRefs;

};

class Q_QML_EXPORT InstructionSelection:
        public IR::IRDecoder,
        public EvalInstructionSelection
{
public:
    InstructionSelection(QQmlEnginePrivate *qmlEngine, QV4::ExecutableAllocator *execAllocator, IR::Module *module, QV4::Compiler::JSUnitGenerator *jsGenerator, EvalISelFactory *iselFactory);
    ~InstructionSelection();

    void run(int functionIndex) override;

protected:
    QQmlRefPointer<CompiledData::CompilationUnit> backendCompileStep() override;

    void visitJump(IR::Jump *) override;
    void visitCJump(IR::CJump *) override;
    void visitRet(IR::Ret *) override;

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
    void constructActivationProperty(IR::Name *func, IR::ExprList *args, IR::Expr *result) override;
    void constructProperty(IR::Expr *base, const QString &name, IR::ExprList *args, IR::Expr *result) override;
    void constructValue(IR::Expr *value, IR::ExprList *args, IR::Expr *result) override;
    void loadThisObject(IR::Expr *e) override;
    void loadQmlContext(IR::Expr *e) override;
    void loadQmlImportedScripts(IR::Expr *e) override;
    void loadQmlSingleton(const QString &name, IR::Expr *e) override;
    void loadConst(IR::Const *sourceConst, IR::Expr *e) override;
    void loadString(const QString &str, IR::Expr *target) override;
    void loadRegexp(IR::RegExp *sourceRegexp, IR::Expr *target) override;
    void getActivationProperty(const IR::Name *name, IR::Expr *target) override;
    void setActivationProperty(IR::Expr *source, const QString &targetName) override;
    void initClosure(IR::Closure *closure, IR::Expr *target) override;
    void getProperty(IR::Expr *base, const QString &name, IR::Expr *target) override;
    void setProperty(IR::Expr *source, IR::Expr *targetBase, const QString &targetName) override;
    void setQmlContextProperty(IR::Expr *source, IR::Expr *targetBase, IR::Member::MemberKind kind, int propertyIndex) override;
    void setQObjectProperty(IR::Expr *source, IR::Expr *targetBase, int propertyIndex) override;
    void getQmlContextProperty(IR::Expr *source, IR::Member::MemberKind kind, int index, bool captureRequired, IR::Expr *target) override;
    void getQObjectProperty(IR::Expr *base, int propertyIndex, bool captureRequired, bool isSingleton, int attachedPropertiesId, IR::Expr *target) override;
    void getElement(IR::Expr *base, IR::Expr *index, IR::Expr *target) override;
    void setElement(IR::Expr *source, IR::Expr *targetBase, IR::Expr *targetIndex) override;
    void copyValue(IR::Expr *source, IR::Expr *target) override;
    void swapValues(IR::Expr *source, IR::Expr *target) override;
    void unop(IR::AluOp oper, IR::Expr *source, IR::Expr *target) override;
    void binop(IR::AluOp oper, IR::Expr *leftSource, IR::Expr *rightSource, IR::Expr *target) override;

private:
    Param binopHelper(IR::AluOp oper, IR::Expr *leftSource, IR::Expr *rightSource, IR::Expr *target);

    struct Instruction {
#define MOTH_INSTR_DATA_TYPEDEF(I, FMT) typedef InstrData<Instr::I> I;
    FOR_EACH_MOTH_INSTR(MOTH_INSTR_DATA_TYPEDEF)
#undef MOTH_INSTR_DATA_TYPEDEF
    private:
        Instruction();
    };

    Param getParam(IR::Expr *e);

    Param getResultParam(IR::Expr *result)
    {
        if (result)
            return getParam(result);
        else
            return Param::createTemp(scratchTempIndex());
    }

    void simpleMove(IR::Move *);
    void prepareCallArgs(IR::ExprList *, quint32 &, quint32 * = 0);

    int scratchTempIndex() const { return _function->tempCount; }
    int callDataStart() const { return scratchTempIndex() + 1; }
    int outgoingArgumentTempStart() const { return callDataStart() + offsetof(QV4::CallData, args)/sizeof(QV4::Value); }
    int frameSize() const { return outgoingArgumentTempStart() + _function->maxNumberOfArguments; }

    template <int Instr>
    inline ptrdiff_t addInstruction(const InstrData<Instr> &data);
    inline void addDebugInstruction();

    ptrdiff_t addInstructionHelper(Instr::Type type, Instr &instr);
    void patchJumpAddresses();
    QByteArray squeezeCode() const;

    QQmlEnginePrivate *qmlEngine;

    bool blockNeedsDebugInstruction;
    uint currentLine;
    IR::BasicBlock *_block;
    IR::BasicBlock *_nextBlock;

    QHash<IR::BasicBlock *, QVector<ptrdiff_t> > _patches;
    QHash<IR::BasicBlock *, ptrdiff_t> _addrs;

    uchar *_codeStart;
    uchar *_codeNext;
    uchar *_codeEnd;

    BitVector _removableJumps;
    IR::Stmt *_currentStatement;

    QScopedPointer<CompilationUnit> compilationUnit;
    QHash<IR::Function *, QByteArray> codeRefs;
};

class Q_QML_EXPORT ISelFactory: public EvalISelFactory
{
public:
    ISelFactory() : EvalISelFactory(QStringLiteral("moth")) {}
    virtual ~ISelFactory() {}
    EvalInstructionSelection *create(QQmlEnginePrivate *qmlEngine, QV4::ExecutableAllocator *execAllocator, IR::Module *module, QV4::Compiler::JSUnitGenerator *jsGenerator) Q_DECL_OVERRIDE Q_DECL_FINAL
    { return new InstructionSelection(qmlEngine, execAllocator, module, jsGenerator, this); }
    bool jitCompileRegexps() const Q_DECL_OVERRIDE Q_DECL_FINAL
    { return false; }
    QQmlRefPointer<QV4::CompiledData::CompilationUnit> createUnitForLoading() Q_DECL_OVERRIDE;

};

template<int InstrT>
ptrdiff_t InstructionSelection::addInstruction(const InstrData<InstrT> &data)
{
    Instr genericInstr;
    InstrMeta<InstrT>::setDataNoCommon(genericInstr, data);
    return addInstructionHelper(static_cast<Instr::Type>(InstrT), genericInstr);
}

} // namespace Moth
} // namespace QV4

QT_END_NAMESPACE

#endif // QV4ISEL_MOTH_P_H
