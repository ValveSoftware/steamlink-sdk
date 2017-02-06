/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the autotests of the Qt Toolkit.
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

#include <QColor>
#include <QFile>
#include <QFileInfo>
#include <QVariant>
#include <QtDebug>

#include "XSDTSTestCase.h"

#include "qxmlschema.h"
#include "qxmlschemavalidator.h"

using namespace QPatternistSDK;
using namespace QPatternist;

XSDTSTestCase::XSDTSTestCase(const Scenario scen, TreeItem *p, TestType testType)
                            : m_scenario(scen)
                            , m_parent(p)
                            , m_testType(testType)
{
}

XSDTSTestCase::~XSDTSTestCase()
{
    qDeleteAll(m_baseLines);
}

TestResult::List XSDTSTestCase::execute(const ExecutionStage, TestSuite*)
{
    ErrorHandler errHandler;
    ErrorHandler::installQtMessageHandler(&errHandler);

    TestResult::List retval;
    TestResult::Status resultStatus = TestResult::Unknown;
    QString serialized;

    if (m_testType == SchemaTest) {
        executeSchemaTest(resultStatus, serialized, &errHandler);
    } else {
        executeInstanceTest(resultStatus, serialized, &errHandler);
    }

    resultStatus = TestBaseLine::scan(serialized, baseLines());
    Q_ASSERT(resultStatus != TestResult::Unknown);

    m_result = new TestResult(name(), resultStatus, 0, errHandler.messages(),
                              QPatternist::Item::List(), serialized);
    retval.append(m_result);
    ErrorHandler::installQtMessageHandler(0);
    changed(this);
    return retval;
}

void XSDTSTestCase::executeSchemaTest(TestResult::Status &resultStatus, QString &serialized, QAbstractMessageHandler *handler)
{
    QFile file(m_schemaUri.path());
    if (!file.open(QIODevice::ReadOnly)) {
        resultStatus = TestResult::Fail;
        serialized = QString();
        return;
    }

    QXmlSchema schema;
    schema.setMessageHandler(handler);
    schema.load(&file, m_schemaUri);

    if (schema.isValid()) {
        resultStatus = TestResult::Pass;
        serialized = QString::fromLatin1("true");
    } else {
        resultStatus = TestResult::Pass;
        serialized = QString::fromLatin1("false");
    }
}

void XSDTSTestCase::executeInstanceTest(TestResult::Status &resultStatus, QString &serialized, QAbstractMessageHandler *handler)
{
    QFile instanceFile(m_instanceUri.path());
    if (!instanceFile.open(QIODevice::ReadOnly)) {
        resultStatus = TestResult::Fail;
        serialized = QString();
        return;
    }

    QXmlSchema schema;
    if (m_schemaUri.isValid()) {
        QFile file(m_schemaUri.path());
        if (!file.open(QIODevice::ReadOnly)) {
            resultStatus = TestResult::Fail;
            serialized = QString();
            return;
        }

        schema.setMessageHandler(handler);
        schema.load(&file, m_schemaUri);

        if (!schema.isValid()) {
            resultStatus = TestResult::Pass;
            serialized = QString::fromLatin1("false");
            return;
        }
    }

    QXmlSchemaValidator validator(schema);
    validator.setMessageHandler(handler);

    qDebug("check %s", qPrintable(m_instanceUri.path()));
    if (validator.validate(&instanceFile, m_instanceUri)) {
        resultStatus = TestResult::Pass;
        serialized = QString::fromLatin1("true");
    } else {
        resultStatus = TestResult::Pass;
        serialized = QString::fromLatin1("false");
    }
}

QVariant XSDTSTestCase::data(const Qt::ItemDataRole role, int column) const
{
    if(role == Qt::DisplayRole)
    {
        if(column == 0)
            return title();

        const TestResult *const tr = testResult();
        if(!tr)
        {
            if(column == 1)
                return TestResult::displayName(TestResult::NotTested);
            else
                return QString();
        }
        const TestResult::Status status = tr->status();

        switch(column)
        {
            case 1:
                return status == TestResult::Pass ? QString(QChar::fromLatin1('1'))
                                                  : QString(QChar::fromLatin1('0'));
            case 2:
                return status == TestResult::Fail ? QString(QChar::fromLatin1('1'))
                                                  : QString(QChar::fromLatin1('0'));
            default:
                return QString();
        }
    }

    if(role != Qt::BackgroundRole)
        return QVariant();

    const TestResult *const tr = testResult();

    if(!tr)
    {
        if(column == 0)
            return QColor(Qt::yellow);
        else
            return QVariant();
    }

    const TestResult::Status status = tr->status();

    if(status == TestResult::NotTested || status == TestResult::Unknown)
        return QColor(Qt::yellow);

    switch(column)
    {
        case 1:
            return status == TestResult::Pass ? QColor(Qt::green) : QVariant();
        case 2:
            return status == TestResult::Fail ? QColor(Qt::red) : QVariant();
        default:
            return QVariant();
    }
}

