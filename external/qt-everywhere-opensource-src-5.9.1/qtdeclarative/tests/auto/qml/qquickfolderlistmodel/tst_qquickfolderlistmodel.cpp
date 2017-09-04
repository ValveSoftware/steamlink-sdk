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
#include <QtTest/QSignalSpy>
#include <QtQml/qqmlengine.h>
#include <QtQml/qqmlcomponent.h>
#include <QtCore/qdir.h>
#include <QtCore/qfile.h>
#include <QtCore/qabstractitemmodel.h>
#include <QDebug>
#include "../../shared/util.h"

#if defined (Q_OS_WIN)
#include <qt_windows.h>
#endif

// From qquickfolderlistmodel.h
const int FileNameRole = Qt::UserRole+1;
enum SortField { Unsorted, Name, Time, Size, Type };

class tst_qquickfolderlistmodel : public QQmlDataTest
{
    Q_OBJECT
public:
    tst_qquickfolderlistmodel() : removeStart(0), removeEnd(0) {}

public slots:
    void removed(const QModelIndex &, int start, int end) {
        removeStart = start;
        removeEnd = end;
    }

private slots:
    void initTestCase();
    void basicProperties();
    void showFiles();
    void resetFiltering();
    void nameFilters();
    void refresh();
    void cdUp();
#ifdef Q_OS_WIN32
    // WinCE/WinRT do not have drive APIs, so let's execute this test only on desktop Windows.
    void changeDrive();
#endif
    void showDotAndDotDot();
    void showDotAndDotDot_data();
    void sortReversed();
    void introspectQrc();

private:
    void checkNoErrors(const QQmlComponent& component);
    QQmlEngine engine;

    int removeStart;
    int removeEnd;
};

void tst_qquickfolderlistmodel::checkNoErrors(const QQmlComponent& component)
{
    // Wait until the component is ready
    QTRY_VERIFY(component.isReady() || component.isError());

    if (component.isError()) {
        QList<QQmlError> errors = component.errors();
        for (int ii = 0; ii < errors.count(); ++ii) {
            const QQmlError &error = errors.at(ii);
            QByteArray errorStr = QByteArray::number(error.line()) + ':' +
                                  QByteArray::number(error.column()) + ':' +
                                  error.description().toUtf8();
            qWarning() << errorStr;
        }
    }
    QVERIFY(!component.isError());
}

void tst_qquickfolderlistmodel::initTestCase()
{
    // The tests rely on a fixed number of files in the directory with the qml files
    // (the data dir), so disable the disk cache to avoid creating .qmlc files and
    // confusing the test.
    qputenv("QML_DISABLE_DISK_CACHE", "1");
    QQmlDataTest::initTestCase();
}

void tst_qquickfolderlistmodel::basicProperties()
{
    QQmlComponent component(&engine, testFileUrl("basic.qml"));
    checkNoErrors(component);

    QAbstractListModel *flm = qobject_cast<QAbstractListModel*>(component.create());
    QVERIFY(flm != 0);
    QCOMPARE(flm->property("nameFilters").toStringList(), QStringList() << "*.qml"); // from basic.qml
    QCOMPARE(flm->property("folder").toUrl(), QUrl::fromLocalFile(QDir::currentPath()));

    // wait for the initial directory listing (it will find at least the "data" dir,
    // and other dirs on Windows).
    QTRY_VERIFY(flm->property("count").toInt() > 0);

    QSignalSpy folderChangedSpy(flm, SIGNAL(folderChanged()));
    flm->setProperty("folder", dataDirectoryUrl());
    QVERIFY(folderChangedSpy.wait());
    QCOMPARE(flm->property("count").toInt(), 8);
    QCOMPARE(flm->property("folder").toUrl(), dataDirectoryUrl());
    QCOMPARE(flm->property("parentFolder").toUrl(), QUrl::fromLocalFile(QDir(directory()).canonicalPath()));
    QCOMPARE(flm->property("sortField").toInt(), int(Name));
    QCOMPARE(flm->property("nameFilters").toStringList(), QStringList() << "*.qml");
    QCOMPARE(flm->property("sortReversed").toBool(), false);
    QCOMPARE(flm->property("showFiles").toBool(), true);
    QCOMPARE(flm->property("showDirs").toBool(), true);
    QCOMPARE(flm->property("showDotAndDotDot").toBool(), false);
    QCOMPARE(flm->property("showOnlyReadable").toBool(), false);
    QCOMPARE(flm->data(flm->index(0),FileNameRole).toString(), QLatin1String("basic.qml"));
    QCOMPARE(flm->data(flm->index(1),FileNameRole).toString(), QLatin1String("dummy.qml"));

    flm->setProperty("folder",QUrl::fromLocalFile(""));
    QCOMPARE(flm->property("folder").toUrl(), QUrl::fromLocalFile(""));
}

