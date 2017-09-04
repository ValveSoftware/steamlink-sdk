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

#include <QCoreApplication>
#include <QDate>
#include <QUrl>
#include <QVariant>
#include <QtDebug>

#include "TestResult.h"

#include "UserTestCase.h"

using namespace QPatternistSDK;

UserTestCase::UserTestCase() : m_lang(QXmlQuery::XQuery10)
{
}

QVariant UserTestCase::data(const Qt::ItemDataRole role, int /*column*/) const
{
    if(role != Qt::DisplayRole)
        return QVariant();

    return title();
}

QString UserTestCase::creator() const
{
    return QString(QLatin1String("The user of %1."))
                   .arg(QCoreApplication::instance()->applicationName());
}

QString UserTestCase::name() const
{
    return QString(QLatin1String("X-KDE-%1-UserCreated"))
                   .arg(QCoreApplication::instance()->applicationName());
}

QString UserTestCase::description() const
{
    return QLatin1String("No description available; the test case is not part of "
                         "a test suite, but entered manually in the source code window.");
}

QString UserTestCase::title() const
{
    return QLatin1String("User Specified Test");
}

TestCase::Scenario UserTestCase::scenario() const
{
    return Standard;
}

TestBaseLine::List UserTestCase::baseLines() const
{
    TestBaseLine::List retval;

    TestBaseLine *const bl = new TestBaseLine(TestBaseLine::Ignore);
    retval.append(bl);

    return retval;
}

void UserTestCase::setSourceCode(const QString &code)
{
    m_sourceCode = code;
}

QString UserTestCase::sourceCode(bool &ok) const
{
    ok = true;
    return m_sourceCode;
}

QDate UserTestCase::lastModified() const
{
    return QDate();
}

bool UserTestCase::isXPath() const
{
    return true;
}

TreeItem *UserTestCase::parent() const
{
    return 0;
}

int UserTestCase::columnCount() const
{
    return 1;
}

QUrl UserTestCase::testCasePath() const
{
    return QUrl::fromLocalFile(QCoreApplication::applicationDirPath());
}

QPatternist::ExternalVariableLoader::Ptr UserTestCase::externalVariableLoader() const
{
    /* We don't have any bindings for the query that the user writes. */
    return QPatternist::ExternalVariableLoader::Ptr();
}

QUrl UserTestCase::contextItemSource() const
{
    return m_contextSource;
}

void UserTestCase::focusDocumentChanged(const QString &code)
{
    const QUrl focusDoc(code);
    if(focusDoc.isValid())
        m_contextSource = focusDoc;
}

// vim: et:ts=4:sw=4:sts=4

