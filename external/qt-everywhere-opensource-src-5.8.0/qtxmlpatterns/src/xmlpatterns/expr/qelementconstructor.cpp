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

#include "qcommonsequencetypes_p.h"
#include "qdelegatingnamespaceresolver_p.h"
#include "qnamespaceconstructor_p.h"
#include "qnodebuilder_p.h"
#include "qoutputvalidator_p.h"
#include "qqnamevalue_p.h"
#include "qstaticnamespacecontext_p.h"

#include "qelementconstructor_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

ElementConstructor::ElementConstructor(const Expression::Ptr &op1,
                                       const Expression::Ptr &op2,
                                       const bool isXSLT) : PairContainer(op1, op2)
                                                          , m_isXSLT(isXSLT)
{
}

Item ElementConstructor::evaluateSingleton(const DynamicContext::Ptr &context) const
{
    const Item name(m_operand1->evaluateSingleton(context));

    const NodeBuilder::Ptr nodeBuilder(context->nodeBuilder(m_staticBaseURI));
    OutputValidator validator(nodeBuilder.data(), context, this, m_isXSLT);

    const DynamicContext::Ptr receiverContext(context->createReceiverContext(&validator));

    nodeBuilder->startElement(name.as<QNameValue>()->qName());
    m_operand2->evaluateToSequenceReceiver(receiverContext);
    nodeBuilder->endElement();

    const QAbstractXmlNodeModel::Ptr nm(nodeBuilder->builtDocument());
    context->addNodeModel(nm);

    return nm->root(QXmlNodeModelIndex());
}

void ElementConstructor::evaluateToSequenceReceiver(const DynamicContext::Ptr &context) const
{
    /* We create an OutputValidator here too. If we're serializing(a common
     * case, unfortunately) the receiver is already validating in order to
     * catch cases where a computed attribute constructor is followed by an
     * element constructor, but in the cases where we're not serializing it's
     * necessary that we validate in this step. */
    const Item name(m_operand1->evaluateSingleton(context));
    QAbstractXmlReceiver *const receiver = context->outputReceiver();

    OutputValidator validator(receiver, context, this, m_isXSLT);
    const DynamicContext::Ptr receiverContext(context->createReceiverContext(&validator));

    receiver->startElement(name.as<QNameValue>()->qName());
    m_operand2->evaluateToSequenceReceiver(receiverContext);
    receiver->endElement();
}

Expression::Ptr ElementConstructor::typeCheck(const StaticContext::Ptr &context,
                                              const SequenceType::Ptr &reqType)
{
    /* What does this code do? When type checking our children, our namespace
     * bindings, which are also children of the form of NamespaceConstructor
     * instances, must be statically in-scope for them, so find them and
     * shuffle their bindings into the StaticContext. */

    m_staticBaseURI = context->baseURI();

    /* Namespace declarations changes the in-scope bindings, so let's
     * first lookup our child NamespaceConstructors. */
    const ID operandID = m_operand2->id();

    NamespaceResolver::Bindings overrides;
    if(operandID == IDExpressionSequence)
    {
        const Expression::List operands(m_operand2->operands());
        const int len = operands.count();

        for(int i = 0; i < len; ++i)
        {
            if(operands.at(i)->is(IDNamespaceConstructor))
            {
                const QXmlName &nb = operands.at(i)->as<NamespaceConstructor>()->namespaceBinding();
                overrides.insert(nb.prefix(), nb.namespaceURI());
            }
        }
    }

    const NamespaceResolver::Ptr newResolver(new DelegatingNamespaceResolver(context->namespaceBindings(), overrides));
    const StaticContext::Ptr augmented(new StaticNamespaceContext(newResolver, context));

    return PairContainer::typeCheck(augmented, reqType);
}

SequenceType::Ptr ElementConstructor::staticType() const
{
    return CommonSequenceTypes::ExactlyOneElement;
}

SequenceType::List ElementConstructor::expectedOperandTypes() const
{
    SequenceType::List result;
    result.append(CommonSequenceTypes::ExactlyOneQName);
    result.append(CommonSequenceTypes::ZeroOrMoreItems);
    return result;
}

Expression::Properties ElementConstructor::properties() const
{
    return DisableElimination | IsNodeConstructor;
}

ExpressionVisitorResult::Ptr
ElementConstructor::accept(const ExpressionVisitor::Ptr &visitor) const
{
    return visitor->visit(this);
}

QT_END_NAMESPACE
