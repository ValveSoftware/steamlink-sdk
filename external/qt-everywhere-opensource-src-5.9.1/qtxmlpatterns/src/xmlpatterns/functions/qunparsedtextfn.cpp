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

#include "qanyuri_p.h"

#include "qunparsedtextfn_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

Item UnparsedTextFN::evaluateSingleton(const DynamicContext::Ptr &context) const
{
    Q_ASSERT(m_operands.count() == 1 || m_operands.count() == 2);
    const Item href(m_operands.first()->evaluateSingleton(context));
    if(!href)
        return Item();

    const QUrl mayRela(AnyURI::toQUrl<ReportContext::XTDE1170>(href.stringValue(),
                                                               context,
                                                               this));

    const QUrl uri(context->resolveURI(mayRela, staticBaseURI()));

    if(uri.hasFragment())
    {
        context->error(QtXmlPatterns::tr("The URI cannot have a fragment"),
                       ReportContext::XTDE1170, this);
    }

    QString encoding;

    if(m_operands.count() == 2)
    {
        const Item encodingArg(m_operands.at(1)->evaluateSingleton(context));
        if(encodingArg)
            encoding = encodingArg.stringValue();
    }

    Q_ASSERT(uri.isValid() && !uri.isRelative());
    return context->resourceLoader()->openUnparsedText(uri, encoding, context, this);
}

QT_END_NAMESPACE
