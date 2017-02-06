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

#include <QtCore/QUrl>
#include <QtCore/QFileInfo>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>

#include <QtHelp/QHelpEngineCore>

class tst_QHelpEngineCore : public QObject
{
    Q_OBJECT

private slots:
    void init();

    void setupData();
    void collectionFile();
    void setCollectionFile();
    void copyCollectionFile();

    void namespaceName();
    void registeredDocumentations();
    void registerDocumentation();
    void unregisterDocumentation();
    void documentationFileName();

    void customFilters();
    void removeCustomFilter();
    void addCustomFilter();
    void filterAttributes();
    void currentFilter();
    void setCurrentFilter();

    void filterAttributeSets();
    void files();
    void fileData();

    void linksForIdentifier();

    void customValue();
    void setCustomValue();
    void removeCustomValue();

    void setAutoSaveFilter();

    void metaData();

private:
    QString m_path;
    QString m_colFile;
};

void tst_QHelpEngineCore::init()
{
    // defined in profile
    m_path = QLatin1String(SRCDIR);

    m_path = QFileInfo(m_path).absoluteFilePath();

    m_colFile = m_path + QLatin1String("/data/col.qhc");
    if (QFile::exists(m_colFile))
        QDir::current().remove(m_colFile);
    if (!QFile::copy(m_path + "/data/collection.qhc", m_colFile))
        QFAIL("Cannot copy file!");
    QFile f(m_colFile);
    f.setPermissions(QFile::WriteUser|QFile::ReadUser);
}

void tst_QHelpEngineCore::setupData()
{
    QHelpEngineCore help(m_colFile, 0);
    QCOMPARE(help.setupData(), true);
}

void tst_QHelpEngineCore::collectionFile()
{
    QHelpEngineCore help(m_colFile, 0);
    QCOMPARE(help.collectionFile(), QFileInfo(m_colFile).absoluteFilePath());
}

void tst_QHelpEngineCore::setCollectionFile()
{
    QHelpEngineCore help(m_colFile, 0);
    QCOMPARE(help.collectionFile(), QFileInfo(m_colFile).absoluteFilePath());

    QString col1File = m_path + QLatin1String("/data/collection1.qhc");
    help.setCollectionFile(col1File);
    QCOMPARE(help.collectionFile(), QFileInfo(col1File).absoluteFilePath());

    QStringList docs = help.registeredDocumentations();
    QCOMPARE(docs.count(), 1);
    QCOMPARE(docs.first(), QLatin1String("trolltech.com.1-0-0.test"));
}

void tst_QHelpEngineCore::copyCollectionFile()
{
    QHelpEngineCore help(m_colFile, 0);
    QCOMPARE(help.collectionFile(), QFileInfo(m_colFile).absoluteFilePath());

    QString copiedFile = m_path + QLatin1String("/collectionCopy.qhc");
    if (QFile::exists(copiedFile))
        QDir::current().remove(copiedFile);

    QCOMPARE(help.copyCollectionFile(copiedFile), true);

    {
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "testdb");
        db.setDatabaseName(copiedFile);
        if (!db.open()) {
            QSqlDatabase::removeDatabase("testdb");
            QFAIL("Created database seems to be corrupt!");
        }
        QSqlQuery *m_query = new QSqlQuery(db);

        m_query->exec("SELECT Key, Value FROM SettingsTable");
        if (m_query->next()) {
            QCOMPARE(m_query->value(0).toString(), QString("CurrentFilter"));
            QCOMPARE(m_query->value(1).toString(), QString("unfiltered"));
        } else {
            QFAIL("Settingstable is corrupt!");
        }

        m_query->exec("SELECT NameId, FilterAttributeId FROM FilterTable");
        int i = 0;
        while (m_query->next()) {
            if (i == 3) {
                QCOMPARE(m_query->value(0).toInt(), 2);
                QCOMPARE(m_query->value(1).toInt(), 6);
            }
            ++i;
        }
        QCOMPARE(i, 7);
        m_query->clear();

        m_query->exec("SELECT Name, FilePath FROM NamespaceTable");
        i = 0;
        while (m_query->next()) {
            if (i == 0) {
                QCOMPARE(m_query->value(0).toString(), QString("trolltech.com.3-3-8.qmake"));
                QCOMPARE(m_query->value(1).toString(), QString("data/qmake-3.3.8.qch"));
            }
            ++i;
        }
        QCOMPARE(i, 3);

        m_query->clear();
        delete m_query;
    }
    QSqlDatabase::removeDatabase("testdb");
}

