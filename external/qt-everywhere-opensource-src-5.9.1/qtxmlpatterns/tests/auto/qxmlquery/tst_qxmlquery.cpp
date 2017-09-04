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


#include <QtTest/QtTest>

#include <QAbstractMessageHandler>
#include <QFileInfo>
#include <QNetworkReply>
#include <QtNetwork/QTcpServer>
#include <QtNetwork/QTcpSocket>
#include <QXmlFormatter>
#include <QXmlItem>
#include <QXmlName>
#include <QXmlQuery>
#include <QXmlResultItems>
#include <QXmlSerializer>

#include "MessageSilencer.h"
#include "MessageValidator.h"
#include "NetworkOverrider.h"
#include "PushBaseliner.h"
#include "../qabstracturiresolver/TestURIResolver.h"
#include "../qsimplexmlnodemodel/TestSimpleNodeModel.h"
#include "TestFundament.h"
#include "../network-settings.h"

/*!
 \class tst_QXmlQuery
 \internal
 \since 4.4
 \brief Tests class QXmlQuery.

 This test is not intended for testing the engine, but the functionality specific
 to the QXmlQuery class.

 In other words, if you have an engine bug; don't add it here because it won't be
 tested properly. Instead add it to the test suite.

 */
class tst_QXmlQuery : public QObject
                    , private TestFundament
{
    Q_OBJECT

public:
    inline tst_QXmlQuery() : m_generatedBaselines(0)
                           , m_pushTestsCount(0)
                           , m_testNetwork(true)
    {
    }

private Q_SLOTS:
    void initTestCase();
    void defaultConstructor() const;
    void copyConstructor() const;
    void constructorQXmlNamePool() const;
    void constructorQXmlNamePoolQueryLanguage() const;
    void constructorQXmlNamePoolWithinQSimpleXmlNodeModel() const;
    void assignmentOperator() const;
    void isValid() const;
    void sequentialExecution() const;
    void bindVariableQString() const;
    void bindVariableQStringNoExternalDeclaration() const;
    void bindVariableQXmlName() const;
    void bindVariableQXmlNameTriggerWarnings() const;
    void bindVariableQStringQIODevice() const;
    void bindVariableQStringQIODeviceWithByteArray() const;
    void bindVariableQStringQIODeviceWithString() const;
    void bindVariableQStringQIODeviceWithQFile() const;
    void bindVariableQXmlNameQIODevice() const;
    void bindVariableQXmlNameQIODeviceTriggerWarnings() const;
    void bindVariableXSLTSuccess() const;
    void bindVariableTemporaryNode() const;
    void setMessageHandler() const;
    void messageHandler() const;
    void evaluateToQAbstractXmlReceiverTriggerWarnings() const;
    void evaluateToQXmlResultItems() const;
    void evaluateToQXmlResultItemsTriggerWarnings() const;
    void evaluateToQXmlResultItemsErrorAtEnd() const;
    void evaluateToReceiver();
    void evaluateToReceiver_data() const;
    void evaluateToReceiverOnInvalidQuery() const;
    void evaluateToQStringTriggerError() const;
    void evaluateToQString() const;
    void evaluateToQString_data() const;
    void evaluateToQStringSignature() const;
    void checkGeneratedBaselines() const;
    void basicXQueryToQtTypeCheck() const;
    void basicQtToXQueryTypeCheck() const;
    void bindNode() const;
    void relativeBaseURI() const;
    void emptyBaseURI() const;
    void roundTripDateWithinQXmlItem() const;
    void bindingMissing() const;
    void bindDefaultConstructedItem() const;
    void bindDefaultConstructedItem_data() const;
    void bindEmptyNullString() const;
    void bindEmptyString() const;
    void rebindVariableSameType() const;
    void rebindVariableDifferentType() const;
    void rebindVariableWithNullItem() const;
    void eraseQXmlItemBinding() const;
    void eraseDeviceBinding() const;
    void constCorrectness() const;
    void objectSize() const;
    void setUriResolver() const;
    void uriResolver() const;
    void messageXML() const;
    void resultItemsDeallocatedQuery() const;
    void copyCheckMessageHandler() const;
    void shadowedVariables() const;
    void setFocusQXmlItem() const;
    void setFocusQUrl() const;
    void setFocusQIODevice() const;
    void setFocusQIODeviceAvoidVariableClash() const;
    void setFocusQIODeviceFailure() const;
    void setFocusQIODeviceTriggerWarnings() const;
    void setFocusQString() const;
    void setFocusQStringFailure() const;
    void setFocusQStringSignature() const;
    void recompilationWithEvaluateToResultFailing() const;
    void secondEvaluationWithEvaluateToResultFailing() const;
    void recompilationWithEvaluateToReceiver() const;
    void fnDocOnQIODeviceTimeout() const;
    void evaluateToQStringListOnInvalidQuery() const;
    void evaluateToQStringList() const;
    void evaluateToQStringListTriggerWarnings() const;
    void evaluateToQStringList_data() const;
    void evaluateToQStringListNoConversion() const;
    void evaluateToQIODevice() const;
    void evaluateToQIODeviceTriggerWarnings() const;
    void evaluateToQIODeviceSignature() const;
    void evaluateToQIODeviceOnInvalidQuery() const;
    void setQueryQIODeviceQUrl() const;
    void setQueryQIODeviceQUrlTriggerWarnings() const;
    void setQueryQString() const;
    void setQueryQUrlSuccess() const;
    void setQueryQUrlSuccess_data() const;
    void setQueryQUrlFailSucceed() const;
    void setQueryQUrlFailure() const;
    void setQueryQUrlFailure_data() const;
    void setQueryQUrlBaseURI() const;
    void setQueryQUrlBaseURI_data() const;
    void setQueryWithNonExistentQUrlOnValidQuery() const;
    void setQueryWithInvalidQueryFromQUrlOnValidQuery() const;
    void retrieveNameFromQuery() const;
    void retrieveNameFromQuery_data() const;
    void cleanupTestCase() const;
    void declareUnavailableExternal() const;
    void msvcCacheIssue() const;
    void unavailableExternalVariable() const;
    void useUriResolver() const;
    void queryWithFocusAndVariable() const;
    void undefinedFocus() const;
    void basicFocusUsage() const;

    void queryLanguage() const;
    void queryLanguageSignature() const;
    void enumQueryLanguage() const;

    void setNetworkAccessManager() const;
    void networkAccessManagerSignature() const;
    void networkAccessManagerDefaultValue() const;
    void networkAccessManager() const;

    void setInitialTemplateNameQXmlName() const;
    void setInitialTemplateNameQXmlNameSignature() const;
    void setInitialTemplateNameQString() const;
    void setInitialTemplateNameQStringSignature() const;
    void initialTemplateName() const;
    void initialTemplateNameSignature() const;

    void fnDocNetworkAccessSuccess() const;
    void fnDocNetworkAccessSuccess_data() const;
    void fnDocNetworkAccessFailure() const;
    void fnDocNetworkAccessFailure_data() const;
    void multipleDocsAndFocus() const;
    void multipleEvaluationsWithDifferentFocus() const;
    void bindVariableQXmlQuery() const;
    void bindVariableQXmlQuery_data() const;
    void bindVariableQStringQXmlQuerySignature() const;
    void bindVariableQXmlNameQXmlQuerySignature() const;
    void bindVariableQXmlNameQXmlQuery() const;
    void bindVariableQXmlQueryInvalidate() const;
    void unknownSourceLocation() const;

    void identityConstraintSuccess() const;
    void identityConstraintFailure() const;
    void identityConstraintFailure_data() const;

    // TODO call all URI resolving functions where 1) the URI resolver return a null QUrl(); 2) resolves into valid, existing URI, 3) invalid, non-existing URI.
    // TODO bind stringlists, variant lists, both ways.
    // TODO trigger serialization error, or any error in evaluateToushCallback().
    // TODO let items travle between two queries, as seen in the SDK
    // TODO what happens if the query declares local variable and external ones are provided?

private:
    enum
    {
        /**
         * One excluded, since we skip static-base-uri.xq.
         */
        ExpectedQueryCount = 30
    };

    static void checkBaseURI(const QUrl &baseURI, const QString &candidate);
    static QStringList queries();
    static const QString m_xmlPatternsDir;

    int m_generatedBaselines;
    int m_pushTestsCount;
    const bool m_testNetwork;
};

const QString tst_QXmlQuery::m_xmlPatternsDir = QFINDTESTDATA("../xmlpatterns");

void tst_QXmlQuery::initTestCase()
{
    QVERIFY2(!m_xmlPatternsDir.isEmpty(), qPrintable(QString::fromLatin1("Cannot locate '../xmlpatterns' starting from %1").arg(QDir::currentPath())));
}

void tst_QXmlQuery::checkBaseURI(const QUrl &baseURI, const QString &candidate)
{
    /* The use of QFileInfo::canonicalFilePath() takes into account that drive letters
     * on Windows may have different cases. */
    QVERIFY(QDir(baseURI.toLocalFile()).relativeFilePath(QFileInfo(candidate).canonicalFilePath()).startsWith("../"));
}

QStringList tst_QXmlQuery::queries()
{
    QDir dir;
    dir.cd(inputFile(m_xmlPatternsDir + QLatin1String("/queries/")));

    return dir.entryList(QStringList(QLatin1String("*.xq")));
}

void tst_QXmlQuery::defaultConstructor() const
{
    /* Allocate instance in different orders. */
    {
        QXmlQuery query;
    }

    {
        QXmlQuery query1;
        QXmlQuery query2;
    }

    {
        QXmlQuery query1;
        QXmlQuery query2;
        QXmlQuery query3;
    }
}

void tst_QXmlQuery::copyConstructor() const
{
    /* Verify that we can take a const reference, and simply do a copy of a default constructed object. */
    {
        const QXmlQuery query1;
        QXmlQuery query2(query1);
    }

    /* Copy twice. */
    {
        const QXmlQuery query1;
        QXmlQuery query2(query1);
        QXmlQuery query3(query2);
    }

    /* Verify that copying default values works. */
    {
        const QXmlQuery query1;
        const QXmlQuery query2(query1);
        QCOMPARE(query2.messageHandler(), query1.messageHandler());
        QCOMPARE(query2.uriResolver(), query1.uriResolver());
        QCOMPARE(query2.queryLanguage(), query1.queryLanguage());
        QCOMPARE(query2.initialTemplateName(), query1.initialTemplateName());
        QCOMPARE(query2.networkAccessManager(), query1.networkAccessManager());
    }

    /* Check that the
     *
     * - name pool
     * - URI resolver
     * - message handler
     * - query language
     * - initial template name
     *
     *   sticks with the copy. */
    {
        MessageSilencer silencer;
        TestURIResolver resolver;
        QNetworkAccessManager networkManager;
        QXmlQuery query1(QXmlQuery::XSLT20);
        QXmlNamePool np1(query1.namePool());

        query1.setMessageHandler(&silencer);
        query1.setUriResolver(&resolver);
        query1.setNetworkAccessManager(&networkManager);

        const QXmlName name(np1, QLatin1String("localName"),
                                 QLatin1String("http://example.com/"),
                                 QLatin1String("prefix"));
        query1.setInitialTemplateName(name);

        const QXmlQuery query2(query1);
        QCOMPARE(query2.messageHandler(), static_cast<QAbstractMessageHandler *>(&silencer));
        QCOMPARE(query2.uriResolver(), static_cast<const QAbstractUriResolver *>(&resolver));
        QCOMPARE(query2.queryLanguage(), QXmlQuery::XSLT20);
        QCOMPARE(query2.initialTemplateName(), name);
        QCOMPARE(query2.networkAccessManager(), &networkManager);

        QXmlNamePool np2(query2.namePool());

        QCOMPARE(name.namespaceUri(np2), QString::fromLatin1("http://example.com/"));
        QCOMPARE(name.localName(np2), QString::fromLatin1("localName"));
        QCOMPARE(name.prefix(np2), QString::fromLatin1("prefix"));
    }

    {
        QXmlQuery original;

        original.setFocus(QXmlItem(4));
        original.setQuery(QLatin1String("."));
        QVERIFY(original.isValid());

        const QXmlQuery copy(original);

        QXmlResultItems result;
        copy.evaluateTo(&result);
        QCOMPARE(result.next().toAtomicValue(), QVariant(4));
        QVERIFY(result.next().isNull());
        QVERIFY(!result.hasError());
    }

    /* Copy, set, compare. Check that copies are independent. */
    {
        // TODO all members except queryLanguage().
    }
}

void tst_QXmlQuery::constructorQXmlNamePool() const
{
    /* Check that the namepool we are passed, is actually used. */
    QXmlNamePool np;

    QXmlQuery query(np);
    const QXmlName name(np, QLatin1String("localName"),
                            QLatin1String("http://example.com/"),
                            QLatin1String("prefix"));

    QXmlNamePool np2(query.namePool());
    QCOMPARE(name.namespaceUri(np2), QString::fromLatin1("http://example.com/"));
    QCOMPARE(name.localName(np2), QString::fromLatin1("localName"));
    QCOMPARE(name.prefix(np2), QString::fromLatin1("prefix"));
}

/*!
  Ensure that the internal variable loading mechanisms uses the user-supplied
  name pool.

  If that is not the case, different name pools are used and the code crashes.

  \since 4.5
 */
void tst_QXmlQuery::constructorQXmlNamePoolQueryLanguage() const
{
    QXmlNamePool np;
    QXmlName name(np, QLatin1String("arbitraryName"));

    QXmlQuery query(QXmlQuery::XQuery10, np);

    QBuffer input;
    input.setData("<yall/>");

    QVERIFY(input.open(QIODevice::ReadOnly));
    query.bindVariable(name, &input);
    query.setQuery("string(doc($arbitraryName))");

    QStringList result;
    query.evaluateTo(&result);
}

void tst_QXmlQuery::constructorQXmlNamePoolWithinQSimpleXmlNodeModel() const
{
    class TestCTOR : public TestSimpleNodeModel
    {
    public:
        TestCTOR(const QXmlNamePool &np) : TestSimpleNodeModel(np)
        {
        }

        void checkCTOR() const
        {
            /* If this fails to compile, the constructor has trouble with taking
             * a mutable reference.
             *
             * The reason we use the this pointer explicitly, is to avoid a compiler
             * warnings with MSVC 2005. */
            QXmlQuery(this->namePool());
        }
    };

    QXmlNamePool np;
    TestCTOR ctor(np);
    ctor.checkCTOR();
}

