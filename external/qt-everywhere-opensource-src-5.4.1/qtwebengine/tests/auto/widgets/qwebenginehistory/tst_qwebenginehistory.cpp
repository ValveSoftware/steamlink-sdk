/*
    Copyright (C) 2008 Holger Hans Peter Freyther

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include <QtTest/QtTest>
#include <QAction>

#include "../util.h"
#include "qwebenginepage.h"
#include "qwebengineview.h"
#include "qwebenginehistory.h"
#include "qdebug.h"

class tst_QWebEngineHistory : public QObject
{
    Q_OBJECT

public:
    tst_QWebEngineHistory();
    virtual ~tst_QWebEngineHistory();

protected :
    void loadPage(int nr)
    {
        page->load(QUrl("qrc:/resources/page" + QString::number(nr) + ".html"));
        loadFinishedBarrier->ensureSignalEmitted();
    }

public Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

private Q_SLOTS:
    void title();
    void lastVisited();
    void count();
    void back();
    void forward();
    void itemAt();
    void goToItem();
    void items();
    void backForwardItems();
    void serialize_1(); //QWebEngineHistory countity
    void serialize_2(); //QWebEngineHistory index
    void serialize_3(); //QWebEngineHistoryItem
    // Those tests shouldn't crash
    void saveAndRestore_crash_1();
    void saveAndRestore_crash_2();
    void saveAndRestore_crash_3();
    void saveAndRestore_crash_4();

    void popPushState_data();
    void popPushState();
    void clear();
    void historyItemFromDeletedPage();
    void restoreIncompatibleVersion1();


private:
    QWebEnginePage* page;
    QWebEngineHistory* hist;
    QScopedPointer<SignalBarrier> loadFinishedBarrier;
    int histsize;
};

tst_QWebEngineHistory::tst_QWebEngineHistory()
{
}

tst_QWebEngineHistory::~tst_QWebEngineHistory()
{
}

void tst_QWebEngineHistory::initTestCase()
{
}

void tst_QWebEngineHistory::init()
{
    page = new QWebEnginePage(this);
    loadFinishedBarrier.reset(new SignalBarrier(page, SIGNAL(loadFinished(bool))));

    for (int i = 1;i < 6;i++) {
        loadPage(i);
    }
    hist = page->history();
    histsize = 5;
}

void tst_QWebEngineHistory::cleanup()
{
    loadFinishedBarrier.reset();
    delete page;
}

/**
  * Check QWebEngineHistoryItem::title() method
  */
void tst_QWebEngineHistory::title()
{
    QCOMPARE(hist->currentItem().title(), QString("page5"));
}

void tst_QWebEngineHistory::lastVisited()
{
    // Check that the conversion from Chromium's internal time format went well.
    QVERIFY(qAbs(hist->itemAt(0).lastVisited().secsTo(QDateTime::currentDateTime())) < 60);
}

/**
  * Check QWebEngineHistory::count() method
  */
void tst_QWebEngineHistory::count()
{
    QCOMPARE(hist->count(), histsize);
}

/**
  * Check QWebEngineHistory::back() method
  */
void tst_QWebEngineHistory::back()
{
    SignalBarrier titleChangedBarrier(page, SIGNAL(titleChanged(const QString&)));

    for (int i = histsize;i > 1;i--) {
        QCOMPARE(toPlainTextSync(page), QString("page") + QString::number(i));
        hist->back();
        loadFinishedBarrier->ensureSignalEmitted();
        QVERIFY(titleChangedBarrier.ensureSignalEmitted());
    }
    //try one more time (too many). crash test
    hist->back();
    QCOMPARE(toPlainTextSync(page), QString("page1"));
}

/**
  * Check QWebEngineHistory::forward() method
  */
