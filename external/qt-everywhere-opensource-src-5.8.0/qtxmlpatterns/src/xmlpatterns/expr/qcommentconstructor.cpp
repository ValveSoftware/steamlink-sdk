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
#include "qpatternistlocale_p.h"
#include "qnodebuilder_p.h"

#include "qcommentconstructor_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

CommentConstructor::CommentConstructor(const Expression::Ptr &op) : SingleContainer(op)
{
}

QString CommentConstructor::evaluateContent(const DynamicContext::Ptr &context) const
{
    const Item item(m_operand->evaluateSingleton(context));

    if(!item)
        return QString();

    const QString content(item.stringValue());

    if(content.contains(QLatin1String("--")))
    {
        context->error(QtXmlPatterns::tr("A comment cannot contain %1")
                       .arg(formatData("--")),
                       ReportContext::XQDY0072, this);
    }
    else if(content.endsWith(QLatin1Char('-')))
    {
        context->error(QtXmlPatterns::tr("A comment cannot end with a %1.")
                       .arg(formatData(QLatin1Char('-'))),
                       ReportContext::XQDY0072, this);
    }

    return content;
}

Item CommentConstructor::evaluateSingleton(const DynamicContext::Ptr &context) const
{
    const QString content(evaluateContent(context));
    const NodeBuilder::Ptr nodeBuilder(context->nodeBuilder(QUrl()));
    nodeBuilder->comment(content);

    const QAbstractXmlNodeModel::Ptr nm(nodeBuilder->builtDocument());
    context->addNodeModel(nm);

    return nm->root(QXmlNodeModelIndex());
}

void CommentConstructor::evaluateToSequenceReceiver(const DynamicContext::Ptr &context) const
{
    const QString content(evaluateContent(context));
    QAbstractXmlReceiver *const receiver = context->outputReceiver();

    receiver->comment(content);
}

SequenceType::Ptr CommentConstructor::staticType() const
{
    return CommonSequenceTypes::ExactlyOneComment;
}

SequenceType::List CommentConstructor::expectedOperandTypes() const
{
    SequenceType::List result;
    result.append(CommonSequenceTypes::ZeroOrOneString);
    return result;
}

Expression::Properties CommentConstructor::properties() const
{
    return DisableElimination | IsNodeConstructor;
}

ExpressionVisitorResult::Ptr
CommentConstructor::accept(const ExpressionVisitor::Ptr &visitor) const
{
    return visitor->visit(this);
}

QT_END_NAMESPACE
