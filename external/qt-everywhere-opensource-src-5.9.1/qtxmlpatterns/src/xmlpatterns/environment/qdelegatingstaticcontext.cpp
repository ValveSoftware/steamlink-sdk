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

/* Patternist */
#include "qbasictypesfactory_p.h"
#include "qfunctionfactorycollection_p.h"
#include "qgenericnamespaceresolver_p.h"
#include "qcommonnamespaces_p.h"
#include "qgenericdynamiccontext_p.h"

#include "qstaticfocuscontext_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

DelegatingStaticContext::DelegatingStaticContext(const StaticContext::Ptr &context) : m_context(context)
{
    Q_ASSERT(context);
}

NamespaceResolver::Ptr DelegatingStaticContext::namespaceBindings() const
{
    return m_context->namespaceBindings();
}

FunctionFactory::Ptr DelegatingStaticContext::functionSignatures() const
{
    return m_context->functionSignatures();
}

DynamicContext::Ptr DelegatingStaticContext::dynamicContext() const
{
    return m_context->dynamicContext();
}

SchemaTypeFactory::Ptr DelegatingStaticContext::schemaDefinitions() const
{
    return m_context->schemaDefinitions();
}

QUrl DelegatingStaticContext::baseURI() const
{
    return m_context->baseURI();
}

void DelegatingStaticContext::setBaseURI(const QUrl &uri)
{
    m_context->setBaseURI(uri);
}

bool DelegatingStaticContext::compatModeEnabled() const
{
    return m_context->compatModeEnabled();
}

QUrl DelegatingStaticContext::defaultCollation() const
{
    return m_context->defaultCollation();
}

QAbstractMessageHandler * DelegatingStaticContext::messageHandler() const
{
    return m_context->messageHandler();
}

void DelegatingStaticContext::setDefaultCollation(const QUrl &uri)
{
    m_context->setDefaultCollation(uri);
}

void DelegatingStaticContext::setNamespaceBindings(const NamespaceResolver::Ptr &resolver)
{
    m_context->setNamespaceBindings(resolver);
}

StaticContext::BoundarySpacePolicy DelegatingStaticContext::boundarySpacePolicy() const
{
    return m_context->boundarySpacePolicy();
}

void DelegatingStaticContext::setBoundarySpacePolicy(const BoundarySpacePolicy policy)
{
    m_context->setBoundarySpacePolicy(policy);
}

StaticContext::ConstructionMode DelegatingStaticContext::constructionMode() const
{
    return m_context->constructionMode();
}

void DelegatingStaticContext::setConstructionMode(const ConstructionMode mode)
{
    m_context->setConstructionMode(mode);
}

StaticContext::OrderingMode DelegatingStaticContext::orderingMode() const
{
    return m_context->orderingMode();
}

void DelegatingStaticContext::setOrderingMode(const OrderingMode mode)
{
    m_context->setOrderingMode(mode);
}

StaticContext::OrderingEmptySequence DelegatingStaticContext::orderingEmptySequence() const
{
    return m_context->orderingEmptySequence();
}

void DelegatingStaticContext::setOrderingEmptySequence(const OrderingEmptySequence ordering)
{
    m_context->setOrderingEmptySequence(ordering);
}

QString DelegatingStaticContext::defaultFunctionNamespace() const
{
    return m_context->defaultFunctionNamespace();
}

void DelegatingStaticContext::setDefaultFunctionNamespace(const QString &ns)
{
    m_context->setDefaultFunctionNamespace(ns);
}

QString DelegatingStaticContext::defaultElementNamespace() const
{
    return m_context->defaultElementNamespace();
}

void DelegatingStaticContext::setDefaultElementNamespace(const QString &ns)
{
    m_context->setDefaultElementNamespace(ns);
}

StaticContext::InheritMode DelegatingStaticContext::inheritMode() const
{
    return m_context->inheritMode();
}

void DelegatingStaticContext::setInheritMode(const InheritMode mode)
{
    m_context->setInheritMode(mode);
}

StaticContext::PreserveMode DelegatingStaticContext::preserveMode() const
{
    return m_context->preserveMode();
}

void DelegatingStaticContext::setPreserveMode(const PreserveMode mode)
{
    m_context->setPreserveMode(mode);
}

ItemType::Ptr DelegatingStaticContext::contextItemType() const
{
    return m_context->contextItemType();
}

ItemType::Ptr DelegatingStaticContext::currentItemType() const
{
    return m_context->currentItemType();
}

ExternalVariableLoader::Ptr DelegatingStaticContext::externalVariableLoader() const
{
    return m_context->externalVariableLoader();
}

StaticContext::Ptr DelegatingStaticContext::copy() const
{
    return StaticContext::Ptr(new DelegatingStaticContext(m_context->copy()));
}

ResourceLoader::Ptr DelegatingStaticContext::resourceLoader() const
{
    return m_context->resourceLoader();
}

NamePool::Ptr DelegatingStaticContext::namePool() const
{
    return m_context->namePool();
}

void DelegatingStaticContext::addLocation(const SourceLocationReflection *const reflection,
                                     const QSourceLocation &location)
{
    m_context->addLocation(reflection, location);
}

StaticContext::LocationHash DelegatingStaticContext::sourceLocations() const
{
    return m_context->sourceLocations();
}

QSourceLocation DelegatingStaticContext::locationFor(const SourceLocationReflection *const reflection) const
{
    return m_context->locationFor(reflection);
}

const QAbstractUriResolver *DelegatingStaticContext::uriResolver() const
{
    return m_context->uriResolver();
}

VariableSlotID DelegatingStaticContext::currentRangeSlot() const
{
    return m_context->currentRangeSlot();
}

VariableSlotID DelegatingStaticContext::allocateRangeSlot()
{
    return m_context->allocateRangeSlot();
}

void DelegatingStaticContext::setCompatModeEnabled(const bool newVal)
{
    m_context->setCompatModeEnabled(newVal);
}

QT_END_NAMESPACE