void tst_QWebEngineHistory::forward()
{
    //rewind history :-)
    while (hist->canGoBack()) {
        hist->back();
        loadFinishedBarrier->ensureSignalEmitted();
    }

    SignalBarrier titleChangedBarrier(page, SIGNAL(titleChanged(const QString&)));
    for (int i = 1;i < histsize;i++) {
        QCOMPARE(toPlainTextSync(page), QString("page") + QString::number(i));
        hist->forward();
        loadFinishedBarrier->ensureSignalEmitted();
        QVERIFY(titleChangedBarrier.ensureSignalEmitted());
    }
    //try one more time (too many). crash test
    hist->forward();
    QCOMPARE(toPlainTextSync(page), QString("page") + QString::number(histsize));
}

/**
  * Check QWebEngineHistory::itemAt() method
  */
void tst_QWebEngineHistory::itemAt()
{
    for (int i = 1;i < histsize;i++) {
        QCOMPARE(hist->itemAt(i - 1).title(), QString("page") + QString::number(i));
        QVERIFY(hist->itemAt(i - 1).isValid());
    }
    //check out of range values
    QVERIFY(!hist->itemAt(-1).isValid());
    QVERIFY(!hist->itemAt(histsize).isValid());
}

/**
  * Check QWebEngineHistory::goToItem() method
  */
void tst_QWebEngineHistory::goToItem()
{
    QWebEngineHistoryItem current = hist->currentItem();
    hist->back();
    loadFinishedBarrier->ensureSignalEmitted();
    hist->back();
    loadFinishedBarrier->ensureSignalEmitted();
    QVERIFY(hist->currentItem().title() != current.title());
    hist->goToItem(current);
    loadFinishedBarrier->ensureSignalEmitted();
    QCOMPARE(hist->currentItem().title(), current.title());
}

/**
  * Check QWebEngineHistory::items() method
  */
void tst_QWebEngineHistory::items()
{
    QList<QWebEngineHistoryItem> items = hist->items();
    //check count
    QCOMPARE(histsize, items.count());

    //check order
    for (int i = 1;i <= histsize;i++) {
        QCOMPARE(items.at(i - 1).title(), QString("page") + QString::number(i));
    }
}

void tst_QWebEngineHistory::backForwardItems()
{
    hist->back();
    loadFinishedBarrier->ensureSignalEmitted();
    hist->back();
    loadFinishedBarrier->ensureSignalEmitted();
    QCOMPARE(hist->items().size(), 5);
    QCOMPARE(hist->backItems(100).size(), 2);
    QCOMPARE(hist->backItems(1).size(), 1);
    QCOMPARE(hist->forwardItems(100).size(), 2);
    QCOMPARE(hist->forwardItems(1).size(), 1);
}

/**
  * Check history state after serialization (pickle, persistent..) method
  * Checks history size, history order
  */
void tst_QWebEngineHistory::serialize_1()
{
    QByteArray tmp;  //buffer
    QDataStream save(&tmp, QIODevice::WriteOnly); //here data will be saved
    QDataStream load(&tmp, QIODevice::ReadOnly); //from here data will be loaded

    save << *hist;
    QVERIFY(save.status() == QDataStream::Ok);
    QCOMPARE(hist->count(), histsize);

    //check size of history
    //load next page to find differences
    loadPage(6);
    QCOMPARE(hist->count(), histsize + 1);
    load >> *hist;
    QVERIFY(load.status() == QDataStream::Ok);
    QCOMPARE(hist->count(), histsize);

    //check order of historyItems
    QList<QWebEngineHistoryItem> items = hist->items();
    for (int i = 1;i <= histsize;i++) {
        QCOMPARE(items.at(i - 1).title(), QString("page") + QString::number(i));
    }
}

/**
  * Check history state after serialization (pickle, persistent..) method
  * Checks history currentIndex value
  */
