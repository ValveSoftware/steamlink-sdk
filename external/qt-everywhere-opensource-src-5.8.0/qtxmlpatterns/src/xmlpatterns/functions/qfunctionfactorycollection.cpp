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

#include "qbasictypesfactory_p.h"
#include "qconstructorfunctionsfactory_p.h"
#include "qfunctioncall_p.h"
#include "qxpath10corefunctions_p.h"
#include "qxpath20corefunctions_p.h"
#include "qxslt20corefunctions_p.h"

#include "qfunctionfactorycollection_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

Expression::Ptr FunctionFactoryCollection::createFunctionCall(const QXmlName name,
                                                              const Expression::List &arguments,
                                                              const StaticContext::Ptr &context,
                                                              const SourceLocationReflection *const r)
{
    const_iterator it;
    const_iterator e(constEnd());
    Expression::Ptr function;

    for(it = constBegin(); it != e; ++it)
    {
        function = (*it)->createFunctionCall(name, arguments, context, r);

        if(function)
            break;
    }

    return function;
}

bool FunctionFactoryCollection::isAvailable(const NamePool::Ptr &np, const QXmlName name, const xsInteger arity)
{
    const_iterator it;
    const_iterator e(constEnd());

    for(it = constBegin(); it != e; ++it)
        if((*it)->isAvailable(np, name, arity))
            return true;

    return false;
}

FunctionSignature::Hash FunctionFactoryCollection::functionSignatures() const
{
    /* We simply grab the function signatures for each library, and
     * put them all in one list. */

    const const_iterator e(constEnd());
    FunctionSignature::Hash result;

    for(const_iterator it(constBegin()); it != e; ++it)
    {
        const FunctionSignature::Hash::const_iterator e2((*it)->functionSignatures().constEnd());
        FunctionSignature::Hash::const_iterator sit((*it)->functionSignatures().constBegin());

        for(; sit != e2; ++sit)
            result.insert(sit.key(), sit.value());
    }

    return result;
}

FunctionSignature::Ptr FunctionFactoryCollection::retrieveFunctionSignature(const NamePool::Ptr &, const QXmlName name)
{
    return functionSignatures().value(name);
}

FunctionFactory::Ptr FunctionFactoryCollection::xpath10Factory()
{
    /* We don't use a global static for caching this, because AbstractFunctionFactory
     * stores state specific to the NamePool, when being used. */
    return  FunctionFactory::Ptr(new XPath10CoreFunctions());
}

FunctionFactory::Ptr FunctionFactoryCollection::xpath20Factory(const NamePool::Ptr &np)
{
    /* We don't use a global static for caching this, because AbstractFunctionFactory
     * stores state specific to the NamePool, when being used. */
    const FunctionFactoryCollection::Ptr fact(new FunctionFactoryCollection());
    fact->append(xpath10Factory());
    fact->append(FunctionFactory::Ptr(new XPath20CoreFunctions()));
    fact->append(FunctionFactory::Ptr(
                            new ConstructorFunctionsFactory(np, BasicTypesFactory::self(np))));
    return fact;
}

FunctionFactory::Ptr FunctionFactoryCollection::xslt20Factory(const NamePool::Ptr &np)
{
    const FunctionFactory::Ptr retval(xpath20Factory(np));
    static_cast<FunctionFactoryCollection *>(retval.data())->append(FunctionFactory::Ptr(new XSLT20CoreFunctions()));
    return retval;
}

QT_END_NAMESPACE