QString XSDTSTestCase::sourceCode(bool &ok) const
{
    QFile file((m_testType == SchemaTest ? m_schemaUri : m_instanceUri).toLocalFile());

    QString err;

    if(!file.exists())
        err = QString::fromLatin1("Error: %1 does not exist.").arg(file.fileName());
    else if(!QFileInfo(file.fileName()).isFile())
        err = QString::fromLatin1("Error: %1 is not a file, cannot display it.").arg(file.fileName());
    else if(!file.open(QIODevice::ReadOnly))
        err = QString::fromLatin1("Error: Could not open %1. Likely a permission error.")
                                  .arg(file.fileName());

    if(err.isNull()) /* No errors. */
    {
        ok = true;
        /* Scary, we assume the query is stored in UTF-8. */
        return QString::fromUtf8(file.readAll());
    }
    else
    {
        ok = false;
        return err;
    }
}

int XSDTSTestCase::columnCount() const
{
    return 2;
}

void XSDTSTestCase::addBaseLine(TestBaseLine *line)
{
    m_baseLines.append(line);
}

QString XSDTSTestCase::name() const
{
    return m_name;
}

QString XSDTSTestCase::creator() const
{
    return m_creator;
}

QString XSDTSTestCase::description() const
{
    return m_description;
}

QDate XSDTSTestCase::lastModified() const
{
    return m_lastModified;
}

bool XSDTSTestCase::isXPath() const
{
    return false;
}

TestCase::Scenario XSDTSTestCase::scenario() const
{
    return m_scenario;
}

void XSDTSTestCase::setName(const QString &n)
{
    m_name = n;
}

void XSDTSTestCase::setCreator(const QString &ctor)
{
    m_creator = ctor;
}

void XSDTSTestCase::setDescription(const QString &descriptionP)
{
    m_description = descriptionP;
}

void XSDTSTestCase::setLastModified(const QDate &date)
{
    m_lastModified = date;
}

void XSDTSTestCase::setSchemaUri(const QUrl &uri)
{
    m_schemaUri = uri;
}

void XSDTSTestCase::setInstanceUri(const QUrl &uri)
{
    m_instanceUri = uri;
}

TreeItem *XSDTSTestCase::parent() const
{
    return m_parent;
}

QString XSDTSTestCase::title() const
{
    return m_name;
}

TestBaseLine::List XSDTSTestCase::baseLines() const
{
    Q_ASSERT_X(!m_baseLines.isEmpty(), Q_FUNC_INFO,
               qPrintable(QString::fromLatin1("The test %1 has no base lines, it should have at least one.").arg(name())));
    return m_baseLines;
}

QUrl XSDTSTestCase::schemaUri() const
{
    return m_schemaUri;
}

QUrl XSDTSTestCase::instanceUri() const
{
    return m_instanceUri;
}

void XSDTSTestCase::setContextItemSource(const QUrl &uri)
{
    m_contextItemSource = uri;
}

QUrl XSDTSTestCase::contextItemSource() const
{
    return m_contextItemSource;
}

void XSDTSTestCase::setParent(TreeItem *const p)
{
    m_parent = p;
}

QPatternist::ExternalVariableLoader::Ptr XSDTSTestCase::externalVariableLoader() const
{
    return QPatternist::ExternalVariableLoader::Ptr();
}

TestResult *XSDTSTestCase::testResult() const
{
    return m_result;
}

TestItem::ResultSummary XSDTSTestCase::resultSummary() const
{
    if(m_result)
        return ResultSummary(m_result->status() == TestResult::Pass ? 1 : 0,
                             1);

    return ResultSummary(0, 1);
}

// vim: et:ts=4:sw=4:sts=4

