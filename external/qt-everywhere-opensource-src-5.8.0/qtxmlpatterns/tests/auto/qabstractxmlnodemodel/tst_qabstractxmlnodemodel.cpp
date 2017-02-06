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


#include <QFile>
#include <QtTest/QtTest>

#include <QSourceLocation>
#include <QXmlFormatter>
#include <QXmlNamePool>
#include <QXmlQuery>
#include <QXmlResultItems>
#include <QXmlSerializer>
#include <QFileInfo>
#include <QDir>

#include "TestNodeModel.h"
#include "LoadingModel.h"
#include "../qxmlquery/TestFundament.h"

/*!
 \class tst_QAbstractXmlNodeModel
 \internal
 \since 4.4
 \brief Tests the QAbstractXmlNodeModel class.
 */
class tst_QAbstractXmlNodeModel : public QObject
                                , private TestFundament
{
    Q_OBJECT

private Q_SLOTS:
    // TODO lots of tests missing
    void initTestCase();
    void constructor() const;
    void objectSize() const;
    void nextFromSimpleAxis();
    void nextFromSimpleAxis_data() const;
    void constCorrectness() const;
    void createData() const;
    void createPointerAdditionalData() const;
    void createDataAdditionalData() const;
    void id() const;
    void idref() const;
    void typedValue() const;
    void sourceLocation() const;

private:
    QAbstractXmlNodeModel::Ptr  m_nodeModel;
    QXmlNamePool                m_namePool;
    QXmlNodeModelIndex          m_rootNode;
};

const char testFileName[] = "tree.xml";

void tst_QAbstractXmlNodeModel::initTestCase()
{
    const QString testFilePath = QFINDTESTDATA(testFileName);
    QVERIFY2(!testFilePath.isEmpty(), "tree.xml not found");
    const QString testDirectory = QFileInfo(testFilePath).absolutePath();
    QVERIFY2(QDir::setCurrent(testDirectory), qPrintable(QStringLiteral("Could not chdir to ") + testDirectory));

    m_nodeModel = LoadingModel::create(m_namePool);
    QVERIFY(m_nodeModel);
    m_rootNode = m_nodeModel->root(QXmlNodeModelIndex());
    QVERIFY(!m_rootNode.isNull());
}

void tst_QAbstractXmlNodeModel::constructor() const
{
    /* Allocate instance. */
    {
        TestNodeModel instance;
    }

    {
        TestNodeModel instance1;
        TestNodeModel instance2;
    }

    {
        TestNodeModel instance1;
        TestNodeModel instance2;
        TestNodeModel instance3;
    }
}

void tst_QAbstractXmlNodeModel::objectSize() const
{
    /* We can't currently test this in portable way,
     * so disable it. */
    return;

    const int pointerSize = sizeof(void *);
    // adjust for pointer alignment
    const int sharedDataSize = ((sizeof(QSharedData) + pointerSize) / pointerSize) * pointerSize;
    const int modelSize = sizeof(QAbstractXmlNodeModel);

    /* A d pointer plus a vtable pointer. */
    QCOMPARE(modelSize, sharedDataSize + pointerSize * 2);
}

/*!
 Tests nextFromSimpleAxis(). More exactly that all the logic in
 QAbstractXmlNodeModel::iterate() is as we expect to. Subsequently, a lot
 of testing code is in LoadingModel(.cpp).

 Approach:

  1. In initTestCase() we loaded tree.xml into LoadingModel and
     stored the root node in m_rootNode.
  2. We execute a query that navigates from m_rootNode and write out
     the result using QXmlFormatter.
  3. We execute the exact same query, but this time use the built in node backend,
     and write out the result in the same way. This is our baseline.
  4. Compare the two.

  Hence we check QAbstractXmlNodeModel::iterate() and friends against our XQuery
  code, which in turn is (mainly) checked by the XQTS. This means safer testing
  since we don't create baselines manually, and it also means that we can modify
  the input file, tree.xml, without having to update static baselines.
 */
