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
#include <QWinJumpList>
#include <QWinJumpListItem>
#include <QWinJumpListCategory>

Q_DECLARE_METATYPE(QWinJumpListItem::Type)

class tst_QWinJumpList : public QObject
{
    Q_OBJECT

private slots:
    void testRecent();
    void testFrequent();
    void testTasks();
    void testCategories();
    void testItems_data();
    void testItems();
};

static inline QByteArray msgFileNameMismatch(const QString &f1, const QString &f2)
{
    const QString result = QLatin1Char('"') + f1 + QStringLiteral("\" != \"")
        + f2 + QLatin1Char('"');
    return result.toLocal8Bit();
}

void tst_QWinJumpList::testRecent()
{
    if (QSysInfo::windowsVersion() >= QSysInfo::WV_WINDOWS8_1)
        QSKIP("QTBUG-48751: Recent items do not work on Windows 8.1 or 10", Continue);
    QScopedPointer<QWinJumpList> jumplist(new QWinJumpList);
    QWinJumpListCategory *recent1 = jumplist->recent();
    QVERIFY(recent1);
    QVERIFY(!recent1->isVisible());
    QVERIFY(recent1->title().isEmpty());

    recent1->clear();
    QVERIFY(recent1->isEmpty());

    recent1->addItem(0);
    QVERIFY(recent1->isEmpty());

    recent1->setVisible(true);
    QVERIFY(recent1->isVisible());
    recent1->addLink(QStringLiteral("tst_QWinJumpList"), QCoreApplication::applicationFilePath());

    QTest::ignoreMessage(QtWarningMsg, "QWinJumpListCategory::addItem(): only tasks/custom categories support separators.");
    recent1->addSeparator();

    QTest::ignoreMessage(QtWarningMsg, "QWinJumpListCategory::addItem(): only tasks/custom categories support destinations.");
    recent1->addDestination(QCoreApplication::applicationDirPath());

    // cleanup the first jumplist instance and give the system a little time to update.
    // then test that another jumplist instance loads up the recent item(s) added above
    jumplist.reset();
    QTest::qWait(100);

    jumplist.reset(new QWinJumpList);
    QWinJumpListCategory *recent2 = jumplist->recent();
    QVERIFY(recent2);
    QCOMPARE(recent2->count(), 1);

    QWinJumpListItem* item = recent2->items().value(0);
    QVERIFY(item);
    const QString itemPath = item->filePath();
    const QString applicationFilePath = QCoreApplication::applicationFilePath();
    QVERIFY2(!itemPath.compare(applicationFilePath, Qt::CaseInsensitive),
             msgFileNameMismatch(itemPath, applicationFilePath));
    QEXPECT_FAIL("", "QWinJumpListItem::title not supported for recent items", Continue);
    QCOMPARE(item->title(), QStringLiteral("tst_QWinJumpList"));

    recent2->clear();
    QVERIFY(recent2->isEmpty());
}

void tst_QWinJumpList::testFrequent()
{
    if (QSysInfo::windowsVersion() >= QSysInfo::WV_WINDOWS8_1)
        QSKIP("QTBUG-48751: Frequent items do not work on Windows 8.1 or 10", Continue);
    QScopedPointer<QWinJumpList> jumplist(new QWinJumpList);
    QWinJumpListCategory *frequent1 = jumplist->frequent();
    QVERIFY(frequent1);
    QVERIFY(!frequent1->isVisible());
    QVERIFY(frequent1->title().isEmpty());

    frequent1->clear();
    QVERIFY(frequent1->isEmpty());

    frequent1->addItem(0);
    QVERIFY(frequent1->isEmpty());

    frequent1->setVisible(true);
    QVERIFY(frequent1->isVisible());
    frequent1->addLink(QStringLiteral("tst_QWinJumpList"), QCoreApplication::applicationFilePath());

    QTest::ignoreMessage(QtWarningMsg, "QWinJumpListCategory::addItem(): only tasks/custom categories support separators.");
    frequent1->addSeparator();

    QTest::ignoreMessage(QtWarningMsg, "QWinJumpListCategory::addItem(): only tasks/custom categories support destinations.");
    frequent1->addDestination(QCoreApplication::applicationDirPath());

    // cleanup the first jumplist instance and give the system a little time to update.
    // then test that another jumplist instance loads up the frequent item(s) added above
    jumplist.reset();
    QTest::qWait(100);

    jumplist.reset(new QWinJumpList);
    QWinJumpListCategory *frequent2 = jumplist->frequent();
    QVERIFY(frequent2);
    QCOMPARE(frequent2->count(), 1);

    QWinJumpListItem* item = frequent2->items().value(0);
    QVERIFY(item);
    const QString itemPath = item->filePath();
    const QString applicationFilePath = QCoreApplication::applicationFilePath();
    QVERIFY2(!itemPath.compare(applicationFilePath, Qt::CaseInsensitive),
             msgFileNameMismatch(itemPath, applicationFilePath));
    QEXPECT_FAIL("", "QWinJumpListItem::title not supported for frequent items", Continue);
    QCOMPARE(item->title(), QStringLiteral("tst_QWinJumpList"));

    frequent2->clear();
    QVERIFY(frequent2->isEmpty());
}

