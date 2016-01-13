/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the test suite of the Qt Toolkit.
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

#ifndef PatternistSDK_ExpressionNamer_H
#define PatternistSDK_ExpressionNamer_H

#include "Global.h"
#include <private/qexpressiondispatch_p.h>

QT_BEGIN_NAMESPACE

namespace QPatternistSDK
{
    /**
     * @short Extracts debug information from a QPatternist::Expression tree.
     *
     * This data is the name of the AST node(typically the class name),
     * and additional data such as the value, type of operator, and so forth. The result
     * is returned(from visit()), is an ExpressionInfo instance.
     *
     * @see ExpressionInfo
     * @see ASTItem
     * @ingroup PatternistSDK
     * @author Frans Englich <frans.englich@nokia.com>
     */
    class Q_PATTERNISTSDK_EXPORT ExpressionNamer : public QPatternist::ExpressionVisitor
    {
    public:

        virtual QPatternist::ExpressionVisitorResult::Ptr
        visit(const QPatternist::AndExpression *) const;

        virtual QPatternist::ExpressionVisitorResult::Ptr
        visit(const QPatternist::ApplyTemplate *) const;

        virtual QPatternist::ExpressionVisitorResult::Ptr
        visit(const QPatternist::ArgumentReference *) const;

        virtual QPatternist::ExpressionVisitorResult::Ptr
        visit(const QPatternist::ArithmeticExpression *) const;

        virtual QPatternist::ExpressionVisitorResult::Ptr
        visit(const QPatternist::Atomizer *) const;

        virtual QPatternist::ExpressionVisitorResult::Ptr
        visit(const QPatternist::AttributeConstructor *) const;

        virtual QPatternist::ExpressionVisitorResult::Ptr
        visit(const QPatternist::AttributeNameValidator *) const;

        virtual QPatternist::ExpressionVisitorResult::Ptr
        visit(const QPatternist::CallTemplate *) const;

        virtual QPatternist::ExpressionVisitorResult::Ptr
        visit(const QPatternist::CardinalityVerifier *) const;

        virtual QPatternist::ExpressionVisitorResult::Ptr
        visit(const QPatternist::CastAs *) const;

        virtual QPatternist::ExpressionVisitorResult::Ptr
        visit(const QPatternist::CastableAs *) const;

        virtual QPatternist::ExpressionVisitorResult::Ptr
        visit(const QPatternist::CollationChecker *) const;

        virtual QPatternist::ExpressionVisitorResult::Ptr
        visit(const QPatternist::CombineNodes *) const;

        virtual QPatternist::ExpressionVisitorResult::Ptr
        visit(const QPatternist::ComputedNamespaceConstructor *) const;

        virtual QPatternist::ExpressionVisitorResult::Ptr
        visit(const QPatternist::CommentConstructor *) const;

        virtual QPatternist::ExpressionVisitorResult::Ptr
        visit(const QPatternist::ContextItem *) const;

        virtual QPatternist::ExpressionVisitorResult::Ptr
        visit(const QPatternist::CopyOf *) const;

        virtual QPatternist::ExpressionVisitorResult::Ptr
        visit(const QPatternist::CurrentItemStore *) const;

        virtual QPatternist::ExpressionVisitorResult::Ptr
        visit(const QPatternist::DocumentConstructor *) const;

        virtual QPatternist::ExpressionVisitorResult::Ptr
        visit(const QPatternist::DynamicContextStore *) const;

        virtual QPatternist::ExpressionVisitorResult::Ptr
        visit(const QPatternist::EBVExtractor *) const;

        virtual QPatternist::ExpressionVisitorResult::Ptr
        visit(const QPatternist::ElementConstructor *) const;

        virtual QPatternist::ExpressionVisitorResult::Ptr
        visit(const QPatternist::EmptySequence *) const;

        virtual QPatternist::ExpressionVisitorResult::Ptr
        visit(const QPatternist::ExpressionSequence *) const;

        virtual QPatternist::ExpressionVisitorResult::Ptr
        visit(const QPatternist::ExpressionVariableReference *) const;

        virtual QPatternist::ExpressionVisitorResult::Ptr
        visit(const QPatternist::ExternalVariableReference *) const;

        virtual QPatternist::ExpressionVisitorResult::Ptr
        visit(const QPatternist::EvaluationCache<true> *) const;

        virtual QPatternist::ExpressionVisitorResult::Ptr
        visit(const QPatternist::EvaluationCache<false> *) const;

        virtual QPatternist::ExpressionVisitorResult::Ptr
        visit(const QPatternist::FirstItemPredicate *) const;

        virtual QPatternist::ExpressionVisitorResult::Ptr
        visit(const QPatternist::ForClause *) const;

        virtual QPatternist::ExpressionVisitorResult::Ptr
        visit(const QPatternist::FunctionCall *) const;