void tst_QXmlQuery::assignmentOperator() const
{
    class ReturnURI : public QAbstractUriResolver
    {
    public:
        ReturnURI() {}
        virtual QUrl resolve(const QUrl &relative,
                             const QUrl &baseURI) const
        {
            return baseURI.resolved(relative);
        }
    };

    /* Assign this to this. */
    {
        QXmlQuery query;
        query = query;
        query = query;
        query = query;

        /* Just call a couple of functions to give valgrind
         * something to check. */
        QVERIFY(!query.isValid());
        query.messageHandler();
    }

    /* Assign null instances a couple of times. */
    {
        QXmlQuery query1;
        QXmlQuery query2;
        query1 = query2;
        query1 = query2;
        query1 = query2;

        /* Just call a couple of functions to give valgrind
         * something to check. */
        QVERIFY(!query1.isValid());
        query1.messageHandler();

        /* Just call a couple of functions to give valgrind
         * something to check. */
        QVERIFY(!query2.isValid());
        query2.messageHandler();
    }

    /* Create a query, set all the things it stores, and ensure it
     * travels over to the new instance. */
    {
        MessageSilencer silencer;
        const ReturnURI returnURI;
        QXmlNamePool namePool;

        QBuffer documentDevice;
        documentDevice.setData(QByteArray("<e>a</e>"));
        QVERIFY(documentDevice.open(QIODevice::ReadOnly));

        QXmlQuery original(namePool);
        QXmlName testName(namePool, QLatin1String("somethingToCheck"));

        original.setMessageHandler(&silencer);
        original.bindVariable(QLatin1String("var"), QXmlItem(1));
        original.bindVariable(QLatin1String("device"), &documentDevice);
        original.setUriResolver(&returnURI);
        original.setFocus(QXmlItem(3));
        original.setQuery(QLatin1String("$var, 1 + 1, ., string(doc($device))"));

        /* Do a copy, and check that everything followed on into the copy. No modification
         * of the copy. */
        {
            QXmlQuery copy;

            /* We use assignment operator, not copy constructor. */
            copy = original;

            QVERIFY(copy.isValid());
            QCOMPARE(copy.uriResolver(), static_cast<const QAbstractUriResolver *>(&returnURI));
            QCOMPARE(copy.messageHandler(), static_cast<QAbstractMessageHandler *>(&silencer));
            QCOMPARE(testName.localName(copy.namePool()), QString::fromLatin1("somethingToCheck"));

            QXmlResultItems result;
            copy.evaluateTo(&result);
            QCOMPARE(result.next().toAtomicValue(), QVariant(1));
            QCOMPARE(result.next().toAtomicValue(), QVariant(2));
            QCOMPARE(result.next().toAtomicValue(), QVariant(3));
            QCOMPARE(result.next().toAtomicValue(), QVariant(QString::fromLatin1("a")));
            QVERIFY(result.next().isNull());
            QVERIFY(!result.hasError());
        }

        /* Copy, and change values. Things should detach. */
        {
            /* Evaluate the copy. */
            {
                MessageSilencer secondSilencer;
                const ReturnURI secondUriResolver;
                QBuffer documentDeviceCopy;
                documentDeviceCopy.setData(QByteArray("<e>b</e>"));
                QVERIFY(documentDeviceCopy.open(QIODevice::ReadOnly));

                QXmlQuery copy;
                copy = original;

                copy.setMessageHandler(&secondSilencer);
                /* Here we rebind variable values. */
                copy.bindVariable(QLatin1String("var"), QXmlItem(4));
                copy.bindVariable(QLatin1String("device"), &documentDeviceCopy);
                copy.setUriResolver(&secondUriResolver);
                copy.setFocus(QXmlItem(6));
                copy.setQuery(QLatin1String("$var, 1 + 1, ., string(doc($device))"));

                /* Check that the copy picked up the new things. */
                QVERIFY(copy.isValid());
                QCOMPARE(copy.uriResolver(), static_cast<const QAbstractUriResolver *>(&secondUriResolver));
                QCOMPARE(copy.messageHandler(), static_cast<QAbstractMessageHandler *>(&secondSilencer));

                QXmlResultItems resultCopy;
                copy.evaluateTo(&resultCopy);
                QCOMPARE(resultCopy.next().toAtomicValue(), QVariant(4));
                QCOMPARE(resultCopy.next().toAtomicValue(), QVariant(2));
                QCOMPARE(resultCopy.next().toAtomicValue(), QVariant(6));
                const QString stringedDevice(resultCopy.next().toAtomicValue().toString());
                QCOMPARE(stringedDevice, QString::fromLatin1("b"));
                QVERIFY(resultCopy.next().isNull());
                QVERIFY(!resultCopy.hasError());
            }

            /* Evaluate the original. */
            {
                /* Check that the original is unchanged. */
                QVERIFY(original.isValid());
                QCOMPARE(original.uriResolver(), static_cast<const QAbstractUriResolver *>(&returnURI));
                QCOMPARE(original.messageHandler(), static_cast<QAbstractMessageHandler *>(&silencer));

                QXmlResultItems resultOriginal;
                original.evaluateTo(&resultOriginal);
                QCOMPARE(resultOriginal.next().toAtomicValue(), QVariant(1));
                QCOMPARE(resultOriginal.next().toAtomicValue(), QVariant(2));
                QCOMPARE(resultOriginal.next().toAtomicValue(), QVariant(3));
                QCOMPARE(resultOriginal.next().toAtomicValue(), QVariant(QString::fromLatin1("a")));
                QVERIFY(resultOriginal.next().isNull());
                QVERIFY(!resultOriginal.hasError());
            }
        }
    }
}

/*!
  Since QXmlQuery doesn't seek devices to position 0, this code triggers a bug
  where document caching doesn't work. Since the document caching doesn't work,
  the device will be read twice, and the second time the device is at the end,
  hence premature end of document.
 */
void tst_QXmlQuery::sequentialExecution() const
{
    QBuffer inBuffer;
    inBuffer.setData(QByteArray("<input/>"));
    QVERIFY(inBuffer.open(QIODevice::ReadOnly));

    QXmlQuery query;
    query.bindVariable("inputDocument", &inBuffer);

    QByteArray outArray;
    QBuffer outBuffer(&outArray);
    outBuffer.open(QIODevice::WriteOnly);

    const QString queryString(QLatin1String("doc($inputDocument)"));
    query.setQuery(queryString);

    QXmlFormatter formatter(query, &outBuffer);

    QVERIFY(query.evaluateTo(&formatter));

    /* If this line is removed, the bug isn't triggered. */
    query.setQuery(queryString);

    QVERIFY(query.evaluateTo(&formatter));
}

void tst_QXmlQuery::isValid() const
{
    /* Check default value. */
    QXmlQuery query;
    QVERIFY(!query.isValid());
}

void tst_QXmlQuery::bindVariableQString() const
{
    {
        QXmlQuery query;
        /* Bind with a null QXmlItem. */
        query.bindVariable(QLatin1String("name"), QXmlItem());
    }

    {
        QXmlQuery query;
        /* Bind with a null QVariant. */
        query.bindVariable(QLatin1String("name"), QXmlItem(QVariant()));
    }

    {
        QXmlQuery query;
        /* Bind with a null QXmlNodeModelIndex. */
        query.bindVariable(QLatin1String("name"), QXmlItem(QXmlNodeModelIndex()));
    }
}

void tst_QXmlQuery::bindVariableQStringNoExternalDeclaration() const
{
    QXmlQuery query;
    query.bindVariable(QLatin1String("foo"), QXmlItem(QLatin1String("Variable Value")));
    query.setQuery(QLatin1String("$foo"));

    QVERIFY(query.isValid());

    QStringList result;
    QVERIFY(query.evaluateTo(&result));

    QCOMPARE(result, QStringList() << QLatin1String("Variable Value"));
}

void tst_QXmlQuery::bindVariableQXmlName() const
{
    // TODO
}

void tst_QXmlQuery::bindVariableQXmlNameTriggerWarnings() const
{
    QXmlQuery query;

    QTest::ignoreMessage(QtWarningMsg, "The variable name cannot be null.");
    query.bindVariable(QXmlName(), QVariant());
}

void tst_QXmlQuery::bindVariableQStringQIODeviceWithByteArray() const
{
    QXmlQuery query;

    QByteArray in("<e/>");
    QBuffer device(&in);
    QVERIFY(device.open(QIODevice::ReadOnly));

    query.bindVariable("doc", &device);

    query.setQuery(QLatin1String("declare variable $doc external; $doc"));

    QVERIFY(query.isValid());

    /* Check the URI corresponding to the variable. */
    {
        QXmlResultItems items;
        query.evaluateTo(&items);

        QCOMPARE(items.next().toAtomicValue().toString(), QString::fromLatin1("tag:trolltech.com,2007:QtXmlPatterns:QIODeviceVariable:doc"));
    }

    /* Now, actually load the document. We use the same QXmlQuery just to stress recompilation a bit. */
    {
        query.setQuery(QLatin1String("declare variable $doc external; doc($doc)"));

        QByteArray out;
        QBuffer outBuffer(&out);
        QVERIFY(outBuffer.open(QIODevice::WriteOnly));

        QXmlSerializer serializer(query, &outBuffer);

        QVERIFY(query.evaluateTo(&serializer));
        QCOMPARE(out, in);
    }
}

void tst_QXmlQuery::bindVariableQStringQIODeviceWithString() const
{
    QXmlQuery query;

    QString in("<qstring/>");
    QByteArray inUtf8(in.toUtf8());
    QBuffer inDevice(&inUtf8);

    QVERIFY(inDevice.open(QIODevice::ReadOnly));

    query.bindVariable("doc", &inDevice);

    query.setQuery(QLatin1String("declare variable $doc external; doc($doc)"));

    QByteArray out;
    QBuffer outBuffer(&out);
    QVERIFY(outBuffer.open(QIODevice::WriteOnly));

    QXmlSerializer serializer(query, &outBuffer);
    QVERIFY(query.evaluateTo(&serializer));

    QCOMPARE(out, inUtf8);
}

void tst_QXmlQuery::bindVariableQStringQIODeviceWithQFile() const
{
    QXmlQuery query;
    QFile inDevice(QFINDTESTDATA("input.xml"));

    QVERIFY(inDevice.open(QIODevice::ReadOnly));

    query.bindVariable("doc", &inDevice);

    query.setQuery(QLatin1String("declare variable $doc external; doc($doc)"));

    QByteArray out;
    QBuffer outBuffer(&out);
    QVERIFY(outBuffer.open(QIODevice::WriteOnly));

    QXmlSerializer serializer(query, &outBuffer);
    QVERIFY(query.evaluateTo(&serializer));
    outBuffer.close();

    QCOMPARE(out, QByteArray("<!-- This is just a file for testing. --><input/>"));
}

void tst_QXmlQuery::bindVariableQStringQIODevice() const
{
    QXmlQuery query;

    /* Rebind the variable. */
    {
        /* First evaluation. */
        {
            QByteArray in1("<e1/>");
            QBuffer inDevice1(&in1);
            QVERIFY(inDevice1.open(QIODevice::ReadOnly));

            query.bindVariable("in", &inDevice1);
            query.setQuery(QLatin1String("doc($in)"));

            QByteArray out1;
            QBuffer outDevice1(&out1);
            QVERIFY(outDevice1.open(QIODevice::WriteOnly));

            QXmlSerializer serializer(query, &outDevice1);
            query.evaluateTo(&serializer);
            QCOMPARE(out1, in1);
        }

        /* Second evaluation, rebind variable. */
        {
            QByteArray in2("<e2/>");
            QBuffer inDevice2(&in2);
            QVERIFY(inDevice2.open(QIODevice::ReadOnly));

            query.bindVariable(QLatin1String("in"), &inDevice2);

            QByteArray out2;
            QBuffer outDevice2(&out2);
            QVERIFY(outDevice2.open(QIODevice::WriteOnly));

            QXmlSerializer serializer(query, &outDevice2);
            QVERIFY(query.evaluateTo(&serializer));
            QCOMPARE(out2, in2);
        }
    }

    // TODO trigger recompilation when setting qiodevices., and qiodevice overwritten by other type, etc.
}

void tst_QXmlQuery::bindVariableQXmlNameQIODevice() const
{
    // TODO
}

void tst_QXmlQuery::bindVariableQXmlNameQIODeviceTriggerWarnings() const
{
    QXmlNamePool np;
    QXmlQuery query(np);

    QBuffer buffer;
    QTest::ignoreMessage(QtWarningMsg, "A null, or readable QIODevice must be passed.");
    query.bindVariable(QXmlName(np, QLatin1String("foo")), &buffer);

    QTest::ignoreMessage(QtWarningMsg, "The variable name cannot be null.");
    query.bindVariable(QXmlName(), 0);
}

void tst_QXmlQuery::bindVariableXSLTSuccess() const
{
    QXmlQuery stylesheet(QXmlQuery::XSLT20);
    stylesheet.setInitialTemplateName(QLatin1String("main"));

    stylesheet.bindVariable(QLatin1String("variableNoSelectNoBodyBoundWithBindVariable"),
                                          QVariant(QLatin1String("MUST NOT SHOW 1")));

    stylesheet.bindVariable(QLatin1String("variableSelectBoundWithBindVariable"),
                                          QVariant(QLatin1String("MUST NOT SHOW 2")));

    stylesheet.bindVariable(QLatin1String("variableSelectWithTypeIntBoundWithBindVariable"),
                                          QVariant(QLatin1String("MUST NOT SHOW 3")));

    stylesheet.bindVariable(QLatin1String("paramNoSelectNoBodyBoundWithBindVariable"),
                                          QVariant(QLatin1String("param1")));

    stylesheet.bindVariable(QLatin1String("paramNoSelectNoBodyBoundWithBindVariableRequired"),
                                          QVariant(QLatin1String("param1")));

    stylesheet.bindVariable(QLatin1String("paramSelectBoundWithBindVariable"),
                                          QVariant(QLatin1String("param2")));

    stylesheet.bindVariable(QLatin1String("paramSelectBoundWithBindVariableRequired"),
                                          QVariant(QLatin1String("param3")));

    stylesheet.bindVariable(QLatin1String("paramSelectWithTypeIntBoundWithBindVariable"),
                                          QVariant(4));

    stylesheet.bindVariable(QLatin1String("paramSelectWithTypeIntBoundWithBindVariableRequired"),
                                          QVariant(QLatin1String("param5")));

    stylesheet.setQuery(QUrl(inputFileAsURI(m_xmlPatternsDir + QLatin1String("/stylesheets/parameters.xsl"))));

    QVERIFY(stylesheet.isValid());

    QBuffer deviceOut;
    QVERIFY(deviceOut.open(QIODevice::ReadWrite));

    QVERIFY(stylesheet.evaluateTo(&deviceOut));

    const QString result(QString::fromUtf8(deviceOut.data().constData()));

    QCOMPARE(result,
             QString::fromLatin1("Variables:   variableSelectsDefaultValue variableSelectsDefaultValue2 3 4 "
                                 "Parameters: param1 param1 param2 param3 4 param5"));
}