void tst_QHelpEngineCore::namespaceName()
{
    QCOMPARE(QHelpEngineCore::namespaceName(m_path + "/data/qmake-3.3.8.qch"),
        QString("trolltech.com.3-3-8.qmake"));
    QCOMPARE(QHelpEngineCore::namespaceName(m_path + "/data/linguist-3.3.8.qch"),
        QString("trolltech.com.3-3-8.linguist"));
}

void tst_QHelpEngineCore::registeredDocumentations()
{
    QHelpEngineCore help(m_colFile, 0);
    QCOMPARE(help.setupData(), true);
    QStringList docs = help.registeredDocumentations();
    QCOMPARE(docs.count(), 3);
    QStringList lst;
    lst << "trolltech.com.3-3-8.qmake" << "trolltech.com.4-3-0.qmake"
        << "trolltech.com.1-0-0.test";
    foreach (QString s, docs)
        lst.removeAll(s);
    QCOMPARE(lst.isEmpty(), true);
}

void tst_QHelpEngineCore::registerDocumentation()
{
    if (QFile::exists(m_colFile))
        QDir::current().remove(m_colFile);
    {
        QHelpEngineCore c(m_colFile);
        QCOMPARE(c.setupData(), true);
        c.registerDocumentation(m_path + "/data/qmake-3.3.8.qch");
        QCOMPARE(c.registeredDocumentations().count(), 1);
        c.registerDocumentation(m_path + "/data/qmake-3.3.8.qch");
        QCOMPARE(c.registeredDocumentations().count(), 1);
        c.registerDocumentation(m_path + "/data/linguist-3.3.8.qch");
        QCOMPARE(c.registeredDocumentations().count(), 2);
    }

    {
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "testdb");
        db.setDatabaseName(m_colFile);
        if (!db.open()) {
            QSqlDatabase::removeDatabase("testdb");
            QFAIL("Created database seems to be corrupt!");
        }
        QSqlQuery query(db);
        query.exec("SELECT FilePath FROM NamespaceTable WHERE "
            "Name=\'trolltech.com.3-3-8.linguist\'");
        if (query.next())
            QCOMPARE(query.value(0).toString(),
                QString("linguist-3.3.8.qch"));
        else
            QFAIL("Query error!");
    }
    QSqlDatabase::removeDatabase("testdb");
}

void tst_QHelpEngineCore::unregisterDocumentation()
{
    QHelpEngineCore c(m_colFile);
    QCOMPARE(c.setupData(), true);
    QCOMPARE(c.registeredDocumentations().count(), 3);
    c.unregisterDocumentation("trolltech.com.3-3-8.qmake");
    QCOMPARE(c.registeredDocumentations().count(), 2);
    QCOMPARE(c.unregisterDocumentation("noexisting"), false);
}

void tst_QHelpEngineCore::documentationFileName()
{
    QHelpEngineCore c(m_colFile);
    QCOMPARE(c.setupData(), true);
    QCOMPARE(c.documentationFileName(QLatin1String("trolltech.com.3-3-8.qmake")),
        QString(m_path + "/data/qmake-3.3.8.qch"));
    QCOMPARE(c.documentationFileName(QLatin1String("trolltech.com.1-0-0.test")),
        QString(m_path + "/data/test.qch"));
    QCOMPARE(c.documentationFileName(QLatin1String("trolltech.com.empty")),
        QString());
}

