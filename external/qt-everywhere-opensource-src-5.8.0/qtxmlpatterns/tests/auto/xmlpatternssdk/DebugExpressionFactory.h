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

#ifndef PatternistSDK_DebugExpressionFactory_H
#define PatternistSDK_DebugExpressionFactory_H

#include "Global.h"
#include <private/qexpressionfactory_p.h>
#include <private/qfunctionfactory_p.h>

QT_BEGIN_NAMESPACE

namespace QPatternistSDK
{
    class ASTItem;

    /**
     * @short Is a QPatternist::ExpressionFactory, with the
     * difference that it provides the hooks for building from a tree of
     * debug data from the compiled expression.
     *
     * This tree can be retrieved via astTree(). The astTree() function
     * returns the AST built the last time createExpression() was called.
     *
     * @ingroup PatternistSDK
     * @author Frans Englich <frans.englich@nokia.com>
     */
    class Q_PATTERNISTSDK_EXPORT DebugExpressionFactory : public QPatternist::ExpressionFactory
    {
    public:
        DebugExpressionFactory() : m_ast(0)
        {
        }

        typedef QExplicitlySharedDataPointer<DebugExpressionFactory> Ptr;
        /**
         * Identical to ExpressionFactory::createExpression() with the difference
         * that it builds an ASTItem tree which can be accessed via astTree().
         */
        virtual QPatternist::Expression::Ptr createExpression(QIODevice *const expr,
                                                              const QPatternist::StaticContext::Ptr &context,
                                                              const QXmlQuery::QueryLanguage lang,
                                                              const QPatternist::SequenceType::Ptr &requiredType,
                                                              const QUrl &queryURI,
                                                              const QXmlName &initialTemplateName);

        /**
         * @returns an ASTItem tree built for the last created expression,
         * via createExpression().
         */
        virtual ASTItem *astTree() const;

        /**
         * @returns a list containing string representations of all available
         * functions in Patternist. Each QString in the returned QStringList
         * is a function synopsis for human consumption.
         */
        static QStringList availableFunctionSignatures();

    protected:
        /**
         * Performs the ASTItem tree building.
         */
        virtual void processTreePass(const QPatternist::Expression::Ptr &tree,
                                     const CompilationStage stage);

        void processTemplateRule(const QPatternist::Expression::Ptr &body,
                                 const QPatternist::TemplatePattern::Ptr &pattern,
                                 const QXmlName &mode,
                                 const TemplateCompilationStage stage);

        void processNamedTemplate(const QXmlName &name,
                                  const QPatternist::Expression::Ptr &body,
                                  const TemplateCompilationStage stage);
    private:
        static ASTItem *buildASTTree(const QPatternist::Expression::Ptr &expr,
                                     ASTItem *const parent,
                                     const QPatternist::SequenceType::Ptr &reqType);
        ASTItem *m_ast;
    };
}

QT_END_NAMESPACE

#endif
// vim: et:ts=4:sw=4:sts=4