void tst_QWinJumpList::testTasks()
{
    QWinJumpList jumplist;
    QWinJumpListCategory *tasks = jumplist.tasks();
    QVERIFY(tasks);
    QVERIFY(!tasks->isVisible());
    QVERIFY(tasks->isEmpty());
    QVERIFY(tasks->title().isEmpty());

    tasks->setVisible(true);
    QVERIFY(tasks->isVisible());

    tasks->addItem(0);
    QVERIFY(tasks->isEmpty());

    QWinJumpListItem* link1 = tasks->addLink(QStringLiteral("tst_QWinJumpList"), QCoreApplication::applicationFilePath());
    QCOMPARE(link1->type(), QWinJumpListItem::Link);
    QCOMPARE(link1->title(), QStringLiteral("tst_QWinJumpList"));
    QCOMPARE(link1->filePath(), QCoreApplication::applicationFilePath());
    QCOMPARE(tasks->count(), 1);
    QCOMPARE(tasks->items(), QList<QWinJumpListItem *>() << link1);

    QWinJumpListItem* link2 = tasks->addLink(QStringLiteral("tst_QWinJumpList"), QCoreApplication::applicationFilePath(), QStringList(QStringLiteral("-test")));
    QCOMPARE(link2->type(), QWinJumpListItem::Link);
    QCOMPARE(link2->title(), QStringLiteral("tst_QWinJumpList"));
    QCOMPARE(link2->filePath(), QCoreApplication::applicationFilePath());
    QCOMPARE(link2->arguments(), QStringList(QStringLiteral("-test")));
    QCOMPARE(tasks->count(), 2);
    QCOMPARE(tasks->items(), QList<QWinJumpListItem *>() << link1 << link2);

    QWinJumpListItem* separator = tasks->addSeparator();
    QCOMPARE(separator->type(), QWinJumpListItem::Separator);
    QCOMPARE(tasks->count(), 3);
    QCOMPARE(tasks->items(), QList<QWinJumpListItem *>() << link1 << link2 << separator);

    QWinJumpListItem* destination = tasks->addDestination(QCoreApplication::applicationDirPath());
    QCOMPARE(destination->type(), QWinJumpListItem::Destination);
    QCOMPARE(destination->filePath(), QCoreApplication::applicationDirPath());
    QCOMPARE(tasks->count(), 4);
    QCOMPARE(tasks->items(), QList<QWinJumpListItem *>() << link1 << link2 << separator << destination);

    tasks->clear();
    QVERIFY(tasks->isEmpty());
    QVERIFY(tasks->items().isEmpty());
}

void tst_QWinJumpList::testCategories()
{
    QWinJumpList jumplist;
    QVERIFY(jumplist.categories().isEmpty());

    jumplist.addCategory(0);

    QWinJumpListCategory *cat1 = new QWinJumpListCategory(QStringLiteral("tmp"));
    QCOMPARE(cat1->title(), QStringLiteral("tmp"));
    cat1->setTitle(QStringLiteral("cat1"));
    QCOMPARE(cat1->title(), QStringLiteral("cat1"));

    jumplist.addCategory(cat1);
    QCOMPARE(jumplist.categories().count(), 1);
    QCOMPARE(jumplist.categories().at(0), cat1);

    QWinJumpListCategory *cat2 = jumplist.addCategory(QStringLiteral("cat2"));
    QCOMPARE(cat2->title(), QStringLiteral("cat2"));

    QCOMPARE(jumplist.categories().count(), 2);
    QCOMPARE(jumplist.categories().at(0), cat1);
    QCOMPARE(jumplist.categories().at(1), cat2);

    jumplist.clear();
    QVERIFY(jumplist.categories().isEmpty());
}

void tst_QWinJumpList::testItems_data()
{
    QTest::addColumn<QWinJumpListItem::Type>("type");

    QTest::newRow("destination") << QWinJumpListItem::Destination;
    QTest::newRow("separator") << QWinJumpListItem::Separator;
    QTest::newRow("link") << QWinJumpListItem::Link;
}

void tst_QWinJumpList::testItems()
{
    QFETCH(QWinJumpListItem::Type, type);

    QWinJumpListItem item(type);
    QCOMPARE(item.type(), type);
    QVERIFY(item.filePath().isNull());
    QVERIFY(item.workingDirectory().isNull());
    QVERIFY(item.icon().isNull());
    QVERIFY(item.title().isNull());
    QVERIFY(item.description().isNull());
    QVERIFY(item.arguments().isEmpty());

    item.setType(QWinJumpListItem::Destination);
    QCOMPARE(item.type(), QWinJumpListItem::Destination);

    item.setFilePath(QCoreApplication::applicationFilePath());
    QCOMPARE(item.filePath(), QCoreApplication::applicationFilePath());

    item.setWorkingDirectory(QCoreApplication::applicationDirPath());
    QCOMPARE(item.workingDirectory(), QCoreApplication::applicationDirPath());

    QIcon icon("qrc:/qt-project.org/qmessagebox/images/qtlogo-64.png");
    item.setIcon(icon);
    QCOMPARE(item.icon().cacheKey(), icon.cacheKey());

    item.setTitle(QStringLiteral("tst_QWinJumpList"));
    QCOMPARE(item.title(), QStringLiteral("tst_QWinJumpList"));

    item.setDescription(QStringLiteral("QtWinExras - tst_QWinJumpList"));
    QCOMPARE(item.description(), QStringLiteral("QtWinExras - tst_QWinJumpList"));

    item.setArguments(QCoreApplication::arguments());
    QCOMPARE(item.arguments(), QCoreApplication::arguments());
}

QTEST_MAIN(tst_QWinJumpList)

#include "tst_qwinjumplist.moc"
