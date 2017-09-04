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

#include <QBuffer>
#include <QUrl>
#include <QXmlAttributes>
#include <QXmlQuery>
#include <QXmlResultItems>
#include <QXmlSerializer>
#include <private/qxmlquery_p.h>
#include <algorithm>

#include "DebugExpressionFactory.h"
#include "ExternalSourceLoader.h"
#include "Global.h"
#include "TestSuite.h"
#include "XMLWriter.h"

#include "TestCase.h"

using namespace QPatternistSDK;
using namespace QPatternist;

// STATIC DATA
static const DebugExpressionFactory::Ptr s_exprFact(new DebugExpressionFactory());

TestCase::TestCase() : m_result(0)
{
}

TestCase::~TestCase()
{
    delete m_result;
}

static bool lessThan(const char *a, const char *b)
{
    return qstrcmp(a, b) < 0;
}

TestResult::List TestCase::execute(const ExecutionStage stage,
                                   TestSuite *)
{
    ++TestCase::executions;

    if ((TestCase::executions < TestCase::executeRange.first) || (TestCase::executions > TestCase::executeRange.second)) {
        qDebug("Skipping test case #%6d", TestCase::executions);
        return TestResult::List();
    }

    const QByteArray nm = name().toLatin1();

    if(name() == QLatin1String("Constr-cont-document-3"))
    {
            TestResult::List result;
            result.append(createTestResult(TestResult::Fail, QLatin1String("Skipped this test, because we loop infinitely on it.")));
            return result;
    }
    else if(name() == QLatin1String("Axes089"))
    {
            TestResult::List result;
            result.append(createTestResult(TestResult::Fail, QLatin1String("Skipped this test, we crash on it.")));
            return result;
    }
    else if (name() == QLatin1String("op-numeric-unary-minus-1"))
    {
            TestResult::List result;
            result.append(createTestResult(TestResult::Fail, QLatin1String("Skipped this test, we crash on it.")));
            return result;
    }
    else if (name() == QLatin1String("emptyorderdecl-13"))
    {
        TestResult::List result;
        result.append(createTestResult(TestResult::Fail, QLatin1String("Skipped this test, we crash on it.")));
        return result;
    }
    else if (name() == QLatin1String("emptyorderdecl-21"))
    {
        TestResult::List result;
        result.append(createTestResult(TestResult::Fail, QLatin1String("Skipped this test, we crash on it.")));
        return result;
    }
    else {
        // Should be sorted in the order that std::binary_search expects
        static const char *crashes[] = {"Constr-attr-content-4",
                                        "K2-DirectConElem-12",
                                        "K2-DirectConElem-50",
                                        "K2-DirectConElemAttr-10",
                                        "K2-DirectConElemAttr-18",
                                        "K2-DirectConElemAttr-19",
                                        "K2-DirectConElemAttr-20",
                                        "K2-DirectConElemAttr-21"
                                        };

        const bool skip = std::binary_search(&crashes[0], &crashes[sizeof(crashes)/sizeof(crashes[0])], nm.constData(), lessThan);
        if (skip) {
            TestResult::List result;
            result.append(createTestResult(TestResult::Fail, QLatin1String("Skipped this test, we crash on it.")));
            return result;
        }
    }


    qDebug("Running test case #%6d: %s", TestCase::executions, nm.constData());
    return execute(stage);

    Q_ASSERT(false);
    return TestResult::List();
}

TestResult *TestCase::createTestResult(const TestResult::Status status,
                                             const QString &comment) const
{
    TestResult *const result = new TestResult(name(),
                                              status,
                                              0 /* We don't have an AST. */,
                                              ErrorHandler::Message::List(),
                                              QPatternist::Item::List(),
                                              QString());
    result->setComment(comment);
    return result;
}