void tst_QXmlQuery::bindVariableTemporaryNode() const
{
    /* First we do it with QXmlResultItems staying in scope. */;
    {
        QXmlQuery query1;
        query1.setQuery("<anElement/>");

        QXmlResultItems result1;
        query1.evaluateTo(&result1);

        QXmlQuery query2(query1);
        query2.bindVariable("fromQuery1", result1.next());
        query2.setQuery("$fromQuery1");

        QString output;
        QVERIFY(query2.evaluateTo(&output));

        QCOMPARE(output, QString::fromLatin1("<anElement/>\n"));
    }

    /* And now with it deallocating, so its internal DynamicContext pointer is
     * released. This doesn't work in Qt 4.5 and is ok. */
    {
        QXmlQuery query1;
        query1.setQuery("<anElement/>");

        QXmlQuery query2;

        {
            QXmlResultItems result1;
            query1.evaluateTo(&result1);

            query2.bindVariable("fromQuery1", result1.next());
            query2.setQuery("$fromQuery1");
        }

        QString output;
        return; // See comment above.
        QVERIFY(query2.evaluateTo(&output));

        QCOMPARE(output, QString::fromLatin1("<anElement/>\n"));
    }
}

void tst_QXmlQuery::messageHandler() const
{
    {
        /* Check default value. */
        QXmlQuery query;
        QCOMPARE(query.messageHandler(), static_cast<QAbstractMessageHandler *>(0));
    }
}

void tst_QXmlQuery::setMessageHandler() const
{
    QXmlQuery query;
    MessageSilencer silencer;
    query.setMessageHandler(&silencer);
    QCOMPARE(static_cast<QAbstractMessageHandler *>(&silencer), query.messageHandler());
}

