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
#include <QtTest/QtTest>

#include <QtCore/QFileInfo>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>

#include <QtHelp/private/qhelpgenerator_p.h>
#include <QtHelp/private/qhelpprojectdata_p.h>

class tst_QHelpGenerator : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void generateHelp();

private:
    void checkNamespace();
    void checkFilters();
    void checkIndices();
    void checkFiles();
    void checkMetaData();

    QString m_outputFile;
    QSqlQuery *m_query;
};

void tst_QHelpGenerator::initTestCase()
{
    QString path = QLatin1String(SRCDIR);
    m_outputFile = path + QLatin1String("/data/test.qch");
    if (QFile::exists(m_outputFile)) {
        QDir d;
        if (!d.remove(m_outputFile))
            QFAIL("Cannot remove old output file!");
    }
}

void tst_QHelpGenerator::generateHelp()
{
    // defined in profile
    QString path = QLatin1String(SRCDIR);

    QString inputFile(path + "/data/test.qhp");
    QHelpProjectData data;
    if (!data.readData(inputFile))
        QFAIL("Cannot read qthp file!");

    QHelpGenerator generator;
    QCOMPARE(generator.generate(&data, m_outputFile), true);

    {
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "testdb");
        db.setDatabaseName(m_outputFile);
        if (!db.open()) {
            QSqlDatabase::removeDatabase("testdb");
            QFAIL("Created database seems to be corrupt!");
        }
        m_query = new QSqlQuery(db);
        checkNamespace();
        checkFilters();
        checkIndices();
        checkFiles();
        checkMetaData();

        m_query->clear();
        delete m_query;
    }
    QSqlDatabase::removeDatabase("testdb");

    // check if db is still in use...
    initTestCase();
}

void tst_QHelpGenerator::checkNamespace()
{
    m_query->exec("SELECT Id, Name FROM NamespaceTable");
    if (m_query->next()
        && m_query->value(1).toString() == QLatin1String("trolltech.com.1.0.0.test"))
        return;
    QFAIL("Namespace Error!");
}

void tst_QHelpGenerator::checkFilters()
{
    m_query->exec("SELECT COUNT(Id) FROM FilterAttributeTable");
    if (!m_query->next() || m_query->value(0).toInt() != 3)
        QFAIL("FilterAttribute Error!");

    m_query->exec("SELECT a.Name FROM FilterAttributeTable a, FilterTable b, "
        "FilterNameTable c WHERE c.Id=b.NameId AND b.FilterAttributeID=a.Id "
        "AND c.Name=\'Custom Filter 2\'");
    QStringList lst;
    while (m_query->next())
        lst.append(m_query->value(0).toString());
    if (!lst.contains("test") || !lst.contains("filter2"))
        QFAIL("FilterAttribute Error!");
}

void tst_QHelpGenerator::checkIndices()
{
    m_query->exec("SELECT a.Name, b.Anchor FROM FileNameTable a, "
        "IndexTable b, IndexFilterTable c, FilterAttributeTable d "
        "WHERE a.FileID=b.FileId AND b.Id=c.IndexId "
        "AND c.FilterAttributeId=d.Id AND d.Name=\'filter1\' AND b.Name=\'foo\'");
    if (!m_query->next() || m_query->value(0).toString() != QLatin1String("test.html")
        || m_query->value(1).toString() != QLatin1String("foo"))
        QFAIL("Index Error!");

    /*
    m_query->exec("SELECT COUNT(DISTINCT Id) FROM IndexItemTable");
    if (!m_query->next() || m_query->value(0).toInt() != 7)
        QFAIL("Index Error!");
    */

    m_query->exec("SELECT COUNT(a.Id) FROM IndexTable a, "
        "IndexFilterTable b, FilterAttributeTable c WHERE a.Id=b.IndexId "
        "AND b.FilterAttributeId=c.Id AND c.Name=\'filter2\'");
    if (!m_query->next() || m_query->value(0).toInt() != 3)
        QFAIL("Index Error!");
}

void tst_QHelpGenerator::checkFiles()
{
    m_query->exec("SELECT COUNT(a.FileId) FROM FileNameTable a, FolderTable b "
        "WHERE a.FolderId=b.Id AND b.Name=\'testFolder\'");
    if (!m_query->next() || m_query->value(0).toInt() != 6)
        QFAIL("File Error!");

    QStringList lst;
    lst << "classic.css" << "test.html" << "people.html" << "sub/about.html";
    m_query->exec("SELECT a.Name FROM FileNameTable a, FileFilterTable b, "
        "FilterAttributeTable c WHERE c.Id=b.FilterAttributeId "
        "AND b.FileId=a.FileID AND c.Name=\'filter1\'");
    while (m_query->next())
        lst.removeAll(m_query->value(0).toString());
    QCOMPARE(lst.count(), 0);

    QMap<int, QStringList> fileAtts;
    m_query->exec("SELECT a.Id, b.Name FROM FileAttributeSetTable a, "
        "FilterAttributeTable b WHERE a.FilterAttributeId=b.Id");
    while (m_query->next()) {
        int id = m_query->value(0).toInt();
        if (!fileAtts.contains(id))
            fileAtts.insert(id, QStringList());
        fileAtts[id].append(m_query->value(1).toString());
    }
    QCOMPARE(fileAtts.count(), 2);
    QCOMPARE((bool)fileAtts.value(1).contains("test"), true);
    QCOMPARE((bool)fileAtts.value(1).contains("filter1"), true);
    QCOMPARE((bool)fileAtts.value(1).contains("filter2"), false);
    QCOMPARE((bool)fileAtts.value(2).contains("test"), true);
    QCOMPARE((bool)fileAtts.value(2).contains("filter2"), true);
}

void tst_QHelpGenerator::checkMetaData()
{
    m_query->exec("SELECT COUNT(Value) FROM MetaDataTable");
    if (!m_query->next())
        QFAIL("Meta Data Error");
    QCOMPARE(m_query->value(0).toInt(), 4);

    m_query->exec("SELECT Value FROM MetaDataTable WHERE Name=\'author\'");
    if (!m_query->next())
        QFAIL("Meta Data Error");
    QCOMPARE(m_query->value(0).toString(), QString("Digia Plc and/or its subsidiary(-ies)"));
}

QTEST_MAIN(tst_QHelpGenerator)
#include "tst_qhelpgenerator.moc"