void tst_qquickfolderlistmodel::showFiles()
{
    QQmlComponent component(&engine, testFileUrl("basic.qml"));
    checkNoErrors(component);

    QAbstractListModel *flm = qobject_cast<QAbstractListModel*>(component.create());
    QVERIFY(flm != 0);

    flm->setProperty("folder", dataDirectoryUrl());
    QTRY_COMPARE(flm->property("count").toInt(), 8); // wait for refresh
    QCOMPARE(flm->property("showFiles").toBool(), true);

    flm->setProperty("showFiles", false);
    QCOMPARE(flm->property("showFiles").toBool(), false);
    QTRY_COMPARE(flm->property("count").toInt(), 2); // wait for refresh
}

void tst_qquickfolderlistmodel::resetFiltering()
{
    // see QTBUG-17837
    QQmlComponent component(&engine, testFileUrl("resetFiltering.qml"));
    checkNoErrors(component);

    QAbstractListModel *flm = qobject_cast<QAbstractListModel*>(component.create());
    QVERIFY(flm != 0);

    flm->setProperty("folder", testFileUrl("resetfiltering"));
    // _q_directoryUpdated may be triggered if model was empty before, but there won't be a rowsRemoved signal
    QTRY_COMPARE(flm->property("count").toInt(),3); // all files visible

    flm->setProperty("folder", testFileUrl("resetfiltering/innerdir"));
    // _q_directoryChanged is triggered so it's a total model refresh
    QTRY_COMPARE(flm->property("count").toInt(),1); // should just be "test2.txt" visible

    flm->setProperty("folder", testFileUrl("resetfiltering"));
    // _q_directoryChanged is triggered so it's a total model refresh
    QTRY_COMPARE(flm->property("count").toInt(),3); // all files visible
}

void tst_qquickfolderlistmodel::nameFilters()
{
    // see QTBUG-36576
    QQmlComponent component(&engine, testFileUrl("resetFiltering.qml"));
    checkNoErrors(component);

    QAbstractListModel *flm = qobject_cast<QAbstractListModel*>(component.create());
    QVERIFY(flm != 0);

    connect(flm, SIGNAL(rowsRemoved(QModelIndex,int,int)),
            this, SLOT(removed(QModelIndex,int,int)));

    QTRY_VERIFY(flm->rowCount() > 0);
    flm->setProperty("folder", testFileUrl("resetfiltering"));
    QTRY_COMPARE(flm->property("count").toInt(),3); // all files visible

    int count = flm->rowCount();
    flm->setProperty("nameFilters", QStringList() << "*.txt");
    // _q_directoryUpdated triggered with range 0:1
    QTRY_COMPARE(flm->property("count").toInt(),1);
    QCOMPARE(flm->data(flm->index(0),FileNameRole), QVariant("test.txt"));
    QCOMPARE(removeStart, 0);
    QCOMPARE(removeEnd, count-1);

    flm->setProperty("nameFilters", QStringList() << "*.html");
    QTRY_COMPARE(flm->property("count").toInt(),2);
    QCOMPARE(flm->data(flm->index(0),FileNameRole), QVariant("test1.html"));
    QCOMPARE(flm->data(flm->index(1),FileNameRole), QVariant("test2.html"));

    flm->setProperty("nameFilters", QStringList());
    QTRY_COMPARE(flm->property("count").toInt(),3); // all files visible
}

void tst_qquickfolderlistmodel::refresh()
{
    QQmlComponent component(&engine, testFileUrl("basic.qml"));
    checkNoErrors(component);

    QAbstractListModel *flm = qobject_cast<QAbstractListModel*>(component.create());
    QVERIFY(flm != 0);

    flm->setProperty("folder", dataDirectoryUrl());
    QTRY_COMPARE(flm->property("count").toInt(),8); // wait for refresh

    int count = flm->rowCount();

    connect(flm, SIGNAL(rowsRemoved(QModelIndex,int,int)),
            this, SLOT(removed(QModelIndex,int,int)));

    flm->setProperty("sortReversed", true);

    QTRY_COMPARE(removeStart, 0);
    QTRY_COMPARE(removeEnd, count-1); // wait for refresh
}

void tst_qquickfolderlistmodel::cdUp()
{
    enum { maxIterations = 50 };
    QQmlComponent component(&engine, testFileUrl("basic.qml"));
    checkNoErrors(component);

    QAbstractListModel *flm = qobject_cast<QAbstractListModel*>(component.create());
    QVERIFY(flm != 0);
    const QUrl startFolder = flm->property("folder").toUrl();
    QVERIFY(startFolder.isValid());

    // QTBUG-32139: Ensure navigating upwards terminates cleanly and does not
    // return invalid Urls like "file:".
    for (int i = 0; ; ++i) {
        const QVariant folderV = flm->property("parentFolder");
        const QUrl folder = folderV.toUrl();
        if (!folder.isValid())
            break;
        QVERIFY(folder.toString() != QLatin1String("file:"));
        QVERIFY2(i < maxIterations,
                 qPrintable(QString::fromLatin1("Unable to reach root after %1 iterations starting from %2, stuck at %3")
                            .arg(maxIterations).arg(QDir::toNativeSeparators(startFolder.toLocalFile()),
                                                    QDir::toNativeSeparators(folder.toLocalFile()))));
        flm->setProperty("folder", folderV);
    }
}

