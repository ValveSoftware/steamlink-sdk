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

#include <QXmlAttributes>
#include <QtDebug>

#include <private/qxmldebug_p.h>
#include "Global.h"
#include "XMLWriter.h"

#include "TestResult.h"

using namespace QPatternistSDK;
using namespace QPatternist;

QString TestResult::displayName(const TestResult::Status stat)
{
    switch(stat)
    {
        case Pass:
            return QLatin1String("pass");
        case Fail:
            return QLatin1String("fail");
        case NotTested:
            return QLatin1String("not tested");
        case Unknown:
            Q_ASSERT(false);
    }

    Q_ASSERT(false);
    return QString();
}

TestResult::Status TestResult::statusFromString(const QString &string)
{
    if(string == QLatin1String("pass"))
        return Pass;
    else if(string == QLatin1String("fail"))
        return Fail;
    else if(string == QLatin1String("not tested"))
        return NotTested;
    else
    {
        Q_ASSERT(false);
        return Fail;
    }
}

TestResult::TestResult(const QString &n,
                       const Status s,
                       ASTItem *tree,
                       const ErrorHandler::Message::List &ers,
                       const QPatternist::Item::List &itemsP,
                       const QString &serialized) : m_status(s),
                                                    m_messages(ers),
                                                    m_astTree(tree),
                                                    m_testName(n),
                                                    m_items(itemsP),
                                                    m_asSerialized(serialized)
{
    Q_ASSERT(!n.isEmpty());
    Q_ASSERT(s != 0);
}

TestResult::~TestResult()
{
    delete m_astTree;
}

void TestResult::toXML(XMLWriter &receiver) const
{
    QXmlAttributes atts;
    atts.append(QLatin1String("name"), QString(), QLatin1String("name"), m_testName);
    atts.append(QLatin1String("result"), QString(), QLatin1String("result"), displayName(m_status));

    if(!m_comment.isEmpty())
        atts.append(QLatin1String("comment"), QString(), QLatin1String("comment"), m_comment);

    receiver.startElement(QLatin1String("test-case"), atts);
    receiver.endElement(QLatin1String("test-case"));
}

void TestResult::setComment(const QString &comm)
{
    m_comment = comm;
}

TestResult::Status TestResult::status() const
{
    return m_status;
}

QString TestResult::comment() const
{
    return m_comment;
}

ASTItem *TestResult::astTree() const
{
    return m_astTree;
}

ErrorHandler::Message::List TestResult::messages() const
{
    return m_messages;
}

QPatternist::Item::List TestResult::items() const
{
    return m_items;
}

QString TestResult::asSerialized() const
{
    pDebug() << "asSerialized: " << m_asSerialized;
    return m_asSerialized;
}

// vim: et:ts=4:sw=4:sts=4