void tst_QWebEngineHistory::serialize_2()
{
    QByteArray tmp;  //buffer
    QDataStream save(&tmp, QIODevice::WriteOnly); //here data will be saved
    QDataStream load(&tmp, QIODevice::ReadOnly); //from here data will be loaded

    // Force a "same document" navigation.
    page->load(page->url().toString() + QLatin1String("#dummyAnchor"));
    loadFinishedBarrier->ensureSignalEmitted();

    int initialCurrentIndex = hist->currentItemIndex();

    hist->back();
    loadFinishedBarrier->ensureSignalEmitted();
    hist->back();
    loadFinishedBarrier->ensureSignalEmitted();
    hist->back();
    loadFinishedBarrier->ensureSignalEmitted();
    //check if current index was changed (make sure that it is not last item)
    QVERIFY(hist->currentItemIndex() != initialCurrentIndex);
    //save current index
    int oldCurrentIndex = hist->currentItemIndex();

    save << *hist;
    QVERIFY(save.status() == QDataStream::Ok);
    load >> *hist;
    QVERIFY(load.status() == QDataStream::Ok);
    // Restoring the history will trigger a load.
    loadFinishedBarrier->ensureSignalEmitted();

    //check current index
    QCOMPARE(hist->currentItemIndex(), oldCurrentIndex);

    hist->forward();
    loadFinishedBarrier->ensureSignalEmitted();
    hist->forward();
    loadFinishedBarrier->ensureSignalEmitted();
    hist->forward();
    loadFinishedBarrier->ensureSignalEmitted();
    QCOMPARE(hist->currentItemIndex(), initialCurrentIndex);
}

/**
  * Check history state after serialization (pickle, persistent..) method
  * Checks QWebEngineHistoryItem public property after serialization
  */
void tst_QWebEngineHistory::serialize_3()
{
    QByteArray tmp;  //buffer
    QDataStream save(&tmp, QIODevice::WriteOnly); //here data will be saved
    QDataStream load(&tmp, QIODevice::ReadOnly); //from here data will be loaded

    //prepare two different history items
    QWebEngineHistoryItem a = hist->currentItem();

    //check properties BEFORE serialization
    QString title(a.title());
    QDateTime lastVisited(a.lastVisited());
    QUrl originalUrl(a.originalUrl());
    QUrl url(a.url());

    save << *hist;
    QVERIFY(save.status() == QDataStream::Ok);
    QVERIFY(!load.atEnd());
    hist->clear();
    QVERIFY(hist->count() == 1);
    load >> *hist;
    QVERIFY(load.status() == QDataStream::Ok);
    QWebEngineHistoryItem b = hist->currentItem();

    //check properties AFTER serialization
    QCOMPARE(b.title(), title);
    QCOMPARE(b.lastVisited(), lastVisited);
    QCOMPARE(b.originalUrl(), originalUrl);
    QCOMPARE(b.url(), url);

    //Check if all data was read
    QVERIFY(load.atEnd());
}

static void saveHistory(QWebEngineHistory* history, QByteArray* in)
{
    in->clear();
    QDataStream save(in, QIODevice::WriteOnly);
    save << *history;
}

static void restoreHistory(QWebEngineHistory* history, QByteArray* out)
{
    QDataStream load(out, QIODevice::ReadOnly);
    load >> *history;
}

void tst_QWebEngineHistory::saveAndRestore_crash_1()
{
    QByteArray buffer;
    saveHistory(hist, &buffer);
    for (unsigned i = 0; i < 5; i++) {
        restoreHistory(hist, &buffer);
        saveHistory(hist, &buffer);
    }
}

void tst_QWebEngineHistory::saveAndRestore_crash_2()
{
    QByteArray buffer;
    saveHistory(hist, &buffer);
    QWebEnginePage* page2 = new QWebEnginePage(this);
    QWebEngineHistory* hist2 = page2->history();
    for (unsigned i = 0; i < 5; i++) {
        restoreHistory(hist2, &buffer);
        saveHistory(hist2, &buffer);
    }
    delete page2;
}

void tst_QWebEngineHistory::saveAndRestore_crash_3()
{
    QByteArray buffer;
    saveHistory(hist, &buffer);
    QWebEnginePage* page2 = new QWebEnginePage(this);
    QWebEngineHistory* hist1 = hist;
    QWebEngineHistory* hist2 = page2->history();
    for (unsigned i = 0; i < 5; i++) {
        restoreHistory(hist1, &buffer);
        restoreHistory(hist2, &buffer);
        QVERIFY(hist1->count() == hist2->count());
        QVERIFY(hist1->count() == histsize);
        hist2->back();
        saveHistory(hist2, &buffer);
        hist2->clear();
    }
    delete page2;
}