void tst_QHelpEngineCore::customFilters()
{
    QHelpEngineCore help(m_colFile, 0);
    QCOMPARE(help.setupData(), true);
    QStringList custom = help.customFilters();
    QCOMPARE(custom.count(), 4);
    QStringList lst;
    lst << "qmake Manual" << "Custom Filter 1"
        << "Custom Filter 2" << "unfiltered";
    foreach (QString s, custom)
        lst.removeAll(s);
    QCOMPARE(lst.count(), 0);
}

void tst_QHelpEngineCore::removeCustomFilter()
{
    QHelpEngineCore help(m_colFile, 0);
    QCOMPARE(help.setupData(), true);
    help.removeCustomFilter("Custom Filter 1");
    QStringList custom = help.customFilters();
    QCOMPARE(custom.count(), 3);
    QCOMPARE((bool)custom.contains("Custom Filter 1"), false);
}

void tst_QHelpEngineCore::addCustomFilter()
{
    QHelpEngineCore help(m_colFile, 0);
    QCOMPARE(help.setupData(), true);
    help.addCustomFilter("Qt Tools", QStringList() << "tools" << "qt");
    QStringList custom = help.customFilters();
    QCOMPARE(custom.count(), 5);
    QCOMPARE((bool)custom.contains("Qt Tools"), true);
}

void tst_QHelpEngineCore::filterAttributes()
{
    QHelpEngineCore help(m_colFile, 0);
    QCOMPARE(help.setupData(), true);
    QStringList atts = help.filterAttributes("qmake Manual");
    QCOMPARE(atts.count(), 3);
    QStringList lst;
    lst << "qmake" << "tools" << "qt";
    foreach (QString s, atts)
        lst.removeAll(s);
    QCOMPARE(lst.count(), 0);
}

void tst_QHelpEngineCore::currentFilter()
{
    QHelpEngineCore help(m_colFile, 0);
    QCOMPARE(help.setupData(), true);
    QCOMPARE(help.currentFilter(), QString("unfiltered"));
}

void tst_QHelpEngineCore::setCurrentFilter()
{
    QHelpEngineCore help(m_colFile, 0);
    QCOMPARE(help.setupData(), true);
    QCOMPARE(help.currentFilter(), QString("unfiltered"));
    help.setCurrentFilter("qmake Manual");
    QCOMPARE(help.currentFilter(), QString("qmake Manual"));
    QCOMPARE(help.customValue("CurrentFilter").toString(),
        QString("qmake Manual"));
}

void tst_QHelpEngineCore::filterAttributeSets()
{
    QHelpEngineCore help(m_colFile, 0);
    QCOMPARE(help.setupData(), true);
    QList<QStringList> lst = help.filterAttributeSets("trolltech.com.1-0-0.test");
    QCOMPARE(lst.count(), 2);
    QCOMPARE(lst.first().count(), 2);
    QCOMPARE((bool)lst.first().contains("filter1"), true);
    QCOMPARE((bool)lst.last().contains("filter2"), true);
}

void tst_QHelpEngineCore::files()
{
    QHelpEngineCore help(m_colFile, 0);
    QCOMPARE(help.setupData(), true);
    QList<QUrl> lst = help.files("trolltech.com.4-3-0.qmake",
        QStringList());
    QCOMPARE(lst.count(), 16);
    lst = help.files("trolltech.com.4-3-0.qmake",
        QStringList(), "png");
    QCOMPARE(lst.count(), 2);
    lst = help.files("trolltech.com.4-3-0.qmake",
        QStringList() << "qt", "html");
    QCOMPARE(lst.count(), 13);
    lst = help.files("trolltech.com.4-3-0.qmake",
        QStringList() << "qt" << "qmake", "html");
    QCOMPARE(lst.count(), 13);
    lst = help.files("trolltech.com.4-3-0.qmake",
        QStringList() << "qt" << "qmake" << "bla", "html");
    QCOMPARE(lst.count(), 0);
    lst = help.files("trolltech.com.4-3-0.qmake",
        QStringList() << "qt" << "qmake", "foo");
    QCOMPARE(lst.count(), 0);

    foreach (QUrl url, lst)
        qDebug() << url;

}