void tst_QXmlQuery::evaluateToReceiver()
{
    QFETCH(QString, inputQuery);

    /* This query prints a URI specific to the local system. */
    if(inputQuery == QLatin1String("static-base-uri.xq"))
        return;

    ++m_pushTestsCount;
    const QString queryURI(inputFile(m_xmlPatternsDir + QLatin1String("/queries/") + inputQuery));
    QFile queryFile(queryURI);

    QVERIFY(queryFile.exists());
    QVERIFY(queryFile.open(QIODevice::ReadOnly));

    QXmlQuery query;

    MessageSilencer receiver;
    query.setMessageHandler(&receiver);
    query.setQuery(&queryFile, QUrl::fromLocalFile(queryURI));

    /* We read all the queries, and some of them are invalid. However, we
     * only want those that compile. */
    if(!query.isValid())
        return;

    QString produced;
    QTextStream stream(&produced, QIODevice::WriteOnly);
    PushBaseliner push(stream, query.namePool());
    QVERIFY(push.isValid());
    query.evaluateTo(&push);

    const QString baselineName(QFINDTESTDATA("pushBaselines/") + inputQuery.left(inputQuery.length() - 2) + QString::fromLatin1("ref"));
    QFile baseline(baselineName);

    if(baseline.exists())
    {
        QVERIFY(baseline.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString stringedBaseline(QString::fromUtf8(baseline.readAll()));
        QCOMPARE(produced, stringedBaseline);
    }
    else
    {
        QVERIFY(baseline.open(QIODevice::WriteOnly));
        /* This is intentionally a warning, don't remove it. Update the baselines instead. */
        qWarning() << "Generated baseline for:" << baselineName;
        ++m_generatedBaselines;

        baseline.write(produced.toUtf8());
    }
}

void tst_QXmlQuery::evaluateToReceiver_data() const
{
    QTest::addColumn<QString>("inputQuery");

    const auto queries_ = queries();
    for (QString const& query : queries_) {
        /* This outputs a URI specific to the environment, so we can't use it for this
         * particular test. */
        if (query != QLatin1String("staticBaseURI.xq"))
            QTest::newRow(query.toUtf8().constData()) << query;
    }
}

void tst_QXmlQuery::evaluateToReceiverOnInvalidQuery() const
{
    /* Invoke on a default constructed object. */
    {
        QByteArray out;
        QBuffer buffer(&out);
        buffer.open(QIODevice::WriteOnly);

        QXmlQuery query;
        QXmlSerializer serializer(query, &buffer);
        QVERIFY(!query.evaluateTo(&serializer));
    }

    /* Invoke on an invalid query; compile time error. */
    {
        QByteArray out;
        QBuffer buffer(&out);
        buffer.open(QIODevice::WriteOnly);
        MessageSilencer silencer;

        QXmlQuery query;
        query.setMessageHandler(&silencer);
        query.setQuery(QLatin1String("1 + "));
        QXmlSerializer serializer(query, &buffer);
        QVERIFY(!query.evaluateTo(&serializer));
    }

    /* Invoke on an invalid query; runtime error. */
    {
        QByteArray out;
        QBuffer buffer(&out);
        buffer.open(QIODevice::WriteOnly);
        MessageSilencer silencer;

        QXmlQuery query;
        query.setMessageHandler(&silencer);
        query.setQuery(QLatin1String("error()"));
        QXmlSerializer serializer(query, &buffer);
        QVERIFY(!query.evaluateTo(&serializer));
    }
}

void tst_QXmlQuery::evaluateToQStringTriggerError() const
{
    /* Invoke on a default constructed object. */
    {
        QXmlQuery query;
        QString out;
        QVERIFY(!query.evaluateTo(&out));
    }

    /* Invoke on an invalid query; compile time error. */
    {
        QXmlQuery query;
        MessageSilencer silencer;
        query.setMessageHandler(&silencer);

        query.setQuery(QLatin1String("1 + "));

        QString out;
        QVERIFY(!query.evaluateTo(&out));
    }

    /* Invoke on an invalid query; runtime error. */
    {
        QXmlQuery query;
        MessageSilencer silencer;
        query.setMessageHandler(&silencer);

        query.setQuery(QLatin1String("error()"));

        QString out;
        QVERIFY(!query.evaluateTo(&out));
    }
}

void tst_QXmlQuery::evaluateToQString() const
{
    QFETCH(QString, query);
    QFETCH(QString, expectedOutput);

    QXmlQuery queryInstance;
    queryInstance.setQuery(query);
    QVERIFY(queryInstance.isValid());

    QString result;
    QVERIFY(queryInstance.evaluateTo(&result));

    QCOMPARE(result, expectedOutput);
}

void tst_QXmlQuery::evaluateToQString_data() const
{
    QTest::addColumn<QString>("query");
    QTest::addColumn<QString>("expectedOutput");

    QTest::newRow("Two atomics")
        << QString::fromLatin1("1, 'two'")
        << QString::fromLatin1("1 two\n");

    QTest::newRow("An element")
        << QString::fromLatin1("<e>{1}</e>")
        << QString::fromLatin1("<e>1</e>\n");
}

void tst_QXmlQuery::evaluateToQStringSignature() const
{
    const QXmlQuery query;

    QString output;

    /* evaluateTo(QString *) should be a const function. */
    query.evaluateTo(&output);
}

void tst_QXmlQuery::evaluateToQAbstractXmlReceiverTriggerWarnings() const
{
    QXmlQuery query;

    /* We check the return value as well as warning message here. */
    QTest::ignoreMessage(QtWarningMsg, "A non-null callback must be passed.");
    QCOMPARE(query.evaluateTo(static_cast<QAbstractXmlReceiver *>(0)),
             false);
}

void tst_QXmlQuery::evaluateToQXmlResultItems() const
{
    /* Invoke on a default constructed object. */
    {
        QXmlQuery query;
        QXmlResultItems result;
        query.evaluateTo(&result);
        QVERIFY(result.next().isNull());
    }
}

void tst_QXmlQuery::evaluateToQXmlResultItemsTriggerWarnings() const
{
    QTest::ignoreMessage(QtWarningMsg, "A null pointer cannot be passed.");
    QXmlQuery query;
    query.evaluateTo(static_cast<QXmlResultItems *>(0));
}

void tst_QXmlQuery::evaluateToQXmlResultItemsErrorAtEnd() const
{
    QXmlQuery query;
    MessageSilencer silencer;
    query.setMessageHandler(&silencer);
    query.setQuery(QLatin1String("1 to 100, fn:error()"));
    QVERIFY(query.isValid());

    QXmlResultItems it;
    query.evaluateTo(&it);

    while(!it.next().isNull())
    {
    }
}

/*!
  If baselines were generated, we flag it as a failure such that it gets
  attention, and that they are adjusted accordingly.
 */
void tst_QXmlQuery::checkGeneratedBaselines() const
{
    QCOMPARE(m_generatedBaselines, 0);

    /* If this check fails, the auto test setup is misconfigured, or files have
     * been added/removed without this number being updated. */
    QCOMPARE(m_pushTestsCount, int(ExpectedQueryCount));
}

void tst_QXmlQuery::basicXQueryToQtTypeCheck() const
{
    QFile queryFile(m_xmlPatternsDir + QLatin1String("/queries/") + QString::fromLatin1("allAtomics.xq"));
    QVERIFY(queryFile.open(QIODevice::ReadOnly));

    QXmlQuery query;
    query.setQuery(&queryFile);
    QVERIFY(query.isValid());

    QXmlResultItems it;
    query.evaluateTo(&it);

    QVariantList expectedValues;
    expectedValues.append(QString::fromLatin1("xs:untypedAtomic"));
    expectedValues.append(QDateTime(QDate(2002, 10, 10), QTime(23, 2, 11), Qt::UTC));
    expectedValues.append(QDate(2002, 10, 10));
    expectedValues.append(QVariant()); /* We currently doesn't support xs:time through the API. */

    expectedValues.append(QVariant()); /* xs:duration */
    expectedValues.append(QVariant()); /* xs:dayTimeDuration */
    expectedValues.append(QVariant()); /* xs:yearMonthDuration */

    if(sizeof(qreal) == sizeof(float)) {//ARM casts to Float not to double
        expectedValues.append(QVariant(float(3e3)));     /* xs:float */
        expectedValues.append(QVariant(float(4e4)));     /* xs:double */
        expectedValues.append(QVariant(float(2)));       /* xs:decimal */
    } else {
        expectedValues.append(QVariant(double(3e3)));     /* xs:float */
        expectedValues.append(QVariant(double(4e4)));     /* xs:double */
        expectedValues.append(QVariant(double(2)));       /* xs:decimal */
    }

    /* xs:integer and its sub-types. */
    expectedValues.append(QVariant(qlonglong(16)));
    expectedValues.append(QVariant(qlonglong(-6)));
    expectedValues.append(QVariant(qlonglong(-4)));
    expectedValues.append(QVariant(qlonglong(5)));
    expectedValues.append(QVariant(qlonglong(6)));
    expectedValues.append(QVariant(qlonglong(7)));
    expectedValues.append(QVariant(qlonglong(8)));
    expectedValues.append(QVariant(qlonglong(9)));
    expectedValues.append(QVariant(qulonglong(10)));
    expectedValues.append(QVariant(qlonglong(11)));
    expectedValues.append(QVariant(qlonglong(12)));
    expectedValues.append(QVariant(qlonglong(13)));
    expectedValues.append(QVariant(qlonglong(14)));

    expectedValues.append(QVariant());                                                          /* xs:gYearMonth("1976-02"), */
    expectedValues.append(QVariant());                                                          /* xs:gYear("2005-12:00"), */
    expectedValues.append(QVariant());                                                          /* xs:gMonthDay("--12-25-14:00"), */
    expectedValues.append(QVariant());                                                          /* xs:gDay("---25-14:00"), */
    expectedValues.append(QVariant());                                                          /* xs:gMonth("--12-14:00"), */
    expectedValues.append(true);                                                                /* xs:boolean("true"), */
    expectedValues.append(QVariant(QByteArray::fromBase64(QByteArray("aaaa"))));                /* xs:base64Binary("aaaa"), */
    expectedValues.append(QVariant(QByteArray::fromHex(QByteArray("FFFF"))));                   /* xs:hexBinary("FFFF"), */
    expectedValues.append(QVariant(QString::fromLatin1("http://example.com/")));                /* xs:anyURI("http://example.com/"), */
    QXmlNamePool np(query.namePool());
    expectedValues.append(QVariant(qVariantFromValue(QXmlName(np, QLatin1String("localName"),
                                                              QLatin1String("http://example.com/2"),
                                                              QLatin1String("prefix")))));

    expectedValues.append(QVariant(QString::fromLatin1("An xs:string")));
    expectedValues.append(QVariant(QString::fromLatin1("normalizedString")));
    expectedValues.append(QVariant(QString::fromLatin1("token")));
    expectedValues.append(QVariant(QString::fromLatin1("language")));
    expectedValues.append(QVariant(QString::fromLatin1("NMTOKEN")));
    expectedValues.append(QVariant(QString::fromLatin1("Name")));
    expectedValues.append(QVariant(QString::fromLatin1("NCName")));
    expectedValues.append(QVariant(QString::fromLatin1("ID")));
    expectedValues.append(QVariant(QString::fromLatin1("IDREF")));
    expectedValues.append(QVariant(QString::fromLatin1("ENTITY")));

    int i = 0;
    QXmlItem item(it.next());

    while(!item.isNull())
    {
        QVERIFY(item.isAtomicValue());
        const QVariant produced(item.toAtomicValue());

        const QVariant &expected = expectedValues.at(i);

        /* For the cases where we can't represent a value in the XDM with Qt,
         * we return an invalid QVariant. */
        QCOMPARE(expected.isValid(), produced.isValid());

        QCOMPARE(produced.type(), expected.type());

        if(expected.isValid())
        {
            /* This is only needed for xs:decimal though, for some reason. Probably
             * just artifacts created somewhere. */
            if(produced.type() == QVariant::Double)
                QVERIFY(qFuzzyCompare(produced.toDouble(), expected.toDouble()));
            else if (produced.canConvert<QXmlName>())
            {
                /* QVariant::operator==() does identity comparison, it doesn't delegate to operator==() of
                 * the contained type, unless it's hardcoded into QVariant. */
                const QXmlName n1 = qvariant_cast<QXmlName>(produced);
                const QXmlName n2 = qvariant_cast<QXmlName>(expected);
                QCOMPARE(n1, n2);
            }
            else
                QCOMPARE(produced, expected);
        }

        ++i;
        item = it.next();
    }

    QCOMPARE(i, expectedValues.count());
}

/*!
  Send values from Qt into XQuery.
 */
void tst_QXmlQuery::basicQtToXQueryTypeCheck() const
{
    QFile queryFile(m_xmlPatternsDir + QLatin1String("/queries/") + QLatin1String("allAtomicsExternally.xq"));
    QVERIFY(queryFile.exists());
    QVERIFY(queryFile.open(QIODevice::ReadOnly));

    QCOMPARE(QVariant(QDate(1999, 9, 10)).type(), QVariant::Date);

    QXmlQuery query;

    QXmlNamePool np(query.namePool());

    const QXmlName name(np, QLatin1String("localname"),
                            QLatin1String("http://example.com"),
                            QLatin1String("prefix"));

    query.bindVariable(QLatin1String("fromQUrl"), QXmlItem(QUrl(QString::fromLatin1("http://example.com/"))));
    query.bindVariable(QLatin1String("fromQByteArray"), QXmlItem(QByteArray("AAAA")));
    query.bindVariable(QLatin1String("fromBool"), QXmlItem(bool(true)));
    query.bindVariable(QLatin1String("fromQDate"), QXmlItem(QDate(2000, 10, 11)));
    // TODO Do with different QDateTime time specs
    query.bindVariable(QLatin1String("fromQDateTime"), QXmlItem(QDateTime(QDate(2001, 9, 10), QTime(1, 2, 3))));
    query.bindVariable(QLatin1String("fromDouble"), QXmlItem(double(3)));
    query.bindVariable(QLatin1String("fromFloat"), QXmlItem(float(4)));
    query.bindVariable(QLatin1String("integer"), QXmlItem(5));
    query.bindVariable(QLatin1String("fromQString"), QXmlItem(QString::fromLatin1("A QString")));
    query.bindVariable(QLatin1String("fromQChar"), QXmlItem(QChar::fromLatin1('C')));

    query.bindVariable(QLatin1String("fromIntLiteral"), QXmlItem(QVariant(654)));

    {
        QVariant ui(uint(5));
        QCOMPARE(ui.type(), QVariant::UInt);
        query.bindVariable(QLatin1String("fromUInt"), ui);
    }

    {
        QVariant ulnglng(qulonglong(6));
        QCOMPARE(ulnglng.type(), QVariant::ULongLong);
        query.bindVariable(QLatin1String("fromULongLong"), ulnglng);
    }

    {
        QVariant qlnglng(qlonglong(7));
        QCOMPARE(qlnglng.type(), QVariant::LongLong);
        query.bindVariable(QLatin1String("fromLongLong"), qlnglng);
    }

    query.setQuery(&queryFile);

    // TODO do queries which declares external variables with types. Tons of combos here.
    // TODO ensure that binding with QXmlItem() doesn't make a binding available.
    // TODO test rebinding a variable.

    QVERIFY(query.isValid());

    QXmlResultItems it;
    query.evaluateTo(&it);
    QXmlItem item(it.next());
    QVERIFY(!item.isNull());
    QVERIFY(item.isAtomicValue());

    if(sizeof(qreal) == sizeof(float)) //ARM casts to Float not to double
        QCOMPARE(item.toAtomicValue().toString(),
                 QLatin1String("4 true 3 654 7 41414141 C 2000-10-11Z 2001-09-10T01:02:03 "
                               "A QString http://example.com/ 5 6 true false false true true true true true true true "
                               "true true true"));
    else
        QCOMPARE(item.toAtomicValue().toString(),
                 QLatin1String("4 true 3 654 7 41414141 C 2000-10-11Z 2001-09-10T01:02:03 "
                               "A QString http://example.com/ 5 6 true true true true true true true true true true "
                               "true true true"));

}

void tst_QXmlQuery::bindNode() const
{
    QXmlQuery query;
    TestSimpleNodeModel nodeModel(query.namePool());

    query.bindVariable(QLatin1String("node"), nodeModel.root());
    QByteArray out;
    QBuffer buff(&out);
    QVERIFY(buff.open(QIODevice::WriteOnly));

    query.setQuery(QLatin1String("declare variable $node external; $node"));
    QXmlSerializer serializer(query, &buff);

    QVERIFY(query.evaluateTo(&serializer));
    QCOMPARE(out, QByteArray("<nodeName/>"));
}

/*!
  Pass in a relative URI, and make sure it is resolved against the current application directory.
 */
void tst_QXmlQuery::relativeBaseURI() const
{
    QXmlQuery query;
    query.setQuery(QLatin1String("fn:static-base-uri()"), QUrl(QLatin1String("a/relative/uri.weirdExtension")));
    QVERIFY(query.isValid());

    QByteArray result;
    QBuffer buffer(&result);
    QVERIFY(buffer.open(QIODevice::ReadWrite));

    QXmlSerializer serializer(query, &buffer);
    QVERIFY(query.evaluateTo(&serializer));

    const QUrl loaded(QUrl::fromEncoded(result));
    QUrl appPath(QUrl::fromLocalFile(QCoreApplication::applicationFilePath()));

    QVERIFY(loaded.isValid());
    QVERIFY(appPath.isValid());
    QVERIFY(!loaded.isRelative());
    QVERIFY(!appPath.isRelative());

    QFileInfo dir(appPath.toLocalFile());
    dir.setFile(QString());

    /* We can't use QUrl::isParentOf() because it doesn't do what we want it to */
    if(!loaded.toLocalFile().startsWith(dir.absoluteFilePath()))
        QTextStream(stderr) << "dir.absoluteFilePath():" << dir.absoluteFilePath() << "loaded.toLocalFile():" << loaded.toLocalFile();

    checkBaseURI(loaded, dir.absoluteFilePath());
}

void tst_QXmlQuery::emptyBaseURI() const
{
    QXmlQuery query;
    query.setQuery(QLatin1String("fn:static-base-uri()"), QUrl());
    QVERIFY(query.isValid());

    QByteArray result;
    QBuffer buffer(&result);
    QVERIFY(buffer.open(QIODevice::ReadWrite));

    QXmlSerializer serializer(query, &buffer);
    QVERIFY(query.evaluateTo(&serializer));

    const QUrl loaded(QUrl::fromEncoded(result));
    QUrl appPath(QUrl::fromLocalFile(QCoreApplication::applicationFilePath()));

    QVERIFY(loaded.isValid());
    QVERIFY(appPath.isValid());
    QVERIFY(!loaded.isRelative());
    QVERIFY(!appPath.isRelative());

    QFileInfo dir(appPath.toLocalFile());
    dir.setFile(QString());

    QCOMPARE(loaded, appPath);
}

/*!
  Ensure that QDate comes out as QDateTime.
 */
void tst_QXmlQuery::roundTripDateWithinQXmlItem() const
{
    const QDate date(1999, 9, 10);
    QVERIFY(date.isValid());

    const QVariant variant(date);
    QVERIFY(variant.isValid());
    QCOMPARE(variant.type(), QVariant::Date);

    const QXmlItem item(variant);
    QVERIFY(!item.isNull());
    QVERIFY(item.isAtomicValue());

    const QVariant out(item.toAtomicValue());
    QVERIFY(out.isValid());
    QCOMPARE(out.type(), QVariant::Date);
    QCOMPARE(out.toDate(), date);
}

/*!
 Check whether a query is valid, which uses an unbound variable.
 */
void tst_QXmlQuery::bindingMissing() const
{
    QXmlQuery query;
    MessageSilencer messageHandler;
    query.setMessageHandler(&messageHandler);

    QFile queryFile(m_xmlPatternsDir + QLatin1String("/queries/") + QString::fromLatin1("externalVariable.xq"));
    QVERIFY(queryFile.open(QIODevice::ReadOnly));
    query.setQuery(&queryFile);

    QVERIFY(!query.isValid());
}

void tst_QXmlQuery::bindDefaultConstructedItem() const
{
    QFETCH(QXmlItem, item);

    QXmlQuery query;
    MessageSilencer messageHandler;
    query.setMessageHandler(&messageHandler);

    QFile queryFile(m_xmlPatternsDir + QLatin1String("/queries/") + QString::fromLatin1("externalVariable.xq"));
    QVERIFY(queryFile.open(QIODevice::ReadOnly));
    query.setQuery(&queryFile);
    query.bindVariable(QLatin1String("externalVariable"), item);

    QVERIFY(!query.isValid());
}

void tst_QXmlQuery::bindDefaultConstructedItem_data() const
{
    QTest::addColumn<QXmlItem>("item");

    QTest::newRow("QXmlItem()") << QXmlItem();
    QTest::newRow("QXmlItem(QVariant())") << QXmlItem(QVariant());
    QTest::newRow("QXmlItem(QXmlNodeModelIndex())") << QXmlItem(QXmlNodeModelIndex());
}

/*!
  Remove a binding by binding QXmlItem() with the same name.
 */
void tst_QXmlQuery::eraseQXmlItemBinding() const
{
    QXmlQuery query;
    MessageSilencer messageHandler;
    query.setMessageHandler(&messageHandler);

    QFile queryFile(m_xmlPatternsDir + QLatin1String("/queries/") + QString::fromLatin1("externalVariable.xq"));
    QVERIFY(queryFile.open(QIODevice::ReadOnly));
    query.bindVariable(QLatin1String("externalVariable"), QXmlItem(3));
    query.setQuery(&queryFile);
    QVERIFY(query.isValid());

    QByteArray result;
    QBuffer buffer(&result);
    QVERIFY(buffer.open(QIODevice::ReadWrite));

    QXmlSerializer serializer(query, &buffer);
    QVERIFY(query.evaluateTo(&serializer));

    QCOMPARE(result, QByteArray("3 6<e>3</e>false"));

    query.bindVariable(QLatin1String("externalVariable"), QXmlItem());
    QVERIFY(!query.isValid());
}

/*!
 Erase a variable binding
 */
void tst_QXmlQuery::eraseDeviceBinding() const
{
    /* Erase an existing QIODevice binding with another QIODevice binding. */
    {
        QXmlQuery query;

        QByteArray doc("<e/>");
        QBuffer buffer(&doc);
        QVERIFY(buffer.open(QIODevice::ReadOnly));

        query.bindVariable(QLatin1String("in"), &buffer);
        query.setQuery(QLatin1String("$in"));
        QVERIFY(query.isValid());

        query.bindVariable(QLatin1String("in"), 0);
        QVERIFY(!query.isValid());
    }

    /* Erase an existing QXmlItem binding with another QIODevice binding. */
    {
        QXmlQuery query;

        query.bindVariable(QLatin1String("in"), QXmlItem(5));
        query.setQuery(QLatin1String("$in"));
        QVERIFY(query.isValid());

        query.bindVariable(QLatin1String("in"), 0);
        QVERIFY(!query.isValid());
    }
}

/*!
 Bind a variable, evaluate, bind with a different value but same type, and evaluate again.
 */
void tst_QXmlQuery::rebindVariableSameType() const
{
    QXmlQuery query;
    MessageSilencer messageHandler;
    query.setMessageHandler(&messageHandler);

    query.bindVariable(QLatin1String("externalVariable"), QXmlItem(3));

    {
        QFile queryFile(m_xmlPatternsDir + QLatin1String("/queries/") + QString::fromLatin1("externalVariable.xq"));
        QVERIFY(queryFile.open(QIODevice::ReadOnly));
        query.setQuery(&queryFile);
    }

    QVERIFY(query.isValid());

    {
        QByteArray result;
        QBuffer buffer(&result);
        QVERIFY(buffer.open(QIODevice::ReadWrite));

        QXmlSerializer serializer(query, &buffer);
        QVERIFY(query.evaluateTo(&serializer));

        QCOMPARE(result, QByteArray("3 6<e>3</e>false"));
    }

    {
        query.bindVariable(QLatin1String("externalVariable"), QXmlItem(5));
        QByteArray result;
        QBuffer buffer(&result);
        QVERIFY(buffer.open(QIODevice::ReadWrite));

        QXmlSerializer serializer(query, &buffer);
        QVERIFY(query.evaluateTo(&serializer));

        QCOMPARE(result, QByteArray("5 8<e>5</e>false"));
    }

}

void tst_QXmlQuery::rebindVariableDifferentType() const
{
    /* Rebind QXmlItem variable with QXmlItem variable. */
    {
        QXmlQuery query;
        query.bindVariable(QLatin1String("in"), QXmlItem(3));
        query.setQuery(QLatin1String("$in"));
        QVERIFY(query.isValid());

        query.bindVariable(QLatin1String("in"), QXmlItem("A string"));
        QVERIFY(!query.isValid());
    }

    /* Rebind QIODevice variable with QXmlItem variable. */
    {
        QXmlQuery query;
        QBuffer buffer;
        buffer.setData(QByteArray("<e/>"));
        QVERIFY(buffer.open(QIODevice::ReadOnly));

        query.bindVariable(QLatin1String("in"), &buffer);
        query.setQuery(QLatin1String("$in"));
        QVERIFY(query.isValid());

        query.bindVariable(QLatin1String("in"), QXmlItem("A string"));
        QVERIFY(!query.isValid());
    }

    /* Rebind QXmlItem variable with QIODevice variable. The type of the
     * variable changes, so a recompile is necessary. */
    {
        QXmlQuery query;

        query.bindVariable(QLatin1String("in"), QXmlItem(QLatin1String("A string")));
        query.setQuery(QLatin1String("$in"));
        QVERIFY(query.isValid());

        QBuffer buffer;
        buffer.setData(QByteArray("<e/>"));
        QVERIFY(buffer.open(QIODevice::ReadOnly));
        query.bindVariable(QLatin1String("in"), &buffer);
        QVERIFY(!query.isValid());
    }
}

void tst_QXmlQuery::rebindVariableWithNullItem() const
{
    QXmlQuery query;

    query.bindVariable(QLatin1String("name"), QXmlItem(5));
    query.bindVariable(QLatin1String("name"), QXmlItem());
}

void tst_QXmlQuery::constCorrectness() const
{
    QXmlResultItems result;
    QXmlQuery tmp;
    tmp.setQuery(QLatin1String("1")); /* Just so we have a valid query. */
    const QXmlQuery query(tmp);

    /* These functions should be const. */
    query.isValid();
    query.evaluateTo(&result);
    query.namePool();
    query.uriResolver();
    query.messageHandler();

    {
        QString dummyString;
        QTextStream dummyStream(&dummyString);
        PushBaseliner dummy(dummyStream, query.namePool());
        QVERIFY(dummy.isValid());
        query.evaluateTo(&dummy);
    }
}

void tst_QXmlQuery::objectSize() const
{
    /* We have a d pointer. */
    QCOMPARE(sizeof(QXmlQuery), sizeof(void *));
}

void tst_QXmlQuery::setUriResolver() const
{
    /* Set a null resolver, and make sure it can take a const pointer. */
    {
        QXmlQuery query;
        query.setUriResolver(static_cast<const QAbstractUriResolver *>(0));
        QCOMPARE(query.uriResolver(), static_cast<const QAbstractUriResolver *>(0));
    }

    {
        TestURIResolver resolver;
        QXmlQuery query;
        query.setUriResolver(&resolver);
        QCOMPARE(query.uriResolver(), static_cast<const QAbstractUriResolver *>(&resolver));
    }
}

void tst_QXmlQuery::uriResolver() const
{
    /* Check default value. */
    {
        QXmlQuery query;
        QCOMPARE(query.uriResolver(), static_cast<const QAbstractUriResolver *>(0));
    }
}

void tst_QXmlQuery::messageXML() const
{
    QXmlQuery query;

    MessageValidator messageValidator;
    query.setMessageHandler(&messageValidator);

    query.setQuery(QLatin1String("1basicSyntaxError"));

    QRegExp removeFilename(QLatin1String("Location: file:.*\\#"));
    QVERIFY(removeFilename.isValid());

    QVERIFY(messageValidator.success());
    QCOMPARE(messageValidator.received().remove(removeFilename),
             QString::fromLatin1("Type:3\n"
                                 "Description: <html xmlns='http://www.w3.org/1999/xhtml/'><body><p>syntax error, unexpected unknown keyword</p></body></html>\n"
                                 "Identifier: http://www.w3.org/2005/xqt-errors#XPST0003\n"
                                 "1,1"));
}

/*!
  1. Allocate QXmlResultItems
  2. Allocate QXmlQuery
  3. evaluate to the QXmlResultItems instance
  4. Dellocate the QXmlQuery instance
  5. Ensure QXmlResultItems works
 */
void tst_QXmlQuery::resultItemsDeallocatedQuery() const
{
    QXmlResultItems result;

    {
        QXmlQuery query;
        query.setQuery(QLatin1String("1, 2, xs:integer(<e>3</e>)"));
        query.evaluateTo(&result);
    }

    QCOMPARE(result.next().toAtomicValue(), QVariant(1));
    QCOMPARE(result.next().toAtomicValue(), QVariant(2));
    QCOMPARE(result.next().toAtomicValue(), QVariant(3));
    QVERIFY(result.next().isNull());
    QVERIFY(!result.hasError());
}

/*!
  1. Bind variable with bindVariable()
  2. setQuery that has 'declare variable' with same name.
  3. Ensure the value inside the query is used. We don't guarantee this behavior
     but that's what we lock.
 */
void tst_QXmlQuery::shadowedVariables() const
{
    QXmlQuery query;
    query.bindVariable("varName", QXmlItem(3));
    query.setQuery(QLatin1String("declare variable $varName := 5; $varName"));

    QXmlResultItems result;
    query.evaluateTo(&result);

    QCOMPARE(result.next().toAtomicValue(), QVariant(5));
}

void tst_QXmlQuery::setFocusQXmlItem() const
{
    /* Make sure we can take a const reference. */
    {
        QXmlQuery query;
        const QXmlItem item;
        query.setFocus(item);
    }

    // TODO evaluate with atomic value, check type
    // TODO evaluate with node, check type
    // TODO ensure that setFocus() triggers query recompilation, as appropriate.
    // TODO let the focus be undefined, call isvalid, call evaluate anyway
    // TODO let the focus be undefined, call evaluate directly
}

void tst_QXmlQuery::setFocusQUrl() const
{
    /* Load a focus which isn't well-formed. */
    {
        QXmlQuery query;
        MessageSilencer silencer;

        query.setMessageHandler(&silencer);

        QVERIFY(!query.setFocus(QUrl(QLatin1String("data/notWellformed.xml"))));
    }

    /* Ensure the same URI resolver is used. */
    {
        QXmlQuery query(QXmlQuery::XSLT20);

        const TestURIResolver resolver(QUrl(inputFileAsURI(m_xmlPatternsDir + QLatin1String("/stylesheets/documentElement.xml"))));
        query.setUriResolver(&resolver);

        QVERIFY(query.setFocus(QUrl(QLatin1String("arbitraryURI"))));
        query.setQuery(QUrl(inputFileAsURI(m_xmlPatternsDir + QLatin1String("/stylesheets/copyWholeDocument.xsl"))));
        QVERIFY(query.isValid());

        QBuffer result;
        QVERIFY(result.open(QIODevice::ReadWrite));
        QXmlSerializer serializer(query, &result);
        query.evaluateTo(&serializer);

        QCOMPARE(result.data(), QByteArray("<doc/>"));
    }

    // TODO ensure that the focus type doesn't change from XSLT20 on the main instance.
}

/*!
  This code poses a challenge wrt. to internal caching.
 */
void tst_QXmlQuery::setFocusQIODevice() const
{
    QXmlQuery query;

    {
        QBuffer focus;
        focus.setData(QByteArray("<e>abc</e>"));
        QVERIFY(focus.open(QIODevice::ReadOnly));
        query.setFocus(&focus);
        query.setQuery(QLatin1String("string()"));
        QVERIFY(query.isValid());

        QString output;
        query.evaluateTo(&output);

        QCOMPARE(output, QString::fromLatin1("abc\n"));
    }

    /* Set a new focus, make sure it changes & works. */
    {
        QBuffer focus2;
        focus2.setData(QByteArray("<e>abc2</e>"));
        QVERIFY(focus2.open(QIODevice::ReadOnly));
        query.setFocus(&focus2);
        QVERIFY(query.isValid());

        QString output;
        query.evaluateTo(&output);

        QCOMPARE(output, QString::fromLatin1("abc2\n"));
    }
}

/*!
 Since we internally use variable bindings for implementing the focus, we need
 to make sure we don't clash in this area.
*/
void tst_QXmlQuery::setFocusQIODeviceAvoidVariableClash() const
{
    QBuffer buffer;
    buffer.setData("<e>focus</e>");
    QVERIFY(buffer.open(QIODevice::ReadOnly));

    /* First we bind the variable name, then the focus. */
    {
        QXmlQuery query;
        query.bindVariable(QString(QLatin1Char('u')), QVariant(1));
        query.setFocus(&buffer);
        query.setQuery(QLatin1String("string()"));

        QString out;
        query.evaluateTo(&out);

        QCOMPARE(out, QString::fromLatin1("focus\n"));
    }

    /* First we bind the focus, then the variable name. */
    {
        QXmlQuery query;
        QVERIFY(buffer.open(QIODevice::ReadOnly));
        query.setFocus(&buffer);
        query.bindVariable(QString(QLatin1Char('u')), QVariant(1));
        query.setQuery(QLatin1String("string()"));

        QString out;
        query.evaluateTo(&out);

        QCOMPARE(out, QString::fromLatin1("focus\n"));
    }
}

void tst_QXmlQuery::setFocusQIODeviceFailure() const
{
    /* A not well-formed input document. */
    {
        QXmlQuery query;

        MessageSilencer silencer;
        query.setMessageHandler(&silencer);

        QBuffer input;
        input.setData("<e");
        QVERIFY(input.open(QIODevice::ReadOnly));

        QCOMPARE(query.setFocus(&input), false);
    }
}

void tst_QXmlQuery::setFocusQString() const
{
    QXmlQuery query;

    /* Basic use of focus. */
    {
        QVERIFY(query.setFocus(QLatin1String("<e>textNode</e>")));
        query.setQuery(QLatin1String("string()"));
        QVERIFY(query.isValid());
        QString out;
        query.evaluateTo(&out);
        QCOMPARE(out, QString::fromLatin1("textNode\n"));
    }

    /* Set to a new focus, make sure it changes and works. */
    {
        QVERIFY(query.setFocus(QLatin1String("<e>newFocus</e>")));
        QString out;
        query.evaluateTo(&out);
        QCOMPARE(out, QString::fromLatin1("newFocus\n"));
    }
}

void tst_QXmlQuery::setFocusQStringFailure() const
{
    QXmlQuery query;
    MessageSilencer silencer;

    query.setMessageHandler(&silencer);
    QVERIFY(!query.setFocus(QLatin1String("<notWellformed")));

    /* Let's try the slight special case of a null string. */
    QVERIFY(!query.setFocus(QString()));
}

void tst_QXmlQuery::setFocusQStringSignature() const
{
    QXmlQuery query;
    MessageSilencer silencer;
    query.setMessageHandler(&silencer);

    const QString argument;
    /* We should take a const ref. */
    query.setFocus(argument);

    /* We should return a bool. */
    static_cast<bool>(query.setFocus(QString()));
}

void tst_QXmlQuery::setFocusQIODeviceTriggerWarnings() const
{
    /* A null pointer. */
    {
        QXmlQuery query;

        QTest::ignoreMessage(QtWarningMsg, "A null QIODevice pointer cannot be passed.");
        QCOMPARE(query.setFocus(static_cast<QIODevice *>(0)), false);
    }

    /* A non opened-device. */
    {
        QXmlQuery query;

        QBuffer notReadable;
        QVERIFY(!notReadable.isReadable());

        QTest::ignoreMessage(QtWarningMsg, "The device must be readable.");
        QCOMPARE(query.setFocus(&notReadable), false);
    }
}

void tst_QXmlQuery::fnDocNetworkAccessSuccess() const
{
    if (QTest::currentDataTag() == QByteArray("http scheme")
            || QTest::currentDataTag() == QByteArray("ftp scheme"))
        QVERIFY(QtNetworkSettings::verifyTestNetworkSettings());

    QFETCH(QUrl, uriToOpen);
    QFETCH(QByteArray, expectedOutput);

    if(!uriToOpen.isValid())
        qDebug() << "uriToOpen:" << uriToOpen;

    QVERIFY(uriToOpen.isValid());

    QXmlQuery query;
    query.bindVariable(QLatin1String("uri"), QVariant(uriToOpen));
    query.setQuery(QLatin1String("declare variable $uri external;\ndoc($uri)"));
    QVERIFY(query.isValid());

    QByteArray result;
    QBuffer buffer(&result);
    QVERIFY(buffer.open(QIODevice::WriteOnly));

    QXmlSerializer serializer(query, &buffer);
    QVERIFY(query.evaluateTo(&serializer));

    QCOMPARE(result, expectedOutput);
}

void tst_QXmlQuery::fnDocNetworkAccessSuccess_data() const
{
    QTest::addColumn<QUrl>("uriToOpen");
    QTest::addColumn<QByteArray>("expectedOutput");

    QTest::newRow("file scheme")
        << inputFileAsURI(QFINDTESTDATA("input.xml"))
        << QByteArray("<!-- This is just a file for testing. --><input/>");

    QTest::newRow("data scheme with ASCII")
        /* QUrl::toPercentEncoding(QLatin1String("<e/>")) yields "%3Ce%2F%3E". */
        << QUrl::fromEncoded("data:application/xml,%3Ce%2F%3E")
        << QByteArray("<e/>");

    QTest::newRow("data scheme with ASCII no MIME type")
        << QUrl::fromEncoded("data:,%3Ce%2F%3E")
        << QByteArray("<e/>");

    QTest::newRow("data scheme with base 64")
        << QUrl::fromEncoded("data:application/xml;base64,PGUvPg==")
        << QByteArray("<e/>");

    QTest::newRow("qrc scheme")
        << QUrl::fromEncoded("qrc:/QXmlQueryTestData/data/oneElement.xml")
        << QByteArray("<oneElement/>");

    if(!m_testNetwork)
        return;

    QTest::newRow("http scheme")
        << QUrl(QString("http://" + QtNetworkSettings::serverName() + "/qtest/qxmlquery/wellFormed.xml"))
        << QByteArray("<!-- a comment --><e from=\"http\">Some Text</e>");

    QTest::newRow("ftp scheme")
        << QUrl(QString("ftp://" + QtNetworkSettings::serverName() + "/pub/qxmlquery/wellFormed.xml"))
        << QByteArray("<!-- a comment --><e from=\"ftp\">Some Text</e>");

}

void tst_QXmlQuery::fnDocNetworkAccessFailure() const
{
    if (QTest::currentDataTag() == QByteArray("http scheme, not well-formed")
            || QTest::currentDataTag() == QByteArray("https scheme, not well-formed")
            || QTest::currentDataTag() == QByteArray("ftp scheme, not well-formed"))
        QVERIFY(QtNetworkSettings::verifyTestNetworkSettings());

    QFETCH(QUrl, uriToOpen);

    QVERIFY(uriToOpen.isValid());

    QXmlQuery query;
    MessageSilencer silencer;
    query.setMessageHandler(&silencer);
    query.bindVariable(QLatin1String("uri"), QVariant(uriToOpen));
    query.setQuery(QLatin1String("declare variable $uri external;\ndoc($uri)"));
    QVERIFY(query.isValid());

    QXmlResultItems result;
    query.evaluateTo(&result);

    while(!result.next().isNull())
    {
        /* Just loop until the end. */
    }

    // TODO do something that triggers a /timeout/.
    QVERIFY(result.hasError());
}

void tst_QXmlQuery::fnDocNetworkAccessFailure_data() const
{
    QTest::addColumn<QUrl>("uriToOpen");

    QTest::newRow("data scheme, not-well-formed")
        << QUrl(QLatin1String("data:application/xml;base64,PGUvg==="));

    QTest::newRow("file scheme, non-existent file")
        << QUrl(QLatin1String("file:///example.com/does/notExist.xml"));

    QTest::newRow("http scheme, file not found")
        << QUrl(QLatin1String("http://www.example.com/does/not/exist.xml"));

    QTest::newRow("http scheme, nonexistent host")
        << QUrl(QLatin1String("http://this.host.does.not.exist.I.SWear"));

    QTest::newRow("qrc scheme, not well-formed")
        << QUrl(QLatin1String("qrc:/QXmlQueryTestData/notWellformed.xml"));

    QTest::newRow("'qrc:/', non-existing file")
        << QUrl(QLatin1String("qrc:/QXmlQueryTestData/data/thisFileDoesNotExist.xml"));

    if(!m_testNetwork)
        return;

    QTest::newRow("http scheme, not well-formed")
        << QUrl(QString("http://" + QtNetworkSettings::serverName() + "/qtest/qxmlquery/notWellformed.xml"));

    QTest::newRow("https scheme, not well-formed")
        << QUrl(QString("https://" + QtNetworkSettings::serverName() + "/qtest/qxmlquery/notWellformedViaHttps.xml"));

    QTest::newRow("https scheme, nonexistent host")
        << QUrl(QLatin1String("https://this.host.does.not.exist.I.SWear"));

    QTest::newRow("ftp scheme, nonexistent host")
        << QUrl(QLatin1String("ftp://this.host.does.not.exist.I.SWear"));

    QTest::newRow("ftp scheme, not well-formed")
        << QUrl(QString("ftp://" + QtNetworkSettings::serverName() + "/pub/qxmlquery/notWellFormed.xml"));
}

/*!
  Create a network timeout from a QIODevice binding such
  that we ensure we don't hang infinitely.
 */
void tst_QXmlQuery::fnDocOnQIODeviceTimeout() const
{
    if(!m_testNetwork)
        return;

    QTcpServer server;
    server.listen(QHostAddress::LocalHost, 1088);

    QTcpSocket client;
    client.connectToHost("localhost", 1088);
    QVERIFY(client.isReadable());

    QXmlQuery query;

    MessageSilencer silencer;
    query.setMessageHandler(&silencer);

    query.bindVariable(QLatin1String("inDevice"), &client);
    query.setQuery(QLatin1String("declare variable $inDevice external;\ndoc($inDevice)"));
    QVERIFY(query.isValid());

    QXmlResultItems result;
    query.evaluateTo(&result);
    QXmlItem next(result.next());

    while(!next.isNull())
    {
        next = result.next();
    }

    QVERIFY(result.hasError());
}

/*!
 When changing query, the static context must change too, such that
 the source locations are updated.
 */
void tst_QXmlQuery::recompilationWithEvaluateToResultFailing() const
{
    QXmlQuery query;
    MessageSilencer silencer;
    query.setMessageHandler(&silencer);

    query.setQuery(QLatin1String("1 + 1")); /* An arbitrary valid query. */
    QVERIFY(query.isValid()); /* Trigger query compilation. */

    query.setQuery(QLatin1String("fn:doc('doesNotExist.example.com.xml')")); /* An arbitrary invalid query that make use of a source location. */
    QVERIFY(query.isValid()); /* Trigger second compilation. */

    QXmlResultItems items;
    query.evaluateTo(&items);
    items.next();
    QVERIFY(items.hasError());
}

void tst_QXmlQuery::secondEvaluationWithEvaluateToResultFailing() const
{
    QXmlQuery query;
    MessageSilencer silencer;
    query.setMessageHandler(&silencer);

    query.setQuery(QLatin1String("1 + 1")); /* An arbitrary valid query. */
    QVERIFY(query.isValid()); /* Trigger query compilation. */

    query.setQuery(QLatin1String("fn:doc('doesNotExist.example.com.xml')")); /* An arbitrary invalid query that make use of a source location. */
    /* We don't call isValid(). */
QXmlResultItems items;
    query.evaluateTo(&items);
    items.next();
    QVERIFY(items.hasError());
}

/*!
 Compilation is triggered in the evaluation function due to no call to QXmlQuery::isValid().
 */
void tst_QXmlQuery::recompilationWithEvaluateToReceiver() const
{
    QXmlQuery query;
    MessageSilencer silencer;
    query.setMessageHandler(&silencer);

    query.setQuery(QLatin1String("1 + 1")); /* An arbitrary valid query. */
    QVERIFY(query.isValid()); /* Trigger query compilation. */

    query.setQuery(QLatin1String("fn:doc('doesNotExist.example.com.xml')")); /* An arbitrary invalid query that make use of a source location. */
    /* We don't call isValid(). */

    QByteArray dummy;
    QBuffer buffer(&dummy);
    buffer.open(QIODevice::WriteOnly);

    QXmlSerializer serializer(query, &buffer);

    QVERIFY(!query.evaluateTo(&serializer));
}

void tst_QXmlQuery::evaluateToQStringListOnInvalidQuery() const
{
    MessageSilencer silencer;

    /* Invoke on a default constructed object. */
    {
        QXmlQuery query;
        QStringList out;
        QVERIFY(!query.evaluateTo(&out));
    }

    /* Invoke on a syntactically invalid query. */
    {
        QXmlQuery query;
        QStringList out;
        MessageSilencer silencer;

        query.setMessageHandler(&silencer);
        query.setQuery(QLatin1String("1 + "));

        QVERIFY(!query.evaluateTo(&out));
    }

    /* Invoke on a query with the wrong type, one atomic. */
    {
        QXmlQuery query;
        QStringList out;

        query.setQuery(QLatin1String("1"));
        query.setMessageHandler(&silencer);
        QVERIFY(!query.evaluateTo(&out));
    }

    /* Invoke on a query with the wrong type, one element. */
    {
        QXmlQuery query;
        QStringList out;

        query.setQuery(QLatin1String("<e/>"));
        QVERIFY(!query.evaluateTo(&out));
    }

    /* Invoke on a query with the wrong type, mixed nodes & atomics. */
    {
        QXmlQuery query;
        QStringList out;

        query.setQuery(QLatin1String("<e/>, 1, 'a string'"));
        query.setMessageHandler(&silencer);
        QVERIFY(!query.evaluateTo(&out));
    }

    /* Evaluate the empty sequence. */
    {
        QXmlQuery query;
        QStringList out;

        query.setQuery(QLatin1String("()"));
        QVERIFY(!query.evaluateTo(&out));
        QVERIFY(out.isEmpty());
    }
}

void tst_QXmlQuery::evaluateToQStringList() const
{
    QFETCH(QString, queryString);
    QFETCH(QStringList, expectedOutput);

    QXmlQuery query;
    query.setQuery(queryString);
    QStringList out;
    QVERIFY(query.isValid());

    QVERIFY(query.evaluateTo(&out));

    QCOMPARE(out, expectedOutput);
}

void tst_QXmlQuery::evaluateToQStringListTriggerWarnings() const
{
    QXmlQuery query;

    QTest::ignoreMessage(QtWarningMsg, "A non-null callback must be passed.");
    QCOMPARE(query.evaluateTo(static_cast<QStringList *>(0)),
             false);
}

void tst_QXmlQuery::evaluateToQStringList_data() const
{
    QTest::addColumn<QString>("queryString");
    QTest::addColumn<QStringList>("expectedOutput");

    QTest::newRow("One atomic")
        << QString::fromLatin1("(1 + 1) cast as xs:string")
        << QStringList(QString::fromLatin1("2"));

    {
        QStringList expected;
        expected << QLatin1String("2");
        expected << QLatin1String("a string");

        QTest::newRow("Two atomics")
            << QString::fromLatin1("(1 + 1) cast as xs:string, 'a string'")
            << expected;
    }

    QTest::newRow("A query which evaluates to sub-types of xs:string.")
        << QString::fromLatin1("xs:NCName('NCName'), xs:normalizedString('  a b c   ')")
        << (QStringList() << QString::fromLatin1("NCName")
                          << QString::fromLatin1("  a b c   "));

    QTest::newRow("A query which evaluates to two elements.")
        << QString::fromLatin1("string(<e>theString1</e>), string(<e>theString2</e>)")
        << (QStringList() << QString::fromLatin1("theString1")
                          << QString::fromLatin1("theString2"));
}

/*!
  Ensure that we don't automatically convert non-xs:string values.
 */
void tst_QXmlQuery::evaluateToQStringListNoConversion() const
{
    QXmlQuery query;
    query.setQuery(QString::fromLatin1("<e/>"));
    QVERIFY(query.isValid());
    QStringList result;
    QVERIFY(!query.evaluateTo(&result));
}

void tst_QXmlQuery::evaluateToQIODevice() const
{
    /* an XQuery, check that no indentation is performed. */
    {
        QBuffer out;
        QVERIFY(out.open(QIODevice::ReadWrite));

        QXmlQuery query;
        query.setQuery(QLatin1String("<a><b/></a>"));
        QVERIFY(query.isValid());
        QVERIFY(query.evaluateTo(&out));
        QCOMPARE(out.data(), QByteArray("<a><b/></a>"));
    }
}

void tst_QXmlQuery::evaluateToQIODeviceTriggerWarnings() const
{
    QXmlQuery query;

    QTest::ignoreMessage(QtWarningMsg, "The pointer to the device cannot be null.");
    QCOMPARE(query.evaluateTo(static_cast<QIODevice *>(0)),
             false);

    QBuffer buffer;

    QTest::ignoreMessage(QtWarningMsg, "The device must be writable.");
    QCOMPARE(query.evaluateTo(&buffer),
             false);
}

void tst_QXmlQuery::evaluateToQIODeviceSignature() const
{
    /* The function should be const. */
    {
        QBuffer out;
        QVERIFY(out.open(QIODevice::ReadWrite));

        const QXmlQuery query;

        query.evaluateTo(&out);
    }
}

void tst_QXmlQuery::evaluateToQIODeviceOnInvalidQuery() const
{
    QBuffer out;
    QVERIFY(out.open(QIODevice::WriteOnly));

    /* On syntactically invalid query. */
    {
        QXmlQuery query;
        MessageSilencer silencer;
        query.setMessageHandler(&silencer);
        query.setQuery(QLatin1String("1 +"));
        QVERIFY(!query.isValid());
        QVERIFY(!query.evaluateTo(&out));
    }

    /* On null QXmlQuery instance. */
    {
        QXmlQuery query;
        QVERIFY(!query.evaluateTo(&out));
    }

}

void tst_QXmlQuery::setQueryQIODeviceQUrl() const
{
    /* Basic test. */
    {
        QBuffer buffer;
        buffer.setData("1, 2, 2 + 1");
        QVERIFY(buffer.open(QIODevice::ReadOnly));

        QXmlQuery query;
        query.setQuery(&buffer);
        QVERIFY(query.isValid());

        QXmlResultItems result;
        query.evaluateTo(&result);
        QCOMPARE(result.next().toAtomicValue(), QVariant(1));
        QCOMPARE(result.next().toAtomicValue(), QVariant(2));
        QCOMPARE(result.next().toAtomicValue(), QVariant(3));
        QVERIFY(result.next().isNull());
        QVERIFY(!result.hasError());
    }

    /* Set query that is invalid. */
    {
        QBuffer buffer;
        buffer.setData("1, ");
        QVERIFY(buffer.open(QIODevice::ReadOnly));

        QXmlQuery query;
        MessageSilencer silencer;
        query.setMessageHandler(&silencer);
        query.setQuery(&buffer);
        QVERIFY(!query.isValid());
    }

    /* Check that the base URI passes through. */
    {
        QBuffer buffer;
        buffer.setData("string(static-base-uri())");
        QVERIFY(buffer.open(QIODevice::ReadOnly));

        QXmlQuery query;
        query.setQuery(&buffer, QUrl::fromEncoded("http://www.example.com/QIODeviceQUrl"));
        QVERIFY(query.isValid());

        QStringList result;
        query.evaluateTo(&result);
        QCOMPARE(result, QStringList(QLatin1String("http://www.example.com/QIODeviceQUrl")));
    }
}

void tst_QXmlQuery::setQueryQIODeviceQUrlTriggerWarnings() const
{
    QXmlQuery query;
    QTest::ignoreMessage(QtWarningMsg, "A null QIODevice pointer cannot be passed.");
    query.setQuery(0);

    QBuffer buffer;
    QTest::ignoreMessage(QtWarningMsg, "The device must be readable.");
    query.setQuery(&buffer);
}

void tst_QXmlQuery::setQueryQString() const
{
    /* Basic test. */
    {
        QXmlQuery query;
        query.setQuery(QLatin1String("1, 2, 2 + 1"));
        QVERIFY(query.isValid());

        QXmlResultItems result;
        query.evaluateTo(&result);
        QCOMPARE(result.next().toAtomicValue(), QVariant(1));
        QCOMPARE(result.next().toAtomicValue(), QVariant(2));
        QCOMPARE(result.next().toAtomicValue(), QVariant(3));
        QVERIFY(result.next().isNull());
        QVERIFY(!result.hasError());
    }

    /* Set query that is invalid. */
    {
        MessageSilencer silencer;
        QXmlQuery query;
        query.setMessageHandler(&silencer);
        query.setQuery(QLatin1String("1, "));
        QVERIFY(!query.isValid());
    }

    /* Check that the base URI passes through. */
    {
        QXmlQuery query;
        query.setQuery(QLatin1String("string(static-base-uri())"), QUrl::fromEncoded("http://www.example.com/QIODeviceQUrl"));
        QVERIFY(query.isValid());

        QStringList result;
        query.evaluateTo(&result);
        QCOMPARE(result, QStringList(QLatin1String("http://www.example.com/QIODeviceQUrl")));
    }
}

void tst_QXmlQuery::setQueryQUrlSuccess() const
{
    if (QTest::currentDataTag() == QByteArray("A valid query via the ftp scheme")
            || QTest::currentDataTag() == QByteArray("A valid query via the http scheme"))
        QVERIFY(QtNetworkSettings::verifyTestNetworkSettings());

    QFETCH(QUrl, queryURI);
    QFETCH(QByteArray, expectedOutput);

    QVERIFY(queryURI.isValid());

    QXmlQuery query;

    MessageSilencer silencer;
    query.setMessageHandler(&silencer);

    query.setQuery(queryURI);
    QVERIFY(query.isValid());

    QByteArray out;
    QBuffer buffer(&out);
    QVERIFY(buffer.open(QIODevice::WriteOnly));
    QXmlSerializer serializer(query, &buffer);

    query.evaluateTo(&serializer);
    QCOMPARE(out, expectedOutput);
}

void tst_QXmlQuery::setQueryQUrlSuccess_data() const
{
    QTest::addColumn<QUrl>("queryURI");
    QTest::addColumn<QByteArray>("expectedOutput");

    QTest::newRow("A valid query via the data scheme")
        << QUrl::fromEncoded("data:application/xml,1%20%2B%201") /* "1 + 1" */
        << QByteArray("2");

    QTest::newRow("A valid query via the file scheme")
        << QUrl::fromLocalFile(inputFile(m_xmlPatternsDir + QLatin1String("/queries/") + QLatin1String("onePlusOne.xq")))
        << QByteArray("2");

    if(!m_testNetwork)
        return;

    QTest::newRow("A valid query via the ftp scheme")
        << QUrl::fromEncoded(QString("ftp://" + QtNetworkSettings::serverName() + "/pub/qxmlquery/viaFtp.xq").toLatin1())
        << QByteArray("This was received via FTP");

    QTest::newRow("A valid query via the http scheme")
        << QUrl::fromEncoded(QString("http://" + QtNetworkSettings::serverName() + "/qtest/qxmlquery/viaHttp.xq").toLatin1())
        << QByteArray("This was received via HTTP.");
}

void tst_QXmlQuery::setQueryQUrlFailSucceed() const
{
    QXmlQuery query;
    MessageSilencer silencer;

    query.setMessageHandler(&silencer);

    query.setQuery(QLatin1String("1 + 1"));
    QVERIFY(query.isValid());

    query.setQuery(QUrl::fromEncoded("file://example.com/does/not/exist"));
    QVERIFY(!query.isValid());
}

void tst_QXmlQuery::setQueryQUrlFailure() const
{
    if (QTest::currentDataTag() == QByteArray("A query via http:// that is completely empty, but readable.")
            || QTest::currentDataTag() == QByteArray("A query via ftp:// that is completely empty, but readable."))
        QVERIFY(QtNetworkSettings::verifyTestNetworkSettings());

    QFETCH(QUrl, queryURI);

    MessageSilencer silencer;

    QXmlQuery query;
    query.setMessageHandler(&silencer);
    query.setQuery(queryURI);
    QVERIFY(!query.isValid());
}

void tst_QXmlQuery::setQueryQUrlFailure_data() const
{
    QTest::addColumn<QUrl>("queryURI");

    QTest::newRow("Query via file:// that does not exist.")
        << QUrl::fromEncoded("file://example.com/does/not/exist");

    QTest::newRow("A query via file:// that is completely empty, but readable.")
        << QUrl::fromLocalFile(QCoreApplication::applicationFilePath()).resolved(QUrl("../xmlpatterns/queries/completelyEmptyQuery.xq"));

    {
        const QString name(QLatin1String("nonReadableFile.xq"));
        QFile outFile(name);
        QVERIFY(outFile.open(QIODevice::WriteOnly));
        outFile.write(QByteArray("1"));
        outFile.close();
        /* On some windows versions, this fails, so we don't check that this works with QVERIFY. */
        outFile.setPermissions(QFile::Permissions(QFile::Permissions()));

        QTest::newRow("Query via file:/ that does not have read permissions.")
            << QUrl::fromLocalFile(QCoreApplication::applicationFilePath()).resolved(QUrl("nonReadableFile.xq"));
    }

    if(!m_testNetwork)
        return;

    QTest::newRow("Query via HTTP that does not exist.")
        << QUrl::fromEncoded("http://example.com/NoQuery/ISWear");

    /*
    QTest::newRow("Query via FTP that does not exist.")
        << QUrl::fromEncoded("ftp://example.com/NoQuery/ISWear");
        */

    QTest::newRow("A query via http:// that is completely empty, but readable.")
        << QUrl::fromEncoded(QString(
                "http://" + QtNetworkSettings::serverName() + "/qtest/qxmlquery/completelyEmptyQuery.xq").toLatin1());

    QTest::newRow("A query via ftp:// that is completely empty, but readable.")
        << QUrl::fromEncoded(QString(
                "ftp://" + QtNetworkSettings::serverName() + "/pub/qxmlquery/completelyEmptyQuery.xq").toLatin1());

}

void tst_QXmlQuery::setQueryQUrlBaseURI() const
{
    QFETCH(QUrl, inputBaseURI);
    QFETCH(QUrl, expectedBaseURI);

    QXmlQuery query;

    query.setQuery(QUrl(QLatin1String("qrc:/QXmlQueryTestData/queries/staticBaseURI.xq")), inputBaseURI);
    QVERIFY(query.isValid());

    QStringList result;
    QVERIFY(query.evaluateTo(&result));
    QCOMPARE(result.count(), 1);

    if(qstrcmp(QTest::currentDataTag(), "Relative base URI") == 0)
        checkBaseURI(QUrl(result.first()), QCoreApplication::applicationFilePath());
    else
        QCOMPARE(result.first(), expectedBaseURI.toString());
}

void tst_QXmlQuery::setQueryQUrlBaseURI_data() const
{
    QTest::addColumn<QUrl>("inputBaseURI");
    QTest::addColumn<QUrl>("expectedBaseURI");

    QTest::newRow("absolute HTTP")
        << QUrl(QLatin1String("http://www.example.com/"))
        << QUrl(QLatin1String("http://www.example.com/"));

    QTest::newRow("None, so the query URI is used")
        << QUrl()
        << QUrl(QLatin1String("qrc:/QXmlQueryTestData/queries/staticBaseURI.xq"));

    QTest::newRow("Relative base URI")
        << QUrl(QLatin1String("../data/relative.uri"))
        << QUrl();
}

/*!
  1. Create a valid query.
  2. Call setQuery(QUrl), with a query file that doesn't exist.
  3. Verify that the query has changed state into invalid.
 */
void tst_QXmlQuery::setQueryWithNonExistentQUrlOnValidQuery() const
{
    QXmlQuery query;

    MessageSilencer messageSilencer;
    query.setMessageHandler(&messageSilencer);

    query.setQuery(QLatin1String("1 + 1"));
    QVERIFY(query.isValid());

    query.setQuery(QUrl::fromEncoded("qrc:/QXmlQueryTestData/DOESNOTEXIST.xq"));
    QVERIFY(!query.isValid());
}

/*!
  1. Create a valid query.
  2. Call setQuery(QUrl), with a query file that is invalid.
  3. Verify that the query has changed state into invalid.
 */
void tst_QXmlQuery::setQueryWithInvalidQueryFromQUrlOnValidQuery() const
{
    QXmlQuery query;

    MessageSilencer messageSilencer;
    query.setMessageHandler(&messageSilencer);

    query.setQuery(QLatin1String("1 + 1"));
    QVERIFY(query.isValid());

    query.setQuery(QUrl::fromEncoded("qrc:/QXmlQueryTestData/queries/syntaxError.xq"));
    QVERIFY(!query.isValid());
}

/*!
  This triggered two bugs:

  - First, the DynamicContext wasn't assigned to QXmlResultItems, meaning it went out of
    scope and therefore deallocated the document pool, and calls
    to QXmlResultItems::next() would use dangling pointers.

  - Conversion between QPatternist::Item and QXmlItem was incorrectly done, leading to nodes
    being treated as atomic values, and subsequent crashes.

 */
void tst_QXmlQuery::retrieveNameFromQuery() const
{
    QFETCH(QString, queryString);
    QFETCH(QString, expectedName);

    QXmlQuery query;
    query.setQuery(queryString);
    QVERIFY(query.isValid());
    QXmlResultItems result;
    query.evaluateTo(&result);

    QVERIFY(!result.hasError());

    const QXmlItem item(result.next());
    QVERIFY(!result.hasError());
    QVERIFY(!item.isNull());
    QVERIFY(item.isNode());

    const QXmlNodeModelIndex node(item.toNodeModelIndex());
    QVERIFY(!node.isNull());

    QCOMPARE(node.model()->name(node).localName(query.namePool()), expectedName);
}

void tst_QXmlQuery::retrieveNameFromQuery_data() const
{
    QTest::addColumn<QString>("queryString");
    QTest::addColumn<QString>("expectedName");

    QTest::newRow("Document-node")
        << QString::fromLatin1("document{<elementName/>}")
        << QString();

    QTest::newRow("Element")
        << QString::fromLatin1("document{<elementName/>}/*")
        << QString::fromLatin1("elementName");
}

/*!
 Binding a null QString leads to no variable binding, but an
 empty non-null QString is possible.
 */
void tst_QXmlQuery::bindEmptyNullString() const
{
    MessageSilencer messageHandler;
    QXmlQuery query;
    query.setMessageHandler(&messageHandler);
    query.setQuery(QLatin1String("declare variable $v external; $v"));
    /* Here, we effectively pass an invalid QVariant. */
    query.bindVariable(QLatin1String("v"), QVariant(QString()));
    QVERIFY(!query.isValid());

    QStringList result;
    QVERIFY(!query.evaluateTo(&result));
}

void tst_QXmlQuery::bindEmptyString() const
{
    QXmlQuery query;
    query.bindVariable(QLatin1String("v"), QVariant(QString(QLatin1String(""))));
    query.setQuery(QLatin1String("declare variable $v external; $v"));
    QVERIFY(query.isValid());

    QStringList result;
    QVERIFY(query.evaluateTo(&result));
    QStringList expected((QString()));
    QCOMPARE(result, expected);
}

void tst_QXmlQuery::cleanupTestCase() const
{
    /* Remove a weird file we created. */
    const QString name(QLatin1String("nonReadableFile.xq"));

    if(QFile::exists(name))
    {
        QFile file(name);
        QVERIFY(file.setPermissions(QFile::WriteOwner));
        QVERIFY(file.remove());
    }
}

void tst_QXmlQuery::declareUnavailableExternal() const
{
    QXmlQuery query;
    MessageSilencer silencer;
    query.setMessageHandler(&silencer);
    query.setQuery(QLatin1String("declare variable $var external;"
                                 "1 + 1"));
    /* We do not bind $var with QXmlQuery::bindVariable(). */
    QVERIFY(!query.isValid());
}

/*!
 This test triggers an assert in one of the cache iterator
 with MSVC 2005 when compiled in debug mode.
 */
void tst_QXmlQuery::msvcCacheIssue() const
{
    QXmlQuery query;
    query.bindVariable(QLatin1String("externalVariable"), QXmlItem("Variable Value"));
    query.setQuery(QUrl::fromLocalFile(m_xmlPatternsDir + QLatin1String("/queries/") + QString::fromLatin1("externalVariableUsedTwice.xq")));
    QStringList result;
    QVERIFY(query.evaluateTo(&result));

    QCOMPARE(result,
             QStringList() << QString::fromLatin1("Variable Value") << QString::fromLatin1("Variable Value"));
}

void tst_QXmlQuery::unavailableExternalVariable() const
{
    QXmlQuery query;

    MessageSilencer silencer;
    query.setMessageHandler(&silencer);

    query.setQuery(QLatin1String("declare variable $foo external; 1"));

    QVERIFY(!query.isValid());
}

/*!
 Ensure that setUriResolver() affects \c fn:doc() and \c fn:doc-available().
 */
void tst_QXmlQuery::useUriResolver() const
{
    class TestUriResolver : public QAbstractUriResolver
                          , private TestFundament
    {
    public:
        TestUriResolver() {}
        virtual QUrl resolve(const QUrl &relative,
                             const QUrl &baseURI) const
        {
            Q_UNUSED(relative);
            QString fixedInputFile = inputFile(m_xmlPatternsDir + QLatin1String("/queries/") + QLatin1String("simpleDocument.xml"));
#ifdef Q_OS_WIN
            // A file path with drive letter is not a valid relative URI, so remove the drive letter.
            // Note that can't just use inputFileAsURI() instead of inputFile() as that doesn't
            // produce a relative URI either.
            if (fixedInputFile.size() > 1 && fixedInputFile.at(1) == QLatin1Char(':'))
                fixedInputFile.remove(0, 2);
#endif
            return baseURI.resolved(fixedInputFile);
        }
    };

    const TestUriResolver uriResolver;
    QXmlQuery query;

    query.setUriResolver(&uriResolver);
    query.setQuery(QLatin1String("let $i := 'http://www.example.com/DoesNotExist'"
                                 "return (string(doc($i)), doc-available($i))"));


    QXmlResultItems result;
    query.evaluateTo(&result);

    QVERIFY(!result.hasError());
    QCOMPARE(result.next().toAtomicValue().toString(), QString::fromLatin1("text text node"));
    QCOMPARE(result.next().toAtomicValue().toBool(), true);
    QVERIFY(result.next().isNull());
    QVERIFY(!result.hasError());
}

void tst_QXmlQuery::queryWithFocusAndVariable() const
{
    QXmlQuery query;
    query.setFocus(QXmlItem(5));
    query.bindVariable(QLatin1String("var"), QXmlItem(2));

    query.setQuery(QLatin1String("string(. * $var)"));

    QStringList result;

    QVERIFY(query.evaluateTo(&result));

    QCOMPARE(result, QStringList(QLatin1String("10")));
}

void tst_QXmlQuery::undefinedFocus() const
{
    QXmlQuery query;

    MessageSilencer silencer;
    query.setMessageHandler(&silencer);

    query.setQuery(QLatin1String("."));
    QVERIFY(!query.isValid());
}

void tst_QXmlQuery::basicFocusUsage() const
{
    QXmlQuery query;

    MessageSilencer silencer;
    query.setMessageHandler(&silencer);

    query.setFocus(QXmlItem(5));
    query.setQuery(QLatin1String("string(. * .)"));
    QVERIFY(query.isValid());

    QStringList result;
    QVERIFY(query.evaluateTo(&result));

    QCOMPARE(result, QStringList(QLatin1String("25")));
}

/*!
  Triggers an ownership related crash.
 */
void tst_QXmlQuery::copyCheckMessageHandler() const
{
    QXmlQuery query;
    QCOMPARE(query.messageHandler(), static_cast<QAbstractMessageHandler *>(0));

    query.setQuery(QLatin1String("doc('qrc:/QXmlQueryTestData/data/oneElement.xml')"));
    /* By now, we should have set the builtin message handler. */
    const QAbstractMessageHandler *const messageHandler = query.messageHandler();
    QVERIFY(messageHandler);

    {
        /* This copies QXmlQueryPrivate::m_ownerObject, and its destructor
         * will delete it, and hence the builtin message handler attached to it. */
        QXmlQuery copy(query);
    }

    QXmlResultItems result;
    query.evaluateTo(&result);

    while(!result.next().isNull())
    {
    }
    QVERIFY(!result.hasError());
}

void tst_QXmlQuery::queryLanguage() const
{
    /* Check default value. */
    {
        const QXmlQuery query;
        QCOMPARE(query.queryLanguage(), QXmlQuery::XQuery10);
    }

    /* Check default value of copies default instance. */
    {
        const QXmlQuery query1;
        const QXmlQuery query2(query1);

        QCOMPARE(query1.queryLanguage(), QXmlQuery::XQuery10);
        QCOMPARE(query2.queryLanguage(), QXmlQuery::XQuery10);
    }
}

void tst_QXmlQuery::queryLanguageSignature() const
{
    /* This getter should be const. */
    QXmlQuery query;
    query.queryLanguage();
}

void tst_QXmlQuery::enumQueryLanguage() const
{
    /* These enum values should be possible to OR for future plans. */
    QCOMPARE(int(QXmlQuery::XQuery10), 1);
    QCOMPARE(int(QXmlQuery::XSLT20), 2);
    QCOMPARE(int(QXmlQuery::XmlSchema11IdentityConstraintSelector), 1024);
    QCOMPARE(int(QXmlQuery::XmlSchema11IdentityConstraintField), 2048);
    QCOMPARE(int(QXmlQuery::XPath20), 4096);
}

void tst_QXmlQuery::setInitialTemplateNameQXmlName() const
{
    QXmlQuery query(QXmlQuery::XSLT20);
    QXmlNamePool np(query.namePool());
    const QXmlName name(np, QLatin1String("main"));

    query.setInitialTemplateName(name);

    QCOMPARE(query.initialTemplateName(), name);

    query.setQuery(QUrl(inputFileAsURI(m_xmlPatternsDir + QLatin1String("/stylesheets/namedTemplate.xsl"))));
    QVERIFY(query.isValid());

    QBuffer result;
    QVERIFY(result.open(QIODevice::ReadWrite));
    QXmlSerializer serializer(query, &result);
    query.evaluateTo(&serializer);

    QCOMPARE(result.data(), QByteArray("1 2 3 4 5"));

    // TODO invoke a template which has required params.
}

void tst_QXmlQuery::setInitialTemplateNameQXmlNameSignature() const
{
    QXmlQuery query;
    QXmlNamePool np(query.namePool());
    const QXmlName name(np, QLatin1String("foo"));

    /* The signature should take a const reference. */
    query.setInitialTemplateName(name);
}

void tst_QXmlQuery::setInitialTemplateNameQString() const
{
    QXmlQuery query;
    QXmlNamePool np(query.namePool());
    query.setInitialTemplateName(QLatin1String("foo"));

    QCOMPARE(query.initialTemplateName(), QXmlName(np, QLatin1String("foo")));
}

void tst_QXmlQuery::setInitialTemplateNameQStringSignature() const
{
    const QString name(QLatin1String("name"));
    QXmlQuery query;

    /* We should take a const reference. */
    query.setInitialTemplateName(name);
}

void tst_QXmlQuery::initialTemplateName() const
{
    /* Check our default value. */
    QXmlQuery query;
    QCOMPARE(query.initialTemplateName(), QXmlName());
    QVERIFY(query.initialTemplateName().isNull());
}

void tst_QXmlQuery::initialTemplateNameSignature() const
{
    const QXmlQuery query;
    /* This should be a const member. */
    query.initialTemplateName();
}

void tst_QXmlQuery::setNetworkAccessManager() const
{

    /* Ensure fn:doc() picks up the right QNetworkAccessManager. */
    {
        NetworkOverrider networkOverrider(QUrl(QLatin1String("tag:example.com:DOESNOTEXIST")),
                                          QUrl(inputFileAsURI(m_xmlPatternsDir + QLatin1String("/queries/simpleDocument.xml"))));
        QVERIFY(networkOverrider.isValid());

        QXmlQuery query;
        query.setNetworkAccessManager(&networkOverrider);
        query.setQuery(QLatin1String("string(doc('tag:example.com:DOESNOTEXIST'))"));
        QVERIFY(query.isValid());

        QStringList result;
        QVERIFY(query.evaluateTo(&result));

        QCOMPARE(result, QStringList(QLatin1String("text text node")));
    }

    /* Ensure setQuery() is using the right network manager. */
    {
        NetworkOverrider networkOverrider(QUrl(QLatin1String("tag:example.com:DOESNOTEXIST")),
                                          QUrl(inputFileAsURI(m_xmlPatternsDir + QLatin1String("/queries/concat.xq"))));
        QVERIFY(networkOverrider.isValid());

        QXmlQuery query;
        query.setNetworkAccessManager(&networkOverrider);
        query.setQuery(QUrl("tag:example.com:DOESNOTEXIST"));
        QVERIFY(query.isValid());

        QStringList result;
        QVERIFY(query.evaluateTo(&result));

        QCOMPARE(result, QStringList(QLatin1String("abcdef")));
    }
}
void tst_QXmlQuery::networkAccessManagerSignature() const
{
    /* Const object. */
    const QXmlQuery query;

    /* The function should be const. */
    query.networkAccessManager();
}

void tst_QXmlQuery::networkAccessManagerDefaultValue() const
{
    const QXmlQuery query;

    QCOMPARE(query.networkAccessManager(), static_cast<QNetworkAccessManager *>(0));
}

void tst_QXmlQuery::networkAccessManager() const
{
    /* Test that we return the network manager that was set. */
    {
        QNetworkAccessManager manager;
        QXmlQuery query;
        query.setNetworkAccessManager(&manager);
        QCOMPARE(query.networkAccessManager(), &manager);
    }
}

/*!
 \internal
 \since 4.5

  1. Load a document into QXmlQuery's document cache, by executing a query which does it.
  2. Set a focus
  3. Change query, to one which uses the focus
  4. Evaluate

 Used to crash.
 */
void tst_QXmlQuery::multipleDocsAndFocus() const
{
    QXmlQuery query;

    /* We use string concatenation, since variable bindings might disturb what
     * we're testing. */
    query.setQuery(QLatin1String("string(doc('") +
                   inputFile(m_xmlPatternsDir + QLatin1String("/queries/simpleDocument.xml")) +
                   QLatin1String("'))"));
    query.setFocus(QUrl(inputFileAsURI(m_xmlPatternsDir + QLatin1String("/stylesheets/documentElement.xml"))));
    query.setQuery(QLatin1String("string(.)"));

    QStringList result;
    QVERIFY(query.evaluateTo(&result));
}

/*!
 \internal
 \since 4.5

 1. Set a focus
 2. Set a query
 3. Evaluate
 4. Change focus
 5. Evaluate

 Used to crash.
 */
void tst_QXmlQuery::multipleEvaluationsWithDifferentFocus() const
{
    QXmlQuery query;
    QStringList result;

    query.setFocus(QUrl(inputFileAsURI(m_xmlPatternsDir + QLatin1String("/stylesheets/documentElement.xml"))));
    query.setQuery(QLatin1String("string(.)"));
    QVERIFY(query.evaluateTo(&result));

    query.setFocus(QUrl(inputFileAsURI(m_xmlPatternsDir + QLatin1String("/stylesheets/documentElement.xml"))));
    QVERIFY(query.evaluateTo(&result));
}

void tst_QXmlQuery::bindVariableQXmlQuery() const
{
    QFETCH(QString, query1);
    QFETCH(QString, query2);
    QFETCH(QString, expectedOutput);
    QFETCH(bool, expectedSuccess);

    MessageSilencer silencer;
    QXmlQuery xmlQuery1;
    xmlQuery1.setMessageHandler(&silencer);
    xmlQuery1.setQuery(query1);

    QXmlQuery xmlQuery2(xmlQuery1);
    xmlQuery2.bindVariable("query1", xmlQuery1);
    xmlQuery2.setQuery(query2);

    QString output;
    const bool querySuccess = xmlQuery2.evaluateTo(&output);

    QCOMPARE(querySuccess, expectedSuccess);

    if(querySuccess)
        QCOMPARE(output, expectedOutput);
}

void tst_QXmlQuery::bindVariableQXmlQuery_data() const
{
    QTest::addColumn<QString>("query1");
    QTest::addColumn<QString>("query2");
    QTest::addColumn<QString>("expectedOutput");
    QTest::addColumn<bool>("expectedSuccess");

    QTest::newRow("First query has one atomic value.")
            << "2"
            << "1, $query1, 3"
            << "1 2 3\n"
            << true;

    QTest::newRow("First query has two atomic values.")
            << "2, 3"
            << "1, $query1, 4"
            << "1 2 3 4\n"
            << true;

    QTest::newRow("First query is a node.")
            << "<e/>"
            << "1, $query1, 3"
            << "1<e/>3\n"
            << true;

    /* This is a good test, because it triggers the exception in the
     * bindVariable() call, as supposed to when the actual evaluation is done.
     */
    QTest::newRow("First query has a dynamic error.")
            << "error()"
            << "1, $query1"
            << QString() /* We don't care. */
            << false;
}

void tst_QXmlQuery::bindVariableQStringQXmlQuerySignature() const
{
    QXmlQuery query1;
    query1.setQuery("'dummy'");

    QXmlQuery query2;
    const QString name(QLatin1String("name"));

    /* We should be able to take a const QXmlQuery reference. Evaluation never mutate
     * QXmlQuery, and evaluation is what we do here. */
    query2.bindVariable(name, const_cast<const QXmlQuery &>(query1));
}

void tst_QXmlQuery::bindVariableQXmlNameQXmlQuerySignature() const
{
    QXmlNamePool np;
    QXmlQuery query1(np);
    query1.setQuery("'dummy'");

    QXmlQuery query2;
    const QXmlName name(np, QLatin1String("name"));

    /* We should be able to take a const QXmlQuery reference. Evaluation never mutate
     * QXmlQuery, and evaluation is what we do here. */
    query2.bindVariable(name, const_cast<const QXmlQuery &>(query1));
}

/*!
  Check that the QXmlName is handled correctly.
 */
void tst_QXmlQuery::bindVariableQXmlNameQXmlQuery() const
{
    QXmlNamePool np;
    QXmlQuery query1;
    query1.setQuery(QLatin1String("1"));

    QXmlQuery query2(np);
    query2.bindVariable(QXmlName(np, QLatin1String("theName")), query1);
    query2.setQuery("$theName");

    QString result;
    query2.evaluateTo(&result);

    QCOMPARE(result, QString::fromLatin1("1\n"));
}

void tst_QXmlQuery::bindVariableQXmlQueryInvalidate() const
{
    QXmlQuery query;
    query.bindVariable(QLatin1String("name"), QVariant(1));
    query.setQuery("$name");
    QVERIFY(query.isValid());

    QXmlQuery query2;
    query2.setQuery("'query2'");

    query.bindVariable(QLatin1String("name"), query2);
    QVERIFY(!query.isValid());
}

void tst_QXmlQuery::unknownSourceLocation() const
{
    QBuffer b;
    b.setData("<a><b/><b/></a>");
    b.open(QIODevice::ReadOnly);

    MessageSilencer silencer;
    QXmlQuery query;
    query.bindVariable(QLatin1String("inputDocument"), &b);
    query.setMessageHandler(&silencer);

    query.setQuery(QLatin1String("doc($inputDocument)/a/(let $v := b/string() return if ($v) then $v else ())"));

    QString output;
    query.evaluateTo(&output);
}

void tst_QXmlQuery::identityConstraintSuccess() const
{
    QXmlQuery::QueryLanguage queryLanguage = QXmlQuery::XmlSchema11IdentityConstraintSelector;

    /* We run this code for Selector and Field. */
    for(int i = 0; i < 3; ++i)
    {
        QXmlNamePool namePool;
        QXmlResultItems result;
        QXmlItem node;

        {
            QXmlQuery nodeSource(namePool);
            nodeSource.setQuery(QLatin1String("<e/>"));

            nodeSource.evaluateTo(&result);
            node = result.next();
        }

        /* Basic use:
         * 1. The focus is undefined, but it's still valid.
         * 2. We never evaluate. */
        {
            QXmlQuery query(queryLanguage);
            query.setQuery(QLatin1String("a"));
            QVERIFY(query.isValid());
        }

        /* Basic use:
         * 1. The focus is undefined, but it's still valid.
         * 2. We afterwards set the focus. */
        {
            QXmlQuery query(queryLanguage, namePool);
            query.setQuery(QLatin1String("a"));
            query.setFocus(node);
            QVERIFY(query.isValid());
        }

        /* Basic use:
         * 1. The focus is undefined, but it's still valid.
         * 2. We afterwards set the focus.
         * 3. We evaluate. */
        {
            QXmlQuery query(queryLanguage, namePool);
            query.setQuery(QString(QLatin1Char('.')));
            query.setFocus(node);
            QVERIFY(query.isValid());

            QString result;
            QVERIFY(query.evaluateTo(&result));
            QCOMPARE(result, QString::fromLatin1("<e/>\n"));
        }

        /* A slightly more complex Field. */
        {
            QXmlQuery query(queryLanguage);
            query.setQuery(QLatin1String("* | .//xml:*/."));
            QVERIFY(query.isValid());
        }

        /* @ is only allowed in Field. */
        if(queryLanguage == QXmlQuery::XmlSchema11IdentityConstraintField)
        {
            QXmlQuery query(QXmlQuery::XmlSchema11IdentityConstraintField);
            query.setQuery(QLatin1String("@abc"));
            QVERIFY(query.isValid());
        }

        /* Field allows attribute:: and child:: .*/
        if(queryLanguage == QXmlQuery::XmlSchema11IdentityConstraintField)
        {
            QXmlQuery query(QXmlQuery::XmlSchema11IdentityConstraintField);
            query.setQuery(QLatin1String("attribute::name | child::name"));
            QVERIFY(query.isValid());
        }

        /* Selector allows only child:: .*/
        {
            QXmlQuery query(QXmlQuery::XmlSchema11IdentityConstraintSelector);
            query.setQuery(QLatin1String("child::name"));
            QVERIFY(query.isValid());
        }

        if(i == 0)
            queryLanguage = QXmlQuery::XmlSchema11IdentityConstraintField;
        else if(i == 1)
            queryLanguage = QXmlQuery::XPath20;
    }
}

Q_DECLARE_METATYPE(QXmlQuery::QueryLanguage);

/*!
 We just do some basic tests for boot strapping and sanity checking. The actual regression
 testing is in the Schema suite.
 */
void tst_QXmlQuery::identityConstraintFailure() const
{
    QFETCH(QXmlQuery::QueryLanguage, queryLanguage);
    QFETCH(QString, inputQuery);

    QXmlQuery query(queryLanguage);
    MessageSilencer silencer;
    query.setMessageHandler(&silencer);

    query.setQuery(inputQuery);
    QVERIFY(!query.isValid());
}

void tst_QXmlQuery::identityConstraintFailure_data() const
{
    QTest::addColumn<QXmlQuery::QueryLanguage>("queryLanguage");
    QTest::addColumn<QString>("inputQuery");

    QTest::newRow("We don't have element constructors in identity constraint pattern, "
                  "it's an XQuery feature(Selector).")
        << QXmlQuery::XmlSchema11IdentityConstraintSelector
        << QString::fromLatin1("<e/>");

    QTest::newRow("We don't have functions in identity constraint pattern, "
                  "it's an XPath feature(Selector).")
        << QXmlQuery::XmlSchema11IdentityConstraintSelector
        << QString::fromLatin1("current-time()");

    QTest::newRow("We don't have element constructors in identity constraint pattern, "
                  "it's an XQuery feature(Field).")
        << QXmlQuery::XmlSchema11IdentityConstraintSelector
        << QString::fromLatin1("<e/>");

    QTest::newRow("We don't have functions in identity constraint pattern, "
                  "it's an XPath feature(Field).")
        << QXmlQuery::XmlSchema11IdentityConstraintSelector
        << QString::fromLatin1("current-time()");

    QTest::newRow("@attributeName is disallowed for the selector.")
        << QXmlQuery::XmlSchema11IdentityConstraintSelector
        << QString::fromLatin1("@abc");

    QTest::newRow("attribute:: is disallowed for the selector.")
        << QXmlQuery::XmlSchema11IdentityConstraintSelector
        << QString::fromLatin1("attribute::name");

    QTest::newRow("ancestor::name is disallowed for the selector.")
        << QXmlQuery::XmlSchema11IdentityConstraintSelector
        << QString::fromLatin1("ancestor::name");

    QTest::newRow("ancestor::name is disallowed for the field.")
        << QXmlQuery::XmlSchema11IdentityConstraintField
        << QString::fromLatin1("ancestor::name");
}

QTEST_MAIN(tst_QXmlQuery)

#include "tst_qxmlquery.moc"
