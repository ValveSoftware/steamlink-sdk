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

#include <QtCore/QDir>
#include <QtXmlPatterns/QXmlQuery>
#include <QtXmlPatterns/QXmlSerializer>
#include <QtXmlPatterns/QXmlResultItems>
#include <QtXmlPatterns/QXmlFormatter>

#include "../qxmlquery/MessageSilencer.h"
#include "../qsimplexmlnodemodel/TestSimpleNodeModel.h"

/*!
 \class tst_PatternistExamples
 \internal
 \since 4.4
 \brief Verifies examples for Patternist.
 */
class tst_PatternistExamples : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void checkQueries() const;
    void checkQueries_data() const;
    void checkXMLFiles() const;
    void checkXMLFiles_data() const;
    void buildSnippets() const;

private:
    QVector<QDir> m_dirs;
    QStringList listFiles(const QStringList &patterns) const;
    enum Constants
    {
        XMLFileCount = 12,
        XQueryFileCount = 52
    };
};

void tst_PatternistExamples::initTestCase()
{
    m_dirs.append(QDir(QLatin1String(SOURCETREE "src/xmlpatterns/doc/snippets/patternist/")));
    m_dirs.append(QDir(QLatin1String(SOURCETREE "examples/xmlpatterns/xquery/globalVariables/")));
    m_dirs.append(QDir(QLatin1String(SOURCETREE "examples/xmlpatterns/filetree/")));
    m_dirs.append(QDir(QLatin1String(SOURCETREE "examples/xmlpatterns/recipes/")));
    m_dirs.append(QDir(QLatin1String(SOURCETREE "examples/xmlpatterns/recipes/files/")));

    for(int i = 0; i < m_dirs.size(); ++i)
        QVERIFY(m_dirs.at(i).exists());
}

/*!
  Returns a QStringList containing absolute filenames that were found in the predefined locations, when
  filtered through \a pattterns.
 */
QStringList tst_PatternistExamples::listFiles(const QStringList &patterns) const
{
    QStringList result;

    for(int i = 0; i < m_dirs.size(); ++i)
    {
        const QDir &dir = m_dirs.at(i);

        const QStringList files(dir.entryList(patterns));
        for(int s = 0; s < files.count(); ++s)
            result += dir.absoluteFilePath(files.at(s));
    }

    return result;
}

/*!
  Check that the queries contains no static errors such as
  syntax errors.
 */
void tst_PatternistExamples::checkQueries() const
{
    QFETCH(QString, queryFile);

    QFile file(queryFile);
    QVERIFY(file.open(QIODevice::ReadOnly));

    QXmlQuery query;

    /* Two queries relies on this binding, so provide it such that we don't get a compile error. */
    query.bindVariable(QLatin1String("fileToOpen"), QVariant(QString::fromLatin1("dummyString")));

    /* This is needed for the recipes example. */
    query.bindVariable(QLatin1String("inputDocument"), QVariant(QString::fromLatin1("dummString")));

    /* This is needed for literalsAndOperators.xq. */
    query.bindVariable(QLatin1String("date"), QVariant(QDate::currentDate()));

    /* These are needed for introExample2.xq. */
    query.bindVariable(QLatin1String("file"), QVariant(QLatin1String("dummy")));
    query.bindVariable(QLatin1String("publisher"), QVariant(QLatin1String("dummy")));
    query.bindVariable(QLatin1String("year"), QVariant(2000));

    /* and filetree/ needs this. */
    TestSimpleNodeModel nodeModel(query.namePool());
    query.bindVariable(QLatin1String("exampleDirectory"), nodeModel.root());

    query.setQuery(&file, queryFile);

    QVERIFY2(query.isValid(), QString::fromLatin1("%1 failed to compile").arg(queryFile).toLatin1().constData());
}

void tst_PatternistExamples::checkQueries_data() const
{
    QTest::addColumn<QString>("queryFile");

    const QStringList queryExamples(listFiles(QStringList(QLatin1String("*.xq"))));

    QCOMPARE(queryExamples.count(), int(XQueryFileCount));

    for (const QString &q : queryExamples)
        QTest::newRow(q.toLocal8Bit().constData()) << q;
}

void tst_PatternistExamples::checkXMLFiles() const
{
    QFETCH(QString, file);

    QXmlQuery query;
    /* Wrapping in QUrl ensures it gets formatted as a URI on all platforms. */
    query.setQuery(QLatin1String("doc('") + QUrl::fromLocalFile(file).toString() + QLatin1String("')"));
    QVERIFY(query.isValid());

    /* We don't care about the result, we only want to ensure the files can be parsed. */
    QByteArray dummy;
    QBuffer buffer(&dummy);
    QVERIFY(buffer.open(QIODevice::WriteOnly));

    QXmlSerializer serializer(query, &buffer);

    /* This is the important one. */
    QVERIFY(query.evaluateTo(&serializer));
}

