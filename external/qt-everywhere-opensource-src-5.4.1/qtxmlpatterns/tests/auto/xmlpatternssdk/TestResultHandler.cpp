/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QtDebug>

#include "Global.h"

#include "TestResultHandler.h"

using namespace QPatternistSDK;

TestResultHandler::TestResultHandler()
{
    /* Fifteen thousand. When finished, we squeeze them. */
    m_result.reserve(15000);
    m_comments.reserve(1000); /* Comments are only used for stuff that crash, more or less. */
}

bool TestResultHandler::startElement(const QString &namespaceURI,
                                     const QString &localName,
                                     const QString &,
                                     const QXmlAttributes &atts)
{
    /* We only care about 'test-case', ignore everything else. */
    if(localName != QLatin1String("test-case") ||
       namespaceURI != Global::xqtsResultNS)
        return true;

    /* The 'comments' attribute is optional. */
    Q_ASSERT_X(atts.count() == 2 || atts.count() == 3, Q_FUNC_INFO,
               "The input appears to not conform to XQTSResult.xsd");

    Q_ASSERT_X(!m_result.contains(atts.value(QLatin1String("name"))),
               Q_FUNC_INFO,
               qPrintable(QString::fromLatin1("A test result for test case %1 has "
                                              "already been read(duplicate entry it seems).").arg(atts.value(QLatin1String("name")))));

    m_result.insert(atts.value(0), TestResult::statusFromString(atts.value(QLatin1String("result"))));

    return true;
}

bool TestResultHandler::endDocument()
{
    m_result.squeeze();
    m_comments.squeeze();
    return true;
}

TestResultHandler::Hash TestResultHandler::result() const
{
    return m_result;
}

TestResultHandler::CommentHash TestResultHandler::comments() const
{
    return m_comments;
}

// vim: et:ts=4:sw=4:sts=4