TestResult::List TestCase::execute(const ExecutionStage stage)
{
    ErrorHandler errHandler;
    ErrorHandler::installQtMessageHandler(&errHandler);

    pDebug() << "TestCase::execute()";
    delete m_result;

    QXmlQuery query(language(), Global::namePoolAsPublic());

    query.d->setExpressionFactory(s_exprFact);
    query.setInitialTemplateName(initialTemplateName());

    QXmlQuery openDoc(query.namePool());

    if(contextItemSource().isValid())
    {
        openDoc.setQuery(QString::fromLatin1("doc('") + contextItemSource().toString() + QLatin1String("')"));
        Q_ASSERT(openDoc.isValid());
        QXmlResultItems result;

        openDoc.evaluateTo(&result);
        const QXmlItem item(result.next());
        Q_ASSERT(!item.isNull());
        query.setFocus(item);
    }

    TestResult::List retval;

    const Scenario scen(scenario());
    TestResult::Status resultStatus = TestResult::Unknown;

    bool ok = false;
    const QString queryString(sourceCode(ok));

    if(!ok)
    {
        /* Loading the query file failed, or similar. */
        resultStatus = TestResult::Fail;

        m_result = new TestResult(name(), resultStatus, s_exprFact->astTree(),
                                  errHandler.messages(), QPatternist::Item::List(), QString());
        retval.append(m_result);
        ErrorHandler::installQtMessageHandler(0);
        changed(this);
        return retval;
    }

    query.setMessageHandler(&errHandler);
    QXmlNamePool namePool(query.namePool());

    /* Bind variables. */
    QPatternist::ExternalVariableLoader::Ptr loader(externalVariableLoader());
    if(loader)
    {
        Q_ASSERT(loader);
        const ExternalSourceLoader::VariableMap vMap(static_cast<const ExternalSourceLoader *>(loader.data())->variableMap());
        const QStringList variables(vMap.keys());

        for(int i = 0; i < variables.count(); ++i)
        {
            const QXmlName name(namePool, variables.at(i));
            const QXmlItem val(QPatternist::Item::toPublic(loader->evaluateSingleton(name, QPatternist::DynamicContext::Ptr())));
            query.bindVariable(name, val);
        }
    }

    /* We pass in the testCasePath(), such that the base URI is correct fort
     * XSL-T stylesheets. */
    query.setQuery(queryString, testCasePath());

    if(!query.isValid())
    {
        pDebug() << "Got compilation exception.";
        resultStatus = TestBaseLine::scanErrors(errHandler.messages(), baseLines());

        Q_ASSERT(resultStatus != TestResult::Unknown);
        m_result = new TestResult(name(), resultStatus, s_exprFact->astTree(),
                                  errHandler.messages(), QPatternist::Item::List(), QString());
        retval.append(m_result);
        ErrorHandler::installQtMessageHandler(0);
        changed(this);
        return retval;
    }

    if(stage == CompileOnly)
    {
        m_result = new TestResult(name(), TestResult::Fail, s_exprFact->astTree(),
                                  errHandler.messages(), QPatternist::Item::List(), QString());
        retval.append(m_result);
        return retval;
    }

    Q_ASSERT(stage == CompileAndRun);

    if(scen == ParseError) /* We're supposed to have received an error
                              at this point. */
    {
        m_result = new TestResult(name(), TestResult::Fail, s_exprFact->astTree(),
                                  errHandler.messages(), QPatternist::Item::List(), QString());
        ErrorHandler::installQtMessageHandler(0);
        retval.append(m_result);
        changed(this);
        return retval;
    }

    QPatternist::Item::List itemList;

    QByteArray output;
    QBuffer buffer(&output);
    buffer.open(QIODevice::WriteOnly);

    QXmlSerializer serializer(query, &buffer);

    pDebug() << "-------------------------- evaluateToPushCallback() ---------------------------- ";
    const bool success = query.evaluateTo(&serializer);
    pDebug() << "------------------------------------------------------------------------------------ ";

    buffer.close();

    const QString serialized(QString::fromUtf8(output.constData(), output.size()));

    if(!success)
    {
        resultStatus = TestBaseLine::scanErrors(errHandler.messages(), baseLines());

        Q_ASSERT(resultStatus != TestResult::Unknown);
        m_result = new TestResult(name(), resultStatus, s_exprFact->astTree(),
                                  errHandler.messages(), QPatternist::Item::List(), serialized);
        retval.append(m_result);
        ErrorHandler::installQtMessageHandler(0);
        changed(this);
        return retval;
    }

    /* It's a regular test. */
    Q_ASSERT(scen == Standard || scen == RuntimeError);

    resultStatus = TestBaseLine::scan(serialized, baseLines());
    Q_ASSERT(resultStatus != TestResult::Unknown);

    /* Check that errHandler()->messages() at most only contains
     * warnings, since it shouldn't have errors at this point. */
    const ErrorHandler::Message::List errors (errHandler.messages());
    const ErrorHandler::Message::List::const_iterator end(errors.constEnd());
    ErrorHandler::Message::List::const_iterator it(errors.constBegin());

    for(; it != end; ++it)
    {
        const QtMsgType type = (*it).type();
        if(type == QtFatalMsg)
        {
            m_result = new TestResult(name(), TestResult::Fail, s_exprFact->astTree(),
                                      errHandler.messages(), itemList, serialized);
            retval.append(m_result);
            ErrorHandler::installQtMessageHandler(0);
            changed(this);
            return retval;
        }
    }

    m_result = new TestResult(name(), resultStatus, s_exprFact->astTree(),
                              errHandler.messages(), itemList, serialized);
    retval.append(m_result);
    ErrorHandler::installQtMessageHandler(0);
    changed(this);
    return retval;
}