void tst_QHelpEngineCore::fileData()
{
    QHelpEngineCore help(m_colFile, 0);
    QCOMPARE(help.setupData(), true);
    QByteArray ba = help.fileData(QUrl("NotExisting"));
    QCOMPARE(ba.size(), 0);
    ba = help.fileData(QUrl("qthelp://trolltech.com.1-0-0.test/testFolder/test.html"));
    QTextStream s(ba, QIODevice::ReadOnly|QIODevice::Text);
    QFile f(m_path + "/data/test.html");
    if (!f.open(QIODevice::ReadOnly|QIODevice::Text))
        QFAIL("Cannot open original file!");
    QTextStream ts(&f);
    QCOMPARE(s.readAll(), ts.readAll());
}

void tst_QHelpEngineCore::linksForIdentifier()
{
    QHelpEngineCore help(m_colFile, 0);
    QCOMPARE(help.setupData(), true);
    QMap<QString, QUrl> map;
    map = help.linksForIdentifier("Test::foo");
    QCOMPARE(map.contains("Test Manual"), true);
    QCOMPARE(map.count(), 1);
    QCOMPARE(map.value("Test Manual"),
        QUrl("qthelp://trolltech.com.1.0.0.test/testFolder/test.html#foo"));

    help.setCurrentFilter("Custom Filter 2");
    map = help.linksForIdentifier("People::newton");
    QCOMPARE(map.isEmpty(), true);
    map = help.linksForIdentifier("Fancy::foobar");
    QCOMPARE(map.contains("Fancy"), true);
    QCOMPARE(map.count(), 1);
    QCOMPARE(map.value("Fancy"),
        QUrl("qthelp://trolltech.com.1.0.0.test/testFolder/fancy.html#foobar"));
}

void tst_QHelpEngineCore::customValue()
{
    QHelpEngineCore help(m_colFile, 0);
    QCOMPARE(help.setupData(), true);
    QCOMPARE(help.customValue("CurrentFilter").toString(),
        QString("unfiltered"));
}

void tst_QHelpEngineCore::setCustomValue()
{
    QHelpEngineCore help(m_colFile, 0);
    QCOMPARE(help.setupData(), true);
    QCOMPARE(help.setCustomValue("Test", 3), true);
    QCOMPARE(help.customValue("Test").toInt(), 3);
    QCOMPARE(help.removeCustomValue("Test"), true);
    QCOMPARE(help.customValue("Test"), QVariant());
}

void tst_QHelpEngineCore::removeCustomValue()
{
    setCustomValue();
}

void tst_QHelpEngineCore::setAutoSaveFilter()
{
    QHelpEngineCore help(m_colFile, 0);
    QCOMPARE(help.setupData(), true);
    QCOMPARE(help.currentFilter(), QString("unfiltered"));

    help.setAutoSaveFilter(false);
    help.setCurrentFilter("qmake Manual");
    QCOMPARE(help.currentFilter(), QString("qmake Manual"));
    QCOMPARE(help.customValue("CurrentFilter").toString(),
        QString("unfiltered"));
}

void tst_QHelpEngineCore::metaData()
{
    QCOMPARE(QHelpEngineCore::metaData(m_path + "/data/test.qch", "author").toString(),
        QString("Digia Plc and/or its subsidiary(-ies)"));
    QCOMPARE(QHelpEngineCore::metaData(m_path + "/data/test.qch", "notExisting").isValid(),
        false);
}

QTEST_MAIN(tst_QHelpEngineCore)
#include "tst_qhelpenginecore.moc"
