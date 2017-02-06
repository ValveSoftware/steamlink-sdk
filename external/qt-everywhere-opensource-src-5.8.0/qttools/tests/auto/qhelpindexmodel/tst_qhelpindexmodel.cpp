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

#include <QtCore/QThread>
#include <QtCore/QUrl>
#include <QtCore/QFileInfo>

#include <QtHelp/QHelpEngine>
#include <QtHelp/QHelpIndexWidget>

class SignalWaiter : public QThread
{
    Q_OBJECT

public:
    SignalWaiter();
    void run();

public slots:
    void stopWaiting();

private:
    bool stop;
};

SignalWaiter::SignalWaiter()
{
    stop = false;
}

void SignalWaiter::run()
{
    while (!stop)
        msleep(500);
    stop = false;
}

void SignalWaiter::stopWaiting()
{
    stop = true;
}


class tst_QHelpIndexModel : public QObject
{
    Q_OBJECT

private slots:
    void init();

    void setupIndex();
    void filter();
    void linksForIndex();

private:
    QString m_colFile;
};

void tst_QHelpIndexModel::init()
{
    QString path = QLatin1String(SRCDIR);

    m_colFile = path + QLatin1String("/data/col.qhc");
    if (QFile::exists(m_colFile))
        QDir::current().remove(m_colFile);
    if (!QFile::copy(path + "/data/collection.qhc", m_colFile))
        QFAIL("Cannot copy file!");
    QFile f(m_colFile);
    f.setPermissions(QFile::WriteUser|QFile::ReadUser);
}

void tst_QHelpIndexModel::setupIndex()
{
    QHelpEngine h(m_colFile, 0);
    QHelpIndexModel *m = h.indexModel();
    SignalWaiter w;
    connect(m, SIGNAL(indexCreated()),
        &w, SLOT(stopWaiting()));
    w.start();
    h.setupData();
    int i = 0;
    while (w.isRunning() && i++ < 10)
        QTest::qWait(500);

    QCOMPARE(h.currentFilter(), QString("unfiltered"));
    QCOMPARE(m->stringList().count(), 19);

    w.start();
    h.setCurrentFilter("Custom Filter 1");
    i = 0;
    while (w.isRunning() && i++ < 10)
        QTest::qWait(500);

    QStringList lst;
    lst << "foo" << "bar" << "bla" << "einstein" << "newton";
    QCOMPARE(m->stringList().count(), 5);
    foreach (QString s, m->stringList())
        lst.removeAll(s);
    QCOMPARE(lst.isEmpty(), true);
}

void tst_QHelpIndexModel::filter()
{
    QHelpEngine h(m_colFile, 0);
    QHelpIndexModel *m = h.indexModel();
    SignalWaiter w;
    connect(m, SIGNAL(indexCreated()),
        &w, SLOT(stopWaiting()));
    w.start();
    h.setupData();
    int i = 0;
    while (w.isRunning() && i++ < 10)
        QTest::qWait(500);

    QCOMPARE(h.currentFilter(), QString("unfiltered"));
    QCOMPARE(m->stringList().count(), 19);

    m->filter("foo");
    QCOMPARE(m->stringList().count(), 2);

    m->filter("fo");
    QCOMPARE(m->stringList().count(), 3);

    m->filter("qmake");
    QCOMPARE(m->stringList().count(), 11);
}

void tst_QHelpIndexModel::linksForIndex()
{
    QHelpEngine h(m_colFile, 0);
    QHelpIndexModel *m = h.indexModel();
    SignalWaiter w;
    connect(m, SIGNAL(indexCreated()),
        &w, SLOT(stopWaiting()));
    w.start();
    h.setupData();
    int i = 0;
    while (w.isRunning() && i++ < 10)
        QTest::qWait(500);

    QCOMPARE(h.currentFilter(), QString("unfiltered"));
    QMap<QString, QUrl> map;
    map = m->linksForKeyword("foo");
    QCOMPARE(map.count(), 2);
    QCOMPARE(map.contains("Test Manual"), true);
    QCOMPARE(map.value("Test Manual"),
        QUrl("qthelp://trolltech.com.1.0.0.test/testFolder/test.html#foo"));

    QCOMPARE(map.contains("Fancy"), true);
    QCOMPARE(map.value("Fancy"),
        QUrl("qthelp://trolltech.com.1.0.0.test/testFolder/fancy.html#foo"));

    map = m->linksForKeyword("foobar");
    QCOMPARE(map.count(), 1);
    QCOMPARE(map.contains("Fancy"), true);

    map = m->linksForKeyword("notexisting");
    QCOMPARE(map.count(), 0);

    w.start();
    h.setCurrentFilter("Custom Filter 1");
    i = 0;
    while (w.isRunning() && i++ < 10)
        QTest::qWait(500);

    map = m->linksForKeyword("foo");
    QCOMPARE(map.count(), 1);
    QCOMPARE(map.contains("Test Manual"), true);
    QCOMPARE(map.value("Test Manual"),
        QUrl("qthelp://trolltech.com.1.0.0.test/testFolder/test.html#foo"));
}

QTEST_MAIN(tst_QHelpIndexModel)
#include "tst_qhelpindexmodel.moc"
