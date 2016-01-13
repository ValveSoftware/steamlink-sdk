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

#include <QtCore/QThread>
#include <QtCore/QUrl>
#include <QtCore/QFileInfo>

#include <QtHelp/QHelpEngine>
#include <QtHelp/QHelpContentWidget>

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


class tst_QHelpContentModel : public QObject
{
    Q_OBJECT

private slots:
    void init();

    void setupContents();
    void contentItemAt();

private:
    QString m_colFile;
};

void tst_QHelpContentModel::init()
{
    // defined in profile
    QString path = QLatin1String(SRCDIR);

    m_colFile = path + QLatin1String("/data/col.qhc");
    if (QFile::exists(m_colFile))
        QDir::current().remove(m_colFile);
    if (!QFile::copy(path + "/data/collection.qhc", m_colFile))
        QFAIL("Cannot copy file!");
    QFile f(m_colFile);
    f.setPermissions(QFile::WriteUser|QFile::ReadUser);
}

void tst_QHelpContentModel::setupContents()
{
    QHelpEngine h(m_colFile, 0);
    QHelpContentModel *m = h.contentModel();
    SignalWaiter w;
    connect(m, SIGNAL(contentsCreated()),
        &w, SLOT(stopWaiting()));
    w.start();
    h.setupData();
    int i = 0;
    while (w.isRunning() && i++ < 10)
        QTest::qWait(500);

    QCOMPARE(h.currentFilter(), QString("unfiltered"));
    QCOMPARE(m->rowCount(), 4);

    w.start();
    h.setCurrentFilter("Custom Filter 1");
    i = 0;
    while (w.isRunning() && i++ < 10)
        QTest::qWait(500);

     QCOMPARE(m->rowCount(), 1);
}

void tst_QHelpContentModel::contentItemAt()
{
    QHelpEngine h(m_colFile, 0);
    QHelpContentModel *m = h.contentModel();
    SignalWaiter w;
    connect(m, SIGNAL(contentsCreated()),
        &w, SLOT(stopWaiting()));
    w.start();
    h.setupData();
    int i = 0;
    while (w.isRunning() && i++ < 10)
        QTest::qWait(500);

    QCOMPARE(h.currentFilter(), QString("unfiltered"));

    QModelIndex root = m->index(0, 0);
    if (!root.isValid())
        QFAIL("Cannot retrieve root item!");
    QHelpContentItem *item = m->contentItemAt(root);
    if (!item)
        QFAIL("Cannot retrieve content item!");
    QCOMPARE(item->title(), QString("qmake Manual"));

    item = m->contentItemAt(m->index(4, 0, root));
    QCOMPARE(item->title(), QString("qmake Concepts"));

    item = m->contentItemAt(m->index(3, 0));
    QCOMPARE(item->title(), QString("Fancy Manual"));

    w.start();
    h.setCurrentFilter("Custom Filter 1");
    i = 0;
    while (w.isRunning() && i++ < 10)
        QTest::qWait(500);

    item = m->contentItemAt(m->index(0, 0));
    QCOMPARE(item->title(), QString("Test Manual"));
}

QTEST_MAIN(tst_QHelpContentModel)
#include "tst_qhelpcontentmodel.moc"
