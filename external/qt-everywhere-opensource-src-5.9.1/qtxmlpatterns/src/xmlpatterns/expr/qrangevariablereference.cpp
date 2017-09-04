/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtXmlPatterns module of the Qt Toolkit.
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

#include "qboolean_p.h"
#include "qgenericsequencetype_p.h"

#include "qrangevariablereference_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

RangeVariableReference::RangeVariableReference(const Expression::Ptr &source,
                                               const VariableSlotID slotP) : VariableReference(slotP),
                                                                             m_sourceExpression(source)
{
    Q_ASSERT(source);
}

bool RangeVariableReference::evaluateEBV(const DynamicContext::Ptr &context) const
{
    Q_ASSERT_X(context->rangeVariable(slot()), Q_FUNC_INFO, "The range variable must be set.");
    return Boolean::evaluateEBV(context->rangeVariable(slot()), context);
}

Item RangeVariableReference::evaluateSingleton(const DynamicContext::Ptr &context) const
{
    Q_ASSERT_X(context->rangeVariable(slot()), Q_FUNC_INFO, "The range variable must be set.");
    return context->rangeVariable(slot());
}

SequenceType::Ptr RangeVariableReference::staticType() const
{
    return makeGenericSequenceType(m_sourceExpression->staticType()->itemType(),
                                   Cardinality::exactlyOne());
}

Expression::ID RangeVariableReference::id() const
{
    return IDRangeVariableReference;
}

ExpressionVisitorResult::Ptr
RangeVariableReference::accept(const ExpressionVisitor::Ptr &visitor) const
{
    return visitor->visit(this);
}

Expression::Properties RangeVariableReference::properties() const
{
    return DependsOnLocalVariable;
}

QT_END_NAMESPACE