void tst_QAbstractXmlNodeModel::nextFromSimpleAxis()
{
    QFETCH(QString, queryString);

    QBuffer out;

    /* Fill out, using LoadingModel. */
    {
        QXmlQuery query(m_namePool);
        query.bindVariable(QLatin1String("node"), m_rootNode);
        query.setQuery(queryString);
        QVERIFY(query.isValid());

        QVERIFY(out.open(QIODevice::WriteOnly));
        QXmlFormatter formatter(query, &out);

        QVERIFY(query.evaluateTo(&formatter));
    }

    QBuffer baseline;

    /* Create the baseline. */
    {
        QXmlQuery openDoc(m_namePool);
        const QString testFilePath = QDir::currentPath() + QLatin1Char('/') + QLatin1String(testFileName);
        openDoc.bindVariable(QLatin1String("docURI"), QVariant(QUrl::fromLocalFile(testFilePath)));
        openDoc.setQuery(QLatin1String("doc($docURI)"));
        QXmlResultItems doc;
        QVERIFY(openDoc.isValid());
        openDoc.evaluateTo(&doc);

        QXmlQuery produceBaseline(m_namePool);
        produceBaseline.bindVariable(QLatin1String("node"), doc.next());
        produceBaseline.setQuery(queryString);
        QVERIFY(produceBaseline.isValid());
        QVERIFY(baseline.open(QIODevice::WriteOnly));

        QXmlFormatter baselineFormatter(produceBaseline, &baseline);
        QVERIFY(produceBaseline.evaluateTo(&baselineFormatter));
    }

    if(out.data() != baseline.data())
    {
        QTextStream(stderr) << "ACTUAL:" << QString::fromUtf8(out.data().constData())
                            << "EXPECTED:" << QString::fromUtf8(baseline.data().constData());
    }

    QCOMPARE(out.data(), baseline.data());
}

void tst_QAbstractXmlNodeModel::nextFromSimpleAxis_data() const
{
     QTest::addColumn<QString>("queryString");

     QTest::newRow("The whole tree")
         << "$node";

     QTest::newRow("::descendant-or-self from $node, all nodes.")
         << "$node/descendant-or-self::node()";

     QTest::newRow("::descendant from $node, all nodes.")
         << "$node/descendant::node()";

     QTest::newRow("::descendant from node with no descendants.")
         << "$node/descendant::node()";

     QTest::newRow("following-sibling on $root.")
         << "$node/text()[1]/following-sibling::node()";

     QTest::newRow("following-sibling from section1.")
         << "$node//section1/following-sibling::node()";

     QTest::newRow("preceding-sibling-sibling from section1.")
         << "$node//section1/preceding-sibling::node()";

     QTest::newRow("following-sibling from oneTextChild.")
         << "$node//oneTextChild/following-sibling::node()";

     QTest::newRow("preceding-sibling-sibling from oneTextChild.")
         << "$node//oneTextChild/preceding-sibling::node()";

     QTest::newRow("preceding-sibling on $root.")
         << "$node/preceding-sibling::node()";

     QTest::newRow("::ancestor from node at the end")
         << "$node//node()/ancestor::node()";

     QTest::newRow("::ancestor-or-self from node at the end")
         << "$node//node()/ancestor-or-self::node()";

     QTest::newRow("Copy attributes from all nodes.")
         << "<e>{for $i in $node//node()/@* order by $i return $i}</e>";

     QTest::newRow("::preceding from node at the end")
         << "($node//node())[last()]/preceding::node()";

     QTest::newRow("::preceding from $node")
         << "$node/preceding::node()";

     QTest::newRow("::following from node at the end")
         << "($node//node())[last()]/following::node()";

     QTest::newRow("::following from $node")
         << "$node//following::node()";

     QTest::newRow("::following from $node")
         << "$node/following::node()";

     QTest::newRow("::descendant from text() nodes.")
         << "$node/descendant-or-self::text()/descendant::node()";

     QTest::newRow("::descendant-or-self from text() nodes.")
         << "$node/descendant-or-self::text()/descendant-or-self::node()";

     QTest::newRow("descendant-or-self::node() from section1.")
         << "$node//section1/descendant-or-self::node()";

     QTest::newRow("descendant::node() from section1.")
         << "$node//section1/descendant::node()";

     /* Checks for axis order. */

     QTest::newRow("::descendant from text() nodes with last(), checking axis order.")
         << "$node/descendant-or-self::text()/(descendant::node()[last()])";

     QTest::newRow("::descendant-or-self from text() nodes with last(), checking axis order.")
         << "$node/descendant-or-self::text()/(descendant-or-self::node()[last()])";

     QTest::newRow("::descendant from text() nodes with predicate, checking axis order.")
         << "$node/descendant-or-self::text()/(descendant::node()[2])";

     QTest::newRow("::descendant-or-self from text() nodes with predicate, checking axis order.")
         << "$node/descendant-or-self::text()/(descendant-or-self::node()[2])";

     QTest::newRow("::following from node at the end with predicate, checking axis order.")
         << "($node//node())[last()]/(following::node()[2])";

     QTest::newRow("::following from node at the end with last(), checking axis order.")
         << "($node//node())[last()]/(following::node()[last()])";

     QTest::newRow("ancestor:: from node at the end with predicate, checking axis order.")
         << "($node//node())[last()]/(ancestor::node()[2])";

     QTest::newRow("ancestor:: from node at the end with last(), checking axis order.")
         << "($node//node())[last()]/(ancestor::node()[last()])";

     QTest::newRow("ancestor-or-self:: from node at the end with predicate, checking axis order.")
         << "($node//node())[last()]/(ancestor::node()[2])";

     QTest::newRow("ancestor-or-self:: from node at the end with last(), checking axis order.")
         << "($node//node())[last()]/(ancestor::node()[last()])";

     QTest::newRow("::preceding from node at the end with predicate, checking axis order.")
         << "($node//node())[last()]/(preceding::node()[2])";

     QTest::newRow("descendant-or-self in two levels, with last()")
         << "$node/descendant-or-self::text()/(descendant-or-self::node()[last()])";

     QTest::newRow("descendant-or-self with last()")
         << "$node/descendant-or-self::node()[last()]";

     QTest::newRow("::preceding from node at the end with last(), checking axis order.")
         << "$node/preceding::node()[last()]";

     QTest::newRow("::preceding combined with descendant-or-self, from node at the end with last(), checking axis order.")
         << "$node//preceding::node()[last()]";

     QTest::newRow("::preceding combined with descendant-or-self, from node at the end with last(), checking axis order.")
         << "($node//node())[last()]/(preceding::node()[last()])";
}

