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

#include "qpatternistlocale_p.h"

#include "qdocumentcontentvalidator_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

DocumentContentValidator::
DocumentContentValidator(QAbstractXmlReceiver *const receiver,
                         const DynamicContext::Ptr &context,
                         const Expression::ConstPtr &expr) : m_receiver(receiver)
                                                           , m_context(context)
                                                           , m_expr(expr)
                                                           , m_elementDepth(0)
{
    Q_ASSERT(receiver);
    Q_ASSERT(m_expr);
    Q_ASSERT(context);
}

void DocumentContentValidator::namespaceBinding(const QXmlName &nb)
{
    m_receiver->namespaceBinding(nb);
}

void DocumentContentValidator::startElement(const QXmlName &name)
{
    ++m_elementDepth;
    m_receiver->startElement(name);
}

void DocumentContentValidator::endElement()
{
    Q_ASSERT(m_elementDepth > 0);
    --m_elementDepth;
    m_receiver->endElement();
}

void DocumentContentValidator::attribute(const QXmlName &name,
                                         const QStringRef &value)
{
    if(m_elementDepth == 0)
    {
        m_context->error(QtXmlPatterns::tr("An attribute node cannot be a "
                                           "child of a document node. "
                                           "Therefore, the attribute %1 "
                                           "is out of place.")
                         .arg(formatKeyword(m_context->namePool(), name)),
                         ReportContext::XPTY0004, m_expr.data());
    }
    else
        m_receiver->attribute(name, value);
}

void DocumentContentValidator::comment(const QString &value)
{
    m_receiver->comment(value);
}

void DocumentContentValidator::characters(const QStringRef &value)
{
    m_receiver->characters(value);
}

void DocumentContentValidator::processingInstruction(const QXmlName &name,
                                                     const QString &value)
{
    m_receiver->processingInstruction(name, value);
}

void DocumentContentValidator::item(const Item &outputItem)
{
    /* We can't send outputItem directly to m_receiver since its item() function
     * won't dispatch to this DocumentContentValidator, but to itself. We're not sub-classing here,
     * we're delegating. */

    if(outputItem.isNode())
        sendAsNode(outputItem);
    else
        m_receiver->item(outputItem);
}

void DocumentContentValidator::startDocument()
{
    m_receiver->startDocument();
}

void DocumentContentValidator::endDocument()
{
    m_receiver->endDocument();
}

void DocumentContentValidator::atomicValue(const QVariant &value)
{
    Q_UNUSED(value);
}

void DocumentContentValidator::startOfSequence()
{
}

void DocumentContentValidator::endOfSequence()
{
}

QT_END_NAMESPACE
