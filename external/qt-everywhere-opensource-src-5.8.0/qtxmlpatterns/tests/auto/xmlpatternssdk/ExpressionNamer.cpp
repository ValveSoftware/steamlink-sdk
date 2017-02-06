/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QtDebug>

#include <private/qabstractfloat_p.h>
#include <private/qandexpression_p.h>
#include <private/qanyuri_p.h>
#include <private/qapplytemplate_p.h>
#include <private/qargumentreference_p.h>
#include <private/qarithmeticexpression_p.h>
#include <private/qatomicstring_p.h>
#include <private/qatomizer_p.h>
#include <private/qattributeconstructor_p.h>
#include <private/qattributenamevalidator_p.h>
#include <private/qaxisstep_p.h>
#include <private/qbase64binary_p.h>
#include <private/qboolean_p.h>
#include <private/qcardinalityverifier_p.h>
#include <private/qcastableas_p.h>
#include <private/qcastas_p.h>
#include <private/qcombinenodes_p.h>
#include <private/qcontextitem_p.h>
#include <private/qdate_p.h>
#include <private/qdecimal_p.h>
#include <private/qdynamiccontextstore_p.h>
#include <private/qelementconstructor_p.h>
#include <private/qemptysequence_p.h>
#include <private/qevaluationcache_p.h>
#include <private/qexpressionsequence_p.h>
#include <private/qexpressionvariablereference_p.h>
#include <private/qfirstitempredicate_p.h>
#include <private/qforclause_p.h>
#include <private/qfunctioncall_p.h>
#include <private/qgday_p.h>
#include <private/qgeneralcomparison_p.h>
#include <private/qgenericpredicate_p.h>
#include <private/qgmonthday_p.h>
#include <private/qgmonth_p.h>
#include <private/qgyearmonth_p.h>
#include <private/qgyear_p.h>
#include <private/qhexbinary_p.h>
#include <private/qifthenclause_p.h>
#include <private/qinstanceof_p.h>
#include <private/qinteger_p.h>
#include <private/qitem_p.h>
#include <private/qitemverifier_p.h>
#include <private/qliteral_p.h>
#include <private/qnamespaceconstructor_p.h>
#include <private/qncnameconstructor_p.h>
#include <private/qnodecomparison_p.h>
#include <private/qorexpression_p.h>
#include <private/qpath_p.h>
#include <private/qpositionalvariablereference_p.h>
#include <private/qqnameconstructor_p.h>
#include <private/qqnamevalue_p.h>
#include <private/qquantifiedexpression_p.h>
#include <private/qrangeexpression_p.h>
#include <private/qrangevariablereference_p.h>
#include <private/qschemadatetime_p.h>
#include <private/qschematime_p.h>
#include <private/qsimplecontentconstructor_p.h>
#include <private/qtreatas_p.h>
#include <private/qtruthpredicate_p.h>
#include <private/quntypedatomicconverter_p.h>
#include <private/quntypedatomic_p.h>
#include <private/quserfunctioncallsite_p.h>
#include <private/qvalidationerror_p.h>
#include <private/qvaluecomparison_p.h>

#include "ExpressionInfo.h"
#include "Global.h"

#include "ExpressionNamer.h"

using namespace QPatternistSDK;