#ifdef Q_OS_WIN32
void tst_qquickfolderlistmodel::changeDrive()
{
    QSKIP("QTBUG-26728");
    class DriveMapper
    {
    public:
        DriveMapper(const QString &dataDir)
        {
            size_t stringLen = dataDir.length();
            targetPath = new wchar_t[stringLen+1];
            dataDir.toWCharArray(targetPath);
            targetPath[stringLen] = 0;

            DefineDosDevice(DDD_NO_BROADCAST_SYSTEM, L"X:", targetPath);
        }

        ~DriveMapper()
        {
            DefineDosDevice(DDD_EXACT_MATCH_ON_REMOVE | DDD_NO_BROADCAST_SYSTEM | DDD_REMOVE_DEFINITION, L"X:", targetPath);
            delete [] targetPath;
        }

    private:
        wchar_t *targetPath;
    };

    QString dataDir = testFile(0);
    DriveMapper dm(dataDir);
    QQmlComponent component(&engine, testFileUrl("basic.qml"));

    QAbstractListModel *flm = qobject_cast<QAbstractListModel*>(component.create());
    QVERIFY(flm != 0);

    QSignalSpy folderChangeSpy(flm, SIGNAL(folderChanged()));

    flm->setProperty("folder",QUrl::fromLocalFile(dataDir));
    QCOMPARE(flm->property("folder").toUrl(), QUrl::fromLocalFile(dataDir));
    QTRY_COMPARE(folderChangeSpy.count(), 1);

    flm->setProperty("folder",QUrl::fromLocalFile("X:/resetfiltering/"));
    QCOMPARE(flm->property("folder").toUrl(), QUrl::fromLocalFile("X:/resetfiltering/"));
    QTRY_COMPARE(folderChangeSpy.count(), 2);
}
#endif

void tst_qquickfolderlistmodel::showDotAndDotDot()
{
    QFETCH(QUrl, folder);
    QFETCH(QUrl, rootFolder);
    QFETCH(bool, showDotAndDotDot);
    QFETCH(bool, showDot);
    QFETCH(bool, showDotDot);

    QQmlComponent component(&engine, testFileUrl("showDotAndDotDot.qml"));
    checkNoErrors(component);

    QAbstractListModel *flm = qobject_cast<QAbstractListModel*>(component.create());
    QVERIFY(flm != 0);

    flm->setProperty("folder", folder);
    flm->setProperty("rootFolder", rootFolder);
    flm->setProperty("showDotAndDotDot", showDotAndDotDot);

    int count = 9;
    if (showDot) count++;
    if (showDotDot) count++;
    QTRY_COMPARE(flm->property("count").toInt(), count); // wait for refresh

    if (showDot)
        QCOMPARE(flm->data(flm->index(0),FileNameRole).toString(), QLatin1String("."));
    if (showDotDot)
        QCOMPARE(flm->data(flm->index(1),FileNameRole).toString(), QLatin1String(".."));
}

void tst_qquickfolderlistmodel::showDotAndDotDot_data()
{
    QTest::addColumn<QUrl>("folder");
    QTest::addColumn<QUrl>("rootFolder");
    QTest::addColumn<bool>("showDotAndDotDot");
    QTest::addColumn<bool>("showDot");
    QTest::addColumn<bool>("showDotDot");

    QTest::newRow("false") << dataDirectoryUrl() << QUrl() << false << false << false;
    QTest::newRow("true") << dataDirectoryUrl() << QUrl() << true << true << true;
    QTest::newRow("true but root") << dataDirectoryUrl() << dataDirectoryUrl() << true << true << false;
}

void tst_qquickfolderlistmodel::sortReversed()
{
    QQmlComponent component(&engine, testFileUrl("sortReversed.qml"));
    checkNoErrors(component);
    QAbstractListModel *flm = qobject_cast<QAbstractListModel*>(component.create());
    QVERIFY(flm != 0);
    flm->setProperty("folder", dataDirectoryUrl());
    QTRY_COMPARE(flm->property("count").toInt(), 9); // wait for refresh
    QCOMPARE(flm->data(flm->index(0),FileNameRole).toString(), QLatin1String("txtdir"));
}

void tst_qquickfolderlistmodel::introspectQrc()
{
    QQmlComponent component(&engine, testFileUrl("qrc.qml"));
    checkNoErrors(component);
    QAbstractListModel *flm = qobject_cast<QAbstractListModel*>(component.create());
    QVERIFY(flm != 0);
    QTRY_COMPARE(flm->property("count").toInt(), 1); // wait for refresh
    QCOMPARE(flm->data(flm->index(0),FileNameRole).toString(), QLatin1String("hello.txt"));
}

QTEST_MAIN(tst_qquickfolderlistmodel)

#include "tst_qquickfolderlistmodel.moc"