void tst_QWebEngineHistory::saveAndRestore_crash_4()
{
#if !defined(QWEBENGINESETTINGS)
    QSKIP("QWEBENGINESETTINGS");
#else
    QByteArray buffer;
    saveHistory(hist, &buffer);

    QWebEnginePage* page2 = new QWebEnginePage(this);
    // The initial crash was in PageCache.
    page2->settings()->setMaximumPagesInCache(3);

    // Load the history in a new page, waiting for the load to finish.
    QEventLoop waitForLoadFinished;
    QObject::connect(page2, SIGNAL(loadFinished(bool)), &waitForLoadFinished, SLOT(quit()), Qt::QueuedConnection);
    QDataStream load(&buffer, QIODevice::ReadOnly);
    load >> *page2->history();
    waitForLoadFinished.exec();

    delete page2;
    // Give some time for the PageCache cleanup 0-timer to fire.
    QTest::qWait(50);
#endif
}

void tst_QWebEngineHistory::popPushState_data()
{
    QTest::addColumn<QString>("script");
    QTest::newRow("pushState") << "history.pushState(123, \"foo\");";
    QTest::newRow("replaceState") << "history.replaceState(\"a\", \"b\");";
    QTest::newRow("back") << "history.back();";
    QTest::newRow("forward") << "history.forward();";
}

/** Crash test, WebKit bug 38840 (https://bugs.webkit.org/show_bug.cgi?id=38840) */
void tst_QWebEngineHistory::popPushState()
{
    QFETCH(QString, script);
    QWebEnginePage page;
    QSignalSpy spyLoadFinished(&page, SIGNAL(loadFinished(bool)));
    page.setHtml("<html><body>long live Qt!</body></html>");
    QTRY_COMPARE(spyLoadFinished.count(), 1);
    evaluateJavaScriptSync(&page, script);
}

/** ::clear */
void tst_QWebEngineHistory::clear()
{
    QByteArray buffer;

    QAction* actionBack = page->action(QWebEnginePage::Back);
    QVERIFY(actionBack->isEnabled());
    saveHistory(hist, &buffer);
    QVERIFY(hist->count() > 1);
    hist->clear();
    QVERIFY(hist->count() == 1);  // Leave current item.
    QVERIFY(!actionBack->isEnabled());

    QWebEnginePage* page2 = new QWebEnginePage(this);
    QWebEngineHistory* hist2 = page2->history();
    QVERIFY(hist2->count() == 0);
    hist2->clear();
    QVERIFY(hist2->count() == 0); // Do not change anything.
    delete page2;
}

void tst_QWebEngineHistory::historyItemFromDeletedPage()
{
    QList<QWebEngineHistoryItem> items = page->history()->items();
    delete page;
    page = 0;

    foreach (QWebEngineHistoryItem item, items) {
        QVERIFY(!item.isValid());
        QCOMPARE(item.originalUrl(), QUrl());
        QCOMPARE(item.url(), QUrl());
        QCOMPARE(item.title(), QString());
        QCOMPARE(item.lastVisited(), QDateTime());
    }
}

// static void dumpCurrentVersion(QWebEngineHistory* history)
// {
//     QByteArray buffer;
//     saveHistory(history, &buffer);
//     printf("    static const char version1Dump[] = {");
//     for (int i = 0; i < buffer.size(); ++i) {
//         bool newLine = !(i % 15);
//         bool last = i == buffer.size() - 1;
//         printf("%s0x%.2x%s", newLine ? "\n        " : "", (unsigned char)buffer[i], last ? "" : ", ");
//     }
//     printf("};\n");
// }