/* Simple ones, they have no additional data. */
#define implClass(cls)                                                                              \
QPatternist::ExpressionVisitorResult::Ptr ExpressionNamer::visit(const QPatternist::cls *) const    \
{                                                                                                   \
    return QPatternist::ExpressionVisitorResult::Ptr                                                \
           (new ExpressionInfo(QLatin1String(#cls), QString()));                                    \
}

implClass(AndExpression)
implClass(ArgumentConverter)
implClass(Atomizer)
implClass(AttributeConstructor)
implClass(AttributeNameValidator)
implClass(CallTemplate)
implClass(CardinalityVerifier)
implClass(CollationChecker)
implClass(CommentConstructor)
implClass(ComputedNamespaceConstructor)
implClass(ContextItem)
implClass(CopyOf)
implClass(CurrentItemStore)
implClass(DocumentConstructor)
implClass(DynamicContextStore)
implClass(EBVExtractor)
implClass(ElementConstructor)
implClass(EmptySequence)
implClass(ExpressionSequence)
implClass(ExternalVariableReference)
implClass(FirstItemPredicate)
implClass(ForClause)
implClass(GenericPredicate)
implClass(IfThenClause)
implClass(ItemVerifier)
implClass(LetClause)
implClass(LiteralSequence)
implClass(NCNameConstructor)
implClass(NodeSortExpression)
implClass(OrderBy)
implClass(OrExpression)
implClass(ParentNodeAxis)
implClass(ProcessingInstructionConstructor)
implClass(QNameConstructor)
implClass(RangeExpression)
implClass(ReturnOrderBy)
implClass(SimpleContentConstructor)
implClass(StaticBaseURIStore)
implClass(StaticCompatibilityStore)
implClass(TemplateParameterReference)
implClass(TextNodeConstructor)
implClass(TreatAs)
implClass(TruthPredicate)
implClass(UnresolvedVariableReference)
implClass(UntypedAtomicConverter)
implClass(UserFunctionCallsite)
implClass(ValidationError)
#undef implClass

/** Variable references. */
#define implVarRef(name)                                                                            \
QPatternist::ExpressionVisitorResult::Ptr ExpressionNamer::visit(const QPatternist::name *i) const    \
{                                                                                                   \
    return QPatternist::ExpressionVisitorResult::Ptr                                                 \
           (new ExpressionInfo(QLatin1String(#name),                                                \
                               QString(QLatin1String("Slot: %1")).arg(i->slot())));                 \
}
implVarRef(RangeVariableReference)
implVarRef(ArgumentReference)
implVarRef(ExpressionVariableReference)
implVarRef(PositionalVariableReference)
#undef implVarRef

/* Type related classes which have a targetType() function. */
#define implTypeClass(cls)                                                                          \
QPatternist::ExpressionVisitorResult::Ptr ExpressionNamer::visit(const QPatternist::cls *i) const     \
{                                                                                                   \
    return QPatternist::ExpressionVisitorResult::Ptr(new ExpressionInfo(QLatin1String(#cls),         \
                                                i->targetType()->displayName(Global::namePool()))); \
}

implTypeClass(InstanceOf)
implTypeClass(CastableAs)
#undef implTypeClass

/* Type related classes which have a targetType() function. */
QPatternist::ExpressionVisitorResult::Ptr ExpressionNamer::visit(const QPatternist::CastAs *i) const
{
    return QPatternist::ExpressionVisitorResult::Ptr(new ExpressionInfo(QLatin1String("CastAs"),
                                                i->targetSequenceType()->displayName(Global::namePool())));
}

/* Classes which represent operators. */
#define implOPClass(cls, compClass)                                                                     \
QPatternist::ExpressionVisitorResult::Ptr ExpressionNamer::visit(const QPatternist::cls *i) const         \
{                                                                                                       \
    return QPatternist::ExpressionVisitorResult::Ptr(new ExpressionInfo(QLatin1String(#cls),             \
                                                QPatternist::compClass::displayName(i->operatorID())));  \
}

implOPClass(ArithmeticExpression,   AtomicMathematician)
implOPClass(NodeComparison,         NodeComparison)
implOPClass(QuantifiedExpression,   QuantifiedExpression)
implOPClass(CombineNodes,            CombineNodes)
#undef implOPClass

/* Classes which represent operators. */
#define implCompClass(cls, type)                                                                \
QPatternist::ExpressionVisitorResult::Ptr ExpressionNamer::visit(const QPatternist::cls *i) const \
{                                                                                               \
    return QPatternist::ExpressionVisitorResult::Ptr(new ExpressionInfo(QLatin1String(#cls),     \
                                    QPatternist::AtomicComparator::displayName(i->operatorID(),  \
                                                    QPatternist::AtomicComparator::type)));      \
}

implCompClass(GeneralComparison,    AsGeneralComparison)
implCompClass(ValueComparison,      AsValueComparison)
#undef implCompClass

QPatternist::ExpressionVisitorResult::Ptr ExpressionNamer::visit(const QPatternist::FunctionCall *i) const
{
    return QPatternist::ExpressionVisitorResult::Ptr
           (new ExpressionInfo(QLatin1String("FunctionCall"),
                               Global::namePool()->displayName(i->signature()->name())));
}

QPatternist::ExpressionVisitorResult::Ptr ExpressionNamer::visit(const QPatternist::Literal *i) const
{
    return QPatternist::ExpressionVisitorResult::Ptr(new ExpressionInfo(
                                                    i->item().type()->displayName(Global::namePool()),
                                                    i->item().stringValue()));
}

QPatternist::ExpressionVisitorResult::Ptr ExpressionNamer::visit(const QPatternist::AxisStep *i) const
{
    return QPatternist::ExpressionVisitorResult::Ptr(new ExpressionInfo(QLatin1String("AxisStep"),
                                                                        QPatternist::AxisStep::axisName(i->axis()) +
                                                                        QLatin1String("::") +
                                                                        i->nodeTest()->displayName(Global::namePool())));

}


QPatternist::ExpressionVisitorResult::Ptr ExpressionNamer::visit(const QPatternist::EvaluationCache<true> *i) const
{
    return QPatternist::ExpressionVisitorResult::Ptr(new ExpressionInfo(QLatin1String("EvaluationCache<IsForGlobal=true>"),
                                                                        QLatin1String("Slot: ") + QString::number(i->slot())));

}

QPatternist::ExpressionVisitorResult::Ptr ExpressionNamer::visit(const QPatternist::EvaluationCache<false> *i) const
{
    return QPatternist::ExpressionVisitorResult::Ptr(new ExpressionInfo(QLatin1String("EvaluationCache<IsForGlobal=false>"),
                                                                        QLatin1String("Slot: ") + QString::number(i->slot())));

}

QPatternist::ExpressionVisitorResult::Ptr ExpressionNamer::visit(const QPatternist::NamespaceConstructor *i) const
{
    return QPatternist::ExpressionVisitorResult::Ptr(new ExpressionInfo(QLatin1String("NamespaceConstructor"),
                                                                        Global::namePool()->stringForPrefix(i->namespaceBinding().prefix()) +
                                                                        QLatin1Char('=') +
                                                                        Global::namePool()->stringForNamespace(i->namespaceBinding().namespaceURI())));

}

QPatternist::ExpressionVisitorResult::Ptr ExpressionNamer::visit(const QPatternist::Path *path) const
{

    QPatternist::Path::Kind k = path->kind();
    QString type;

    switch(k)
    {
        case QPatternist::Path::XSLTForEach:
        {
            type = QLatin1String("XSLTForEach");
            break;
        }
        case QPatternist::Path::RegularPath:
        {
            type = QLatin1String("RegularPath");
            break;
        }
        case QPatternist::Path::ForApplyTemplate:
        {
            type = QLatin1String("ForApplyTemplate");
            break;
        }
    }

    return QPatternist::ExpressionVisitorResult::Ptr(new ExpressionInfo(QLatin1String("Path"), type));
}

QPatternist::ExpressionVisitorResult::Ptr ExpressionNamer::visit(const QPatternist::ApplyTemplate *path) const
{
    const QPatternist::TemplateMode::Ptr mode(path->mode());
    return QPatternist::ExpressionVisitorResult::Ptr
           (new ExpressionInfo(QLatin1String("ApplyTemplate"), mode ? Global::namePool()->displayName(mode->name()) : QString::fromLatin1("#current")));
}

// vim: et:ts=4:sw=4:sts=4