        virtual QPatternist::ExpressionVisitorResult::Ptr
        visit(const QPatternist::GeneralComparison *) const;

        virtual QPatternist::ExpressionVisitorResult::Ptr
        visit(const QPatternist::GenericPredicate *) const;

        virtual QPatternist::ExpressionVisitorResult::Ptr
        visit(const QPatternist::IfThenClause *) const;

        virtual QPatternist::ExpressionVisitorResult::Ptr
        visit(const QPatternist::InstanceOf *) const;

        virtual QPatternist::ExpressionVisitorResult::Ptr
        visit(const QPatternist::ItemVerifier *) const;

        virtual QPatternist::ExpressionVisitorResult::Ptr
        visit(const QPatternist::LetClause *) const;

        virtual QPatternist::ExpressionVisitorResult::Ptr
        visit(const QPatternist::Literal *) const;

        virtual QPatternist::ExpressionVisitorResult::Ptr
        visit(const QPatternist::LiteralSequence *) const;

        virtual QPatternist::ExpressionVisitorResult::Ptr
        visit(const QPatternist::NCNameConstructor *) const;

        virtual QPatternist::ExpressionVisitorResult::Ptr
        visit(const QPatternist::NodeComparison *) const;

        virtual QPatternist::ExpressionVisitorResult::Ptr
        visit(const QPatternist::NodeSortExpression *) const;

        virtual QPatternist::ExpressionVisitorResult::Ptr
        visit(const QPatternist::OrderBy *) const;

        virtual QPatternist::ExpressionVisitorResult::Ptr
        visit(const QPatternist::OrExpression *) const;

        virtual QPatternist::ExpressionVisitorResult::Ptr
        visit(const QPatternist::ParentNodeAxis *) const;

        virtual QPatternist::ExpressionVisitorResult::Ptr
        visit(const QPatternist::Path *) const;

        virtual QPatternist::ExpressionVisitorResult::Ptr
        visit(const QPatternist::PositionalVariableReference *) const;

        virtual QPatternist::ExpressionVisitorResult::Ptr
        visit(const QPatternist::ProcessingInstructionConstructor *) const;

        virtual QPatternist::ExpressionVisitorResult::Ptr
        visit(const QPatternist::QNameConstructor *) const;

        virtual QPatternist::ExpressionVisitorResult::Ptr
        visit(const QPatternist::QuantifiedExpression *) const;

        virtual QPatternist::ExpressionVisitorResult::Ptr
        visit(const QPatternist::RangeExpression *) const;

        virtual QPatternist::ExpressionVisitorResult::Ptr
        visit(const QPatternist::RangeVariableReference *) const;

        virtual QPatternist::ExpressionVisitorResult::Ptr
        visit(const QPatternist::ReturnOrderBy *) const;

        virtual QPatternist::ExpressionVisitorResult::Ptr
        visit(const QPatternist::SimpleContentConstructor *) const;

        virtual QPatternist::ExpressionVisitorResult::Ptr
        visit(const QPatternist::StaticBaseURIStore *) const;

        virtual QPatternist::ExpressionVisitorResult::Ptr
        visit(const QPatternist::StaticCompatibilityStore *) const;

        virtual QPatternist::ExpressionVisitorResult::Ptr
        visit(const QPatternist::AxisStep *) const;

        virtual QPatternist::ExpressionVisitorResult::Ptr
        visit(const QPatternist::TemplateParameterReference *) const;

        virtual QPatternist::ExpressionVisitorResult::Ptr
        visit(const QPatternist::TextNodeConstructor *) const;

        virtual QPatternist::ExpressionVisitorResult::Ptr
        visit(const QPatternist::TreatAs *) const;

        virtual QPatternist::ExpressionVisitorResult::Ptr
        visit(const QPatternist::TruthPredicate *) const;

        virtual QPatternist::ExpressionVisitorResult::Ptr
        visit(const QPatternist::UntypedAtomicConverter *) const;

        virtual QPatternist::ExpressionVisitorResult::Ptr
        visit(const QPatternist::UnresolvedVariableReference *) const;

        virtual QPatternist::ExpressionVisitorResult::Ptr
        visit(const QPatternist::ArgumentConverter *) const;

        virtual QPatternist::ExpressionVisitorResult::Ptr
        visit(const QPatternist::UserFunctionCallsite *) const;

        virtual QPatternist::ExpressionVisitorResult::Ptr
        visit(const QPatternist::ValidationError *) const;

        virtual QPatternist::ExpressionVisitorResult::Ptr
        visit(const QPatternist::ValueComparison *) const;

        virtual QPatternist::ExpressionVisitorResult::Ptr
        visit(const QPatternist::NamespaceConstructor *) const;
    };
}

QT_END_NAMESPACE

#endif
// vim: et:ts=4:sw=4:sts=4