TestCase::Scenario TestCase::scenarioFromString(const QString &string)
{
    if(string == QLatin1String("standard"))
        return Standard;
    else if(string == QLatin1String("parse-error"))
        return ParseError;
    else if(string == QLatin1String("runtime-error"))
        return RuntimeError;
    else if(string == QLatin1String("trivial"))
        return Trivial;
    else
    {
        Q_ASSERT_X(false, Q_FUNC_INFO,
                   qPrintable(QString::fromLatin1("Invalid string representation for the scenario-enum: %1").arg(string)));
        return ParseError; /* Silence GCC. */
    }
}

void TestCase::toXML(XMLWriter &receiver) const
{
    /* <test-case> */
    QXmlAttributes test_caseAtts;
    test_caseAtts.append(QLatin1String("is-XPath2"), QString(),
                         QLatin1String("is-XPath2"), isXPath() ? QLatin1String("true")
                                                               : QLatin1String("false"));
    test_caseAtts.append(QLatin1String("name"), QString(), QLatin1String("name"), name());
    test_caseAtts.append(QLatin1String("creator"), QString(), QLatin1String("creator"), creator());
    QString scen;
    switch(scenario())
    {
        case Standard:
        {
            scen = QLatin1String("standard");
            break;
        }
        case ParseError:
        {
            scen = QLatin1String("parse-error");
            break;
        }
        case RuntimeError:
        {
            scen = QLatin1String("runtime-error");
            break;
        }
        case Trivial:
        {
            scen = QLatin1String("trivial");
            break;
        }
        default: /* includes 'AnyError' */
            Q_ASSERT(false);
    }
    test_caseAtts.append(QLatin1String("scenario"), QString(), QLatin1String("scenario"), scen);
    test_caseAtts.append(QLatin1String(QLatin1String("FilePath")), QString(),
                         QLatin1String("FilePath"), QString());
    receiver.startElement(QLatin1String("test-case"), test_caseAtts);

    /* <description> */
    receiver.startElement(QLatin1String("description"), test_caseAtts);
    receiver.characters(description());

    /* </description> */
    receiver.endElement(QLatin1String("description"));

    /* <query> */
    QXmlAttributes queryAtts;
    queryAtts.append(QLatin1String("date"), QString(), QLatin1String("date"), /* This date is a dummy. */
                     QDate::currentDate().toString(Qt::ISODate));
    queryAtts.append(QLatin1String("name"), QString(), QLatin1String("name"), testCasePath().toString());
    receiver.startElement(QLatin1String("query"), queryAtts);

    /* </query> */
    receiver.endElement(QLatin1String("query"));

    /* Note: this is invalid, we don't add spec-citation. */
    TestBaseLine::List bls(baseLines());
    const TestBaseLine::List::const_iterator end(bls.constEnd());
    TestBaseLine::List::const_iterator it(bls.constBegin());

    for(; it != end; ++it)
        (*it)->toXML(receiver);

    /* </test-case> */
    receiver.endElement(QLatin1String("test-case"));
}

QString TestCase::displayName(const Scenario scen)
{
    switch(scen)
    {
        case Standard:
            return QLatin1String("Standard");
        case ParseError:
            return QLatin1String("Parse Error");
        case RuntimeError:
            return QLatin1String("Runtime Error");
        case Trivial:
            return QLatin1String("Trivial");
        case AnyError:
        {
            Q_ASSERT(false);
            return QString();
        }
    }

    Q_ASSERT(false);
    return QString();
}

TestItem::ResultSummary TestCase::resultSummary() const
{
    if(m_result)
        return ResultSummary(m_result->status() == TestResult::Pass ? 1 : 0,
                             1);

    return ResultSummary(0, 1);
}

void TestCase::appendChild(TreeItem *)
{
    Q_ASSERT_X(false, Q_FUNC_INFO, "Makes no sense to call appendChild() for TestCase.");
}

TreeItem *TestCase::child(const unsigned int) const
{
    return 0; /* Silence GCC */
}

TreeItem::List TestCase::children() const
{
    return TreeItem::List();
}

unsigned int TestCase::childCount() const
{
    return 0;
}

TestResult *TestCase::testResult() const
{
    return m_result;
}

bool TestCase::isFinalNode() const
{
    return true;
}

QXmlQuery::QueryLanguage TestCase::language() const
{
    return QXmlQuery::XQuery10;
}

QXmlName TestCase::initialTemplateName() const
{
    return QXmlName();
}

// vim: et:ts=4:sw=4:sts=4