void tst_QAbstractXmlNodeModel::constCorrectness() const
{
    // TODO
}

void tst_QAbstractXmlNodeModel::createData() const
{
    // TODO
    // Verify that the argument is qint64
}

void tst_QAbstractXmlNodeModel::createPointerAdditionalData() const
{
    // TODO
    // Verify that the second argument is qint64
}

void tst_QAbstractXmlNodeModel::createDataAdditionalData() const
{
    // TODO
}

void tst_QAbstractXmlNodeModel::id() const
{
    // TODO verify that invalid NCNames are not sent to the model.
}

void tst_QAbstractXmlNodeModel::idref() const
{
    // TODO verify that invalid NCNames are not sent to the model.
}

/*!
 Verify that if QAbstractXmlNodeModel::typedValue() return a null
 QVariant, it is treated as that the node has no typed value.
 */
void tst_QAbstractXmlNodeModel::typedValue() const
{
    class TypedModel : public TestNodeModel
    {
    public:
        virtual QVariant typedValue(const QXmlNodeModelIndex &) const
        {
            return QVariant();
        }

        QXmlNodeModelIndex root() const
        {
            return createIndex(qint64(1));
        }
    };

    TypedModel model;

    QXmlQuery query;
    query.bindVariable(QLatin1String("node"), model.root());
    query.setQuery(QLatin1String("declare variable $node external;"
                                 "string($node), data($node)"));

    QByteArray output;
    QBuffer buffer(&output);
    QVERIFY(buffer.open(QIODevice::WriteOnly));
    QVERIFY(query.isValid());

    QXmlSerializer serializer(query, &buffer);
    QVERIFY(query.evaluateTo(&serializer));

    QVERIFY(output.isEmpty());
}

void tst_QAbstractXmlNodeModel::sourceLocation() const
{
    const QAbstractXmlNodeModel* const constModel = m_nodeModel.data();
    const QSourceLocation location = constModel->sourceLocation(m_rootNode);
}

QTEST_MAIN(tst_QAbstractXmlNodeModel)

#include "tst_qabstractxmlnodemodel.moc"

// vim: et:ts=4:sw=4:sts=4
