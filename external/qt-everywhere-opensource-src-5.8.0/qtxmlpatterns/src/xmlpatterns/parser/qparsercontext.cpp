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

#include <QtGlobal>

#include "qexpression_p.h"
#include "qstaticcontext_p.h"
#include "qtokenizer_p.h"

#include "qparsercontext_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

ParserContext::ParserContext(const StaticContext::Ptr &context,
                             const QXmlQuery::QueryLanguage lang,
                             Tokenizer *const tokener) : staticContext(context)
                                                       , tokenizer(tokener)
                                                       , languageAccent(lang)
                                                       , nodeTestSource(BuiltinTypes::element)
                                                       , moduleNamespace(StandardNamespaces::empty)
                                                       , isPreviousEnclosedExpr(false)
                                                       , elementConstructorDepth(0)
                                                       , hasSecondPrologPart(false)
                                                       , preserveNamespacesMode(true)
                                                       , inheritNamespacesMode(true)
                                                       , isParsingPattern(false)
                                                       , currentImportPrecedence(1)
                                                       , m_evaluationCacheSlot(-1)
                                                       , m_expressionSlot(0)
                                                       , m_positionSlot(-1)
                                                       , m_globalVariableSlot(-1)
                                                       , m_currentTemplateID(InitialTemplateID)
{
    resolvers.push(context->namespaceBindings());
    Q_ASSERT(tokenizer);
    Q_ASSERT(context);
    m_isParsingWithParam.push(false);
    isBackwardsCompat.push(false);
}

void ParserContext::finalizePushedVariable(const int amount,
                                           const bool shouldPop)
{
    for(int i = 0; i < amount; ++i)
    {
        const VariableDeclaration::Ptr var(shouldPop ? variables.pop() : variables.top());
        Q_ASSERT(var);

        if(var->isUsed())
            continue;
        else
        {
            staticContext->warning(QtXmlPatterns::tr("The variable %1 is unused")
                                                     .arg(formatKeyword(var, staticContext->namePool())));
        }
    }
}

void ParserContext::handleStackOverflow(const char *, short **yyss, size_t,
                                        TokenValue **yyvs, size_t,
                                        YYLTYPE **yyls, size_t,
                                        size_t *yystacksize)
{
    bool isFirstTime = parserStack_yyvs.isEmpty();
    Q_ASSERT(*yystacksize < INT_MAX - 50);
    int new_yystacksize = static_cast<int>(*yystacksize) + 50;
    parserStack_yyss.resize(new_yystacksize);
    parserStack_yyvs.resize(new_yystacksize);
    parserStack_yyls.resize(new_yystacksize);
    if (isFirstTime) {
        for (int i = 0, ei = static_cast<int>(*yystacksize); i != ei; ++i) {
            parserStack_yyss[i] = (*yyss)[i];
            parserStack_yyvs[i] = (*yyvs)[i];
            parserStack_yyls[i] = (*yyls)[i];
        }
    }
    *yyss = parserStack_yyss.data();
    *yyvs = parserStack_yyvs.data();
    *yyls = parserStack_yyls.data();
    *yystacksize = new_yystacksize;
}

QT_END_NAMESPACE

