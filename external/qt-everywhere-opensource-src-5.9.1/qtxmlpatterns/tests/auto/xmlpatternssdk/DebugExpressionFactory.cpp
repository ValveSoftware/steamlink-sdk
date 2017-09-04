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

#include <QStringList>
#include <QVariant>
#include <QtDebug>
#include <QXmlNamePool>

#include <private/qfunctionfactorycollection_p.h>

#include "ASTItem.h"
#include "ExpressionInfo.h"
#include "ExpressionNamer.h"
#include "Global.h"

#include "DebugExpressionFactory.h"

using namespace QPatternistSDK;
using namespace QPatternist;

static const QPatternist::ExpressionVisitor::Ptr namer(new ExpressionNamer());

QStringList DebugExpressionFactory::availableFunctionSignatures()
{
    const QPatternist::FunctionFactory::Ptr factory(QPatternist::FunctionFactoryCollection::xpath20Factory(Global::namePool()));
    const QPatternist::FunctionSignature::Hash signs(factory->functionSignatures());
    const QPatternist::FunctionSignature::Hash::const_iterator end(signs.constEnd());
    QPatternist::FunctionSignature::Hash::const_iterator it(signs.constBegin());
    QStringList retval;

    while(it != end)
    {
        retval << it.value()->displayName(Global::namePool());
        ++it;
    }

    return retval;
}

ASTItem *DebugExpressionFactory::buildASTTree(const QPatternist::Expression::Ptr &expr,
                                              ASTItem *parent,
                                              const QPatternist::SequenceType::Ptr &reqType)
{
    Q_ASSERT(expr);
    const QPatternist::ExpressionVisitorResult::Ptr exprInfo(expr->accept(namer));
    Q_ASSERT(exprInfo);
    const ExpressionInfo *const constExprInfo = static_cast<const ExpressionInfo *>(exprInfo.data());
    const QString name(constExprInfo->name());
    const QString details(constExprInfo->details());
    const QString rType(reqType ? reqType->displayName(Global::namePool()) : QLatin1String("Not specified"));

    /* ---------- Handle its staticType() -------- */
    const QPatternist::SequenceType::Ptr type(expr->staticType());
    QString seqType;

    if(type)
        seqType = type->displayName(Global::namePool());
    else
        seqType = QLatin1String("no type, null pointer returned");
    /* ------------------------------------------- */

    ASTItem *const node = new ASTItem(parent, name, details, seqType, rType);

    /* ------------ Handle child nodes ----------- */
    const QPatternist::Expression::List children(expr->operands());
    QPatternist::Expression::List::const_iterator it(children.constBegin());
    const QPatternist::Expression::List::const_iterator end(children.constEnd());

    const QPatternist::SequenceType::List reqTypes(expr->expectedOperandTypes());
    const QPatternist::SequenceType::List::const_iterator typeEnd(reqTypes.constEnd());
    QPatternist::SequenceType::List::const_iterator typeIt(reqTypes.constBegin());
    QPatternist::SequenceType::Ptr t;

    for(; it != end; ++it)
    {
        if(typeIt != typeEnd)
        {
            t = *typeIt;
            ++typeIt;
        }

        node->appendChild(buildASTTree(*it, node, t));
    }
    /* ------------------------------------------- */

    return node;
}

QPatternist::Expression::Ptr
DebugExpressionFactory::createExpression(QIODevice *const expr,
                                         const QPatternist::StaticContext::Ptr &context,
                                         const QXmlQuery::QueryLanguage lang,
                                         const QPatternist::SequenceType::Ptr &requiredType,
                                         const QUrl &baseURI,
                                         const QXmlName &initialTemplateName)
{
    /* Create the root node. */
    m_ast = new ASTItem(0, QString());

    return ExpressionFactory::createExpression(expr, context, lang, requiredType, baseURI, initialTemplateName);
}

void DebugExpressionFactory::processTreePass(const QPatternist::Expression::Ptr &expr,
                                             const CompilationStage stage)
{
    ASTItem *newChild = 0;

    switch(stage)
    {
        case QueryBodyInitial:
        {
            newChild = new ASTItem(m_ast, QLatin1String("Initial Build"));
            break;
        }
        case QueryBodyTypeCheck:
        {
            newChild = new ASTItem(m_ast, QLatin1String("Type Check"));
            break;
        }
        case QueryBodyCompression:
        {
            newChild = new ASTItem(m_ast, QLatin1String("Compression"));
            break;
        }
        case UserFunctionTypeCheck:
        {
            newChild = new ASTItem(m_ast, QLatin1String("User Function Type Check"));
            break;
        }
        case UserFunctionCompression:
        {
            newChild = new ASTItem(m_ast, QLatin1String("User Function Compression"));
            break;
        }
        case GlobalVariableTypeCheck:
        {
            newChild = new ASTItem(m_ast, QLatin1String("Global Variable Type Check"));
            break;
        }
    }

    Q_ASSERT(newChild);
    m_ast->appendChild(newChild);
    newChild->appendChild(buildASTTree(expr, newChild, QPatternist::SequenceType::Ptr()));
}

void DebugExpressionFactory::processTemplateRule(const Expression::Ptr &body,
                                                 const TemplatePattern::Ptr &pattern,
                                                 const QXmlName &mode,
                                                 const TemplateCompilationStage stage)
{
    QString title = QLatin1String("T-Rule ");

    switch (stage) {
    case TemplateInitial:
        title += QLatin1String("Initial Build");
        break;
    case TemplateTypeCheck:
        title += QLatin1String("Type Check");
        break;
    case TemplateCompress:
        title += QLatin1String("Compression");
        break;
    }
    title += QLatin1String(" mode: ")
        + Global::namePool()->displayName(mode)
        + QLatin1String(" priority: ")
        + QString::number(pattern->priority());

    ASTItem *const newChild = new ASTItem(m_ast, title);
    m_ast->appendChild(newChild);

    newChild->appendChild(buildASTTree(pattern->matchPattern(), newChild, QPatternist::SequenceType::Ptr()));
    newChild->appendChild(buildASTTree(body, newChild, QPatternist::SequenceType::Ptr()));
}

void DebugExpressionFactory::processNamedTemplate(const QXmlName &name,
                                                  const Expression::Ptr &body,
                                                  const TemplateCompilationStage stage)
{
    QString title;

    switch (stage) {
    case TemplateInitial:
        title += QLatin1String("Named Template Initial Build");
        break;
    case TemplateTypeCheck:
        title += QLatin1String("Named Template Type Check");
        break;
    case TemplateCompress:
        title += QLatin1String("Named Template Compression");
        break;
    }

    title += QLatin1String(": ") + Global::namePool()->displayName(name);

    ASTItem *const newChild = new ASTItem(m_ast, title);

    m_ast->appendChild(newChild);
    newChild->appendChild(buildASTTree(body, newChild, QPatternist::SequenceType::Ptr()));
}

ASTItem *DebugExpressionFactory::astTree() const
{
    return m_ast;
}

// vim: et:ts=4:sw=4:sts=4