void tst_PatternistExamples::checkXMLFiles_data() const
{
    QTest::addColumn<QString>("file");
    QStringList patterns;
    patterns.append(QLatin1String("*.xml"));
    patterns.append(QLatin1String("*.gccxml"));
    patterns.append(QLatin1String("*.svg"));
    patterns.append(QLatin1String("*.ui"));
    patterns.append(QLatin1String("*.html"));

    const QStringList xmlFiles(listFiles(patterns));

    if(xmlFiles.count() != XMLFileCount)
        qDebug() << "These files were encountered:" << xmlFiles;

    QCOMPARE(xmlFiles.count(), int(XMLFileCount));

    for (const QString &q : xmlFiles)
        QTest::newRow(q.toLocal8Bit().constData()) << q;
}

/*!
 Below, we include all the examples and ensure that they build, such that we rule
 out syntax error and that API changes has propagated into examples.

 An improvement could be to run them, to ensure that they behave as they intend
 to.
 */

static QUrl abstractURI()
{
    QUrl baseURI;
    QUrl relative;
#include "../../../src/xmlpatterns/doc/snippets/code/src_xmlpatterns_api_qabstracturiresolver.cpp"
}

class MyValue
{
public:
    MyValue parent() const
    {
        return MyValue();
    }
};

static MyValue toMyValue(const QXmlNodeModelIndex &)
{
    return MyValue();
}

static QXmlNodeModelIndex toNodeIndex(const MyValue &)
{
    return QXmlNodeModelIndex();
}

class MyTreeModel : public QSimpleXmlNodeModel
{
public:
    MyTreeModel(const QXmlNamePool &np, const QFile &f);

    virtual QUrl documentUri(const QXmlNodeModelIndex&) const
    {
        return QUrl();
    }

    virtual QXmlNodeModelIndex::NodeKind kind(const QXmlNodeModelIndex&) const
    {
        return QXmlNodeModelIndex::Element;
    }

    virtual QXmlNodeModelIndex::DocumentOrder compareOrder(const QXmlNodeModelIndex&, const QXmlNodeModelIndex&) const
    {
        return QXmlNodeModelIndex::Is;
    }

    virtual QXmlNodeModelIndex root(const QXmlNodeModelIndex&) const
    {
        return QXmlNodeModelIndex();
    }

    virtual QXmlName name(const QXmlNodeModelIndex&) const
    {
        return QXmlName();
    }

    virtual QVariant typedValue(const QXmlNodeModelIndex&) const
    {
        return QVariant();
    }

    virtual QVector<QXmlNodeModelIndex> attributes(const QXmlNodeModelIndex&) const
    {
        return QVector<QXmlNodeModelIndex>();
    }

    QXmlNodeModelIndex nodeFor(const QString &) const
    {
        return QXmlNodeModelIndex();
    }

    virtual QXmlNodeModelIndex nextFromSimpleAxis(SimpleAxis axis, const QXmlNodeModelIndex &origin) const;
};

/*
 Exists for linking with at least msvc-2005.
*/
MyTreeModel::MyTreeModel(const QXmlNamePool &np, const QFile &) : QSimpleXmlNodeModel(np)
{
}

#include "../../../src/xmlpatterns/doc/snippets/code/src_xmlpatterns_api_qsimplexmlnodemodel.cpp"

class MyMapper
{
public:
    class InputType;
    enum OutputType
    {
    };
#include "../../../src/xmlpatterns/doc/snippets/code/src_xmlpatterns_api_qabstractxmlforwarditerator.cpp"
};

#include "../../../src/xmlpatterns/doc/snippets/code/src_xmlpatterns_api_qxmlname.cpp"

void tst_PatternistExamples::buildSnippets() const
{
    /* We don't run this code, see comment above. */
    return;

    /* We place a call to this function, such that GCC doesn't emit a warning. */
    abstractURI();

    {
    }

    {
#include "../../../src/xmlpatterns/doc/snippets/code/src_xmlpatterns_api_qxmlresultitems.cpp"
    }

    {
    }

    {
        QIODevice *myOutputDevice = 0;
#include "../../../src/xmlpatterns/doc/snippets/code/src_xmlpatterns_api_qxmlformatter.cpp"
    }

    {
        QIODevice *myOutputDevice = 0;
#include "../../../src/xmlpatterns/doc/snippets/code/src_xmlpatterns_api_qxmlserializer.cpp"
    }

    {
        QXmlNodeModelIndex myInstance;
        const char **argv = 0;
        typedef MyTreeModel ChemistryNodeModel;
#include "../../../src/xmlpatterns/doc/snippets/code/src_xmlpatterns_api_qabstractxmlnodemodel.cpp"
    }

    {
    }

    {
        QIODevice *myOutputDevice = 0;
#include "../../../src/xmlpatterns/doc/snippets/code/src_xmlpatterns_api_qabstractxmlreceiver.cpp"
    }

    {
        QXmlQuery query;
        QString localName;
        QVariant value;
#include "../../../src/xmlpatterns/doc/snippets/code/src_xmlpatterns_api_qxmlquery.cpp"
    }
}

QTEST_MAIN(tst_PatternistExamples)

#include "tst_patternistexamples.moc"

// vim: et:ts=4:sw=4:sts=4