void tst_QWebEngineHistory::restoreIncompatibleVersion1()
{
    // Uncomment this code to generate a dump similar to the one below with the current stream version.
    // dumpCurrentVersion(hist);
    static const unsigned char version1Dump[] = {
        0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00,
        0x32, 0x00, 0x71, 0x00, 0x72, 0x00, 0x63, 0x00, 0x3a, 0x00, 0x2f, 0x00, 0x72, 0x00, 0x65,
        0x00, 0x73, 0x00, 0x6f, 0x00, 0x75, 0x00, 0x72, 0x00, 0x63, 0x00, 0x65, 0x00, 0x73, 0x00,
        0x2f, 0x00, 0x70, 0x00, 0x61, 0x00, 0x67, 0x00, 0x65, 0x00, 0x31, 0x00, 0x2e, 0x00, 0x68,
        0x00, 0x74, 0x00, 0x6d, 0x00, 0x6c, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x70, 0x00, 0x61, 0x00,
        0x67, 0x00, 0x65, 0x00, 0x31, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x32, 0x00, 0x71, 0x00, 0x72, 0x00, 0x63, 0x00, 0x3a, 0x00,
        0x2f, 0x00, 0x72, 0x00, 0x65, 0x00, 0x73, 0x00, 0x6f, 0x00, 0x75, 0x00, 0x72, 0x00, 0x63,
        0x00, 0x65, 0x00, 0x73, 0x00, 0x2f, 0x00, 0x70, 0x00, 0x61, 0x00, 0x67, 0x00, 0x65, 0x00,
        0x31, 0x00, 0x2e, 0x00, 0x68, 0x00, 0x74, 0x00, 0x6d, 0x00, 0x6c, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x3f, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x32,
        0x00, 0x71, 0x00, 0x72, 0x00, 0x63, 0x00, 0x3a, 0x00, 0x2f, 0x00, 0x72, 0x00, 0x65, 0x00,
        0x73, 0x00, 0x6f, 0x00, 0x75, 0x00, 0x72, 0x00, 0x63, 0x00, 0x65, 0x00, 0x73, 0x00, 0x2f,
        0x00, 0x70, 0x00, 0x61, 0x00, 0x67, 0x00, 0x65, 0x00, 0x32, 0x00, 0x2e, 0x00, 0x68, 0x00,
        0x74, 0x00, 0x6d, 0x00, 0x6c, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x70, 0x00, 0x61, 0x00, 0x67,
        0x00, 0x65, 0x00, 0x32, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x32, 0x00, 0x71, 0x00, 0x72, 0x00, 0x63, 0x00, 0x3a, 0x00, 0x2f,
        0x00, 0x72, 0x00, 0x65, 0x00, 0x73, 0x00, 0x6f, 0x00, 0x75, 0x00, 0x72, 0x00, 0x63, 0x00,
        0x65, 0x00, 0x73, 0x00, 0x2f, 0x00, 0x70, 0x00, 0x61, 0x00, 0x67, 0x00, 0x65, 0x00, 0x32,
        0x00, 0x2e, 0x00, 0x68, 0x00, 0x74, 0x00, 0x6d, 0x00, 0x6c, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x3f, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x32, 0x00,
        0x71, 0x00, 0x72, 0x00, 0x63, 0x00, 0x3a, 0x00, 0x2f, 0x00, 0x72, 0x00, 0x65, 0x00, 0x73,
        0x00, 0x6f, 0x00, 0x75, 0x00, 0x72, 0x00, 0x63, 0x00, 0x65, 0x00, 0x73, 0x00, 0x2f, 0x00,
        0x70, 0x00, 0x61, 0x00, 0x67, 0x00, 0x65, 0x00, 0x33, 0x00, 0x2e, 0x00, 0x68, 0x00, 0x74,
        0x00, 0x6d, 0x00, 0x6c, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x70, 0x00, 0x61, 0x00, 0x67, 0x00,
        0x65, 0x00, 0x33, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x32, 0x00, 0x71, 0x00, 0x72, 0x00, 0x63, 0x00, 0x3a, 0x00, 0x2f, 0x00,
        0x72, 0x00, 0x65, 0x00, 0x73, 0x00, 0x6f, 0x00, 0x75, 0x00, 0x72, 0x00, 0x63, 0x00, 0x65,
        0x00, 0x73, 0x00, 0x2f, 0x00, 0x70, 0x00, 0x61, 0x00, 0x67, 0x00, 0x65, 0x00, 0x33, 0x00,
        0x2e, 0x00, 0x68, 0x00, 0x74, 0x00, 0x6d, 0x00, 0x6c, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f,
        0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x32, 0x00, 0x71,
        0x00, 0x72, 0x00, 0x63, 0x00, 0x3a, 0x00, 0x2f, 0x00, 0x72, 0x00, 0x65, 0x00, 0x73, 0x00,
        0x6f, 0x00, 0x75, 0x00, 0x72, 0x00, 0x63, 0x00, 0x65, 0x00, 0x73, 0x00, 0x2f, 0x00, 0x70,
        0x00, 0x61, 0x00, 0x67, 0x00, 0x65, 0x00, 0x34, 0x00, 0x2e, 0x00, 0x68, 0x00, 0x74, 0x00,
        0x6d, 0x00, 0x6c, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x70, 0x00, 0x61, 0x00, 0x67, 0x00, 0x65,
        0x00, 0x34, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x32, 0x00, 0x71, 0x00, 0x72, 0x00, 0x63, 0x00, 0x3a, 0x00, 0x2f, 0x00, 0x72,
        0x00, 0x65, 0x00, 0x73, 0x00, 0x6f, 0x00, 0x75, 0x00, 0x72, 0x00, 0x63, 0x00, 0x65, 0x00,
        0x73, 0x00, 0x2f, 0x00, 0x70, 0x00, 0x61, 0x00, 0x67, 0x00, 0x65, 0x00, 0x34, 0x00, 0x2e,
        0x00, 0x68, 0x00, 0x74, 0x00, 0x6d, 0x00, 0x6c, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xf0,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x32, 0x00, 0x71, 0x00,
        0x72, 0x00, 0x63, 0x00, 0x3a, 0x00, 0x2f, 0x00, 0x72, 0x00, 0x65, 0x00, 0x73, 0x00, 0x6f,
        0x00, 0x75, 0x00, 0x72, 0x00, 0x63, 0x00, 0x65, 0x00, 0x73, 0x00, 0x2f, 0x00, 0x70, 0x00,
        0x61, 0x00, 0x67, 0x00, 0x65, 0x00, 0x35, 0x00, 0x2e, 0x00, 0x68, 0x00, 0x74, 0x00, 0x6d,
        0x00, 0x6c, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x70, 0x00, 0x61, 0x00, 0x67, 0x00, 0x65, 0x00,
        0x35, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x32, 0x00, 0x71, 0x00, 0x72, 0x00, 0x63, 0x00, 0x3a, 0x00, 0x2f, 0x00, 0x72, 0x00,
        0x65, 0x00, 0x73, 0x00, 0x6f, 0x00, 0x75, 0x00, 0x72, 0x00, 0x63, 0x00, 0x65, 0x00, 0x73,
        0x00, 0x2f, 0x00, 0x70, 0x00, 0x61, 0x00, 0x67, 0x00, 0x65, 0x00, 0x35, 0x00, 0x2e, 0x00,
        0x68, 0x00, 0x74, 0x00, 0x6d, 0x00, 0x6c, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    QByteArray version1(reinterpret_cast<const char*>(version1Dump), sizeof(version1Dump));
    QDataStream stream(&version1, QIODevice::ReadOnly);

    // This should fail to load, the history should be cleared and the stream should be broken.
    stream >> *hist;
    QEXPECT_FAIL("", "Behavior change: A broken stream won't clear the history in QtWebEngine.", Continue);
    QVERIFY(!hist->canGoBack());
    QVERIFY(!hist->canGoForward());
    QVERIFY(stream.status() == QDataStream::ReadCorruptData);
}

QTEST_MAIN(tst_QWebEngineHistory)
#include "tst_qwebenginehistory.moc"
