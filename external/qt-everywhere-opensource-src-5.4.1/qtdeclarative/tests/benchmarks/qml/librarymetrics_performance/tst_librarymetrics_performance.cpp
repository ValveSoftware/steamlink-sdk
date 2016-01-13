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

#include <qtest.h>
#include <qqml.h>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QDebug>

// This benchmark produces performance statistics
// for the standard set of elements, properties and expressions which
// are provided in the QtDeclarative library (QtQml and QtQuick).

#define AVERAGE_OVER_N 10
#define IGNORE_N_OUTLIERS 2

class ModuleApi : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int intProp READ intProp WRITE setIntProp NOTIFY intPropChanged)

public:
    ModuleApi() : m_intProp(42) { }
    ~ModuleApi() {}

    Q_INVOKABLE int intFunc() const { return 9000; }
    int intProp() const { return m_intProp; }
    void setIntProp(int v) { m_intProp = v; emit intPropChanged(); }

signals:
    void intPropChanged();

private:
    int m_intProp;
};

static QObject *module_api_factory(QQmlEngine*, QJSEngine*)
{
    return new ModuleApi;
}

class tst_librarymetrics_performance : public QObject
{
    Q_OBJECT

public:
    tst_librarymetrics_performance();
    ~tst_librarymetrics_performance();

private slots:
    void initTestCase() {}

    // ---------------------- performance tests:
    void compilation();
    void instantiation_cached();
    void instantiation();
    void positioners();

    // ---------------------- test row data:
    void metrics_data();
    void compilation_data() { metrics_data(); }
    void instantiation_cached_data() { metrics_data(); }
    void instantiation_data() { metrics_data(); }
    void positioners_data();

private:
    QQmlEngine *e;
};

static void cleanState(QQmlEngine **e)
{
    delete *e;
    qmlClearTypeRegistrations();
    *e = new QQmlEngine;
    qmlRegisterSingletonType<ModuleApi>("ModuleApi", 1, 0, "ModuleApi", module_api_factory);//Speed of qmlRegister<foo> not covered in this benchmark
    QCoreApplication::sendPostedEvents(0, QEvent::DeferredDelete);
    QCoreApplication::processEvents();
}

tst_librarymetrics_performance::tst_librarymetrics_performance()
    : e(new QQmlEngine)
{
}

tst_librarymetrics_performance::~tst_librarymetrics_performance()
{
    cleanState(&e);
}

static QUrl testFileUrl(const char * filename)
{
    return QUrl::fromLocalFile(QString(QLatin1String(filename)));
}

void tst_librarymetrics_performance::metrics_data()
{
    // all of the tests follow the same basic format.
    QTest::addColumn<QUrl>("qmlfile");

    QTest::newRow("001) item - empty") << testFileUrl("data/item.1.qml");
    QTest::newRow("002) item - uninitialised int prop") << testFileUrl("data/item.2.qml");
    QTest::newRow("003) item - uninitialised bool prop") << testFileUrl("data/item.3.qml");
    QTest::newRow("004) item - uninitialised string prop") << testFileUrl("data/item.4.qml");
    QTest::newRow("005) item - uninitialised real prop") << testFileUrl("data/item.5.qml");
    QTest::newRow("006) item - uninitialised variant prop") << testFileUrl("data/item.6.qml");
    QTest::newRow("007) item - uninitialised Item prop") << testFileUrl("data/item.7.qml");
    QTest::newRow("008) item - uninitialised var prop") << testFileUrl("data/item.8.qml");
    QTest::newRow("009) item - initialised int prop") << testFileUrl("data/item.9.qml");
    QTest::newRow("010) item - initialised bool prop") << testFileUrl("data/item.10.qml");
    QTest::newRow("011) item - initialised string prop") << testFileUrl("data/item.11.qml");
    QTest::newRow("012) item - initialised real prop") << testFileUrl("data/item.12.qml");
    QTest::newRow("013) item - initialised variant (value type) prop") << testFileUrl("data/item.13.qml");
    QTest::newRow("014) item - initialised Item prop (with child)") << testFileUrl("data/item.14.qml");
    QTest::newRow("015) item - initialised var prop with primitive") << testFileUrl("data/item.15.qml");
    QTest::newRow("016) item - initialised var prop with function") << testFileUrl("data/item.16.qml");
    QTest::newRow("017) item - initialised var prop with object") << testFileUrl("data/item.17.qml");
    QTest::newRow("018) item - int prop with optimized binding") << testFileUrl("data/item.18.qml");
    QTest::newRow("019) item - int prop with shared binding") << testFileUrl("data/item.19.qml");
    QTest::newRow("020) item - int prop with slow binding (alias and func)") << testFileUrl("data/item.20.qml");
    QTest::newRow("021) item - var prop with binding") << testFileUrl("data/item.21.qml");
    QTest::newRow("022) item - int prop with change signal") << testFileUrl("data/item.22.qml");
    QTest::newRow("023) item - importing module api") << testFileUrl("data/item.23.qml");
    QTest::newRow("024) item - accessing property of module api") << testFileUrl("data/item.24.qml");
    QTest::newRow("025) item - invoking function of module api") << testFileUrl("data/item.25.qml");

    QTest::newRow("026) expression - item with unused dynamic function") << testFileUrl("data/expression.1.qml");
    QTest::newRow("027) expression - item with dynamic function called in onCompleted") << testFileUrl("data/expression.2.qml");
    QTest::newRow("027) expression - item with bound signal no params") << testFileUrl("data/expression.3.qml");
    QTest::newRow("027) expression - item with bound signal with params") << testFileUrl("data/expression.4.qml");

    QTest::newRow("028) rectangle - empty") << testFileUrl("data/rectangle.1.qml");
    QTest::newRow("029) rectangle - with size") << testFileUrl("data/rectangle.2.qml");
    QTest::newRow("030) rectangle - with size and color") << testFileUrl("data/rectangle.3.qml");

    QTest::newRow("031) text - empty") << testFileUrl("data/text.1.qml");
    QTest::newRow("032) text - with size") << testFileUrl("data/text.2.qml");
    QTest::newRow("033) text - with size and text") << testFileUrl("data/text.3.qml");

    QTest::newRow("034) mouse area - empty") << testFileUrl("data/mousearea.1.qml");
    QTest::newRow("035) mouse area - with size") << testFileUrl("data/mousearea.2.qml");
    QTest::newRow("036) mouse area - with size and onClicked handler") << testFileUrl("data/mousearea.3.qml");

    QTest::newRow("037) listView - empty") << testFileUrl("data/listview.1.qml");
    QTest::newRow("038) listView - with anchors but no content") << testFileUrl("data/listview.2.qml");
    QTest::newRow("039) listView - with anchors and list model content with delegate") << testFileUrl("data/listview.3.qml");

    QTest::newRow("040) flickable - empty") << testFileUrl("data/flickable.1.qml");
    QTest::newRow("041) flickable - with size") << testFileUrl("data/flickable.2.qml");
    QTest::newRow("042) flickable - with some content") << testFileUrl("data/flickable.3.qml");

    QTest::newRow("043) image - empty") << testFileUrl("data/image.1.qml");
    QTest::newRow("044) image - with size") << testFileUrl("data/image.2.qml");
    QTest::newRow("045) image - with content") << testFileUrl("data/image.3.qml");
    QTest::newRow("046) image - with content, async") << testFileUrl("data/image.4.qml");

    QTest::newRow("047) states - two states, no transitions") << testFileUrl("data/states.1.qml");
    QTest::newRow("048) states - two states, with transition") << testFileUrl("data/states.2.qml");

    QTest::newRow("049) animations - number animation") << testFileUrl("data/animations.1.qml");
    QTest::newRow("050) animations - sequential animation (scriptaction, scriptaction)") << testFileUrl("data/animations.2.qml");
    QTest::newRow("051) animations - parallel animation (scriptaction, scriptaction)") << testFileUrl("data/animations.3.qml");
    QTest::newRow("052) animations - sequential animation, started in onCompleted") << testFileUrl("data/animations.4.qml");

    QTest::newRow("053) repeater - row - repeat simple rectangle") << testFileUrl("data/repeater.1.qml");
    QTest::newRow("054) repeater - column - repeat simple rectangle") << testFileUrl("data/repeater.2.qml");

    QTest::newRow("055) grid - simple grid of rectangles") << testFileUrl("data/grid.1.qml");

    QTest::newRow("056) positioning - no positioning") << testFileUrl("data/nopositioning.qml");
    QTest::newRow("057) positioning - absolute positioning") << testFileUrl("data/absolutepositioning.qml");
    QTest::newRow("058) positioning - anchored positioning") << testFileUrl("data/anchoredpositioning.qml");
    QTest::newRow("059) positioning - anchored (with grid) positioning") << testFileUrl("data/anchorwithgridpositioning.qml");
    QTest::newRow("060) positioning - binding positioning") << testFileUrl("data/bindingpositioning.qml");
    QTest::newRow("061) positioning - anchored (with binding) positioning") << testFileUrl("data/anchorwithbindingpositioning.qml");
    QTest::newRow("062) positioning - binding (with grid) positioning") << testFileUrl("data/bindingwithgridpositioning.qml");
}

void tst_librarymetrics_performance::compilation()
{
    QFETCH(QUrl, qmlfile);

    cleanState(&e);
    {
        // check that they can compile, before timing.
        QQmlComponent c(e, this);
        c.loadUrl(qmlfile); // just compile.
        if (!c.errors().isEmpty()) {
            qWarning() << "ERROR:" << c.errors();
        }
    }

    QList<qint64> nResults;

    // generate AVERAGE_OVER_N results
    for (int i = 0; i < AVERAGE_OVER_N; ++i) {
        cleanState(&e);
        {
            QElapsedTimer et;
            et.start();
            QQmlComponent c(e, this);
            c.loadUrl(qmlfile); // just compile.
            qint64 etime = et.nsecsElapsed();
            nResults.append(etime);
        }
    }

    // sort the list
    qSort(nResults);

    // remove IGNORE_N_OUTLIERS*2 from ONLY the worst end (remove gc interference)
    for (int i = 0; i < IGNORE_N_OUTLIERS; ++i) {
        if (!nResults.isEmpty()) nResults.removeLast();
        if (!nResults.isEmpty()) nResults.removeLast();
    }

    // now generate an average
    qint64 totaltime = 0;
    if (nResults.size() == 0) nResults.append(9999);
    for (int i = 0; i < nResults.size(); ++i)
        totaltime += nResults.at(i);
    double average = ((double)totaltime) / nResults.count();

    // and return it as the result
    QTest::setBenchmarkResult(average, QTest::WalltimeNanoseconds);
    QTest::setBenchmarkResult(average, QTest::WalltimeNanoseconds); // twice to workaround bug in QTestLib
}

void tst_librarymetrics_performance::instantiation_cached()
{
    QFETCH(QUrl, qmlfile);

    cleanState(&e);
    QList<qint64> nResults;

    // generate AVERAGE_OVER_N results
    for (int i = 0; i < AVERAGE_OVER_N; ++i) {
        QElapsedTimer et;
        et.start();
        QQmlComponent c(e, this);
        c.loadUrl(qmlfile); // just compile.
        QObject *o = c.create();
        qint64 etime = et.nsecsElapsed();
        nResults.append(etime);
        delete o;
    }

    // sort the list
    qSort(nResults);

    // remove IGNORE_N_OUTLIERS*2 from ONLY the worst end (remove gc interference)
    for (int i = 0; i < IGNORE_N_OUTLIERS; ++i) {
        if (!nResults.isEmpty()) nResults.removeLast();
        if (!nResults.isEmpty()) nResults.removeLast();
    }

    // now generate an average
    qint64 totaltime = 0;
    if (nResults.size() == 0) nResults.append(9999);
    for (int i = 0; i < nResults.size(); ++i)
        totaltime += nResults.at(i);
    double average = ((double)totaltime) / nResults.count();

    // and return it as the result
    QTest::setBenchmarkResult(average, QTest::WalltimeNanoseconds);
    QTest::setBenchmarkResult(average, QTest::WalltimeNanoseconds); // twice to workaround bug in QTestLib
}

void tst_librarymetrics_performance::instantiation()
{
    QFETCH(QUrl, qmlfile);

    cleanState(&e);
    QList<qint64> nResults;

    // generate AVERAGE_OVER_N results
    for (int i = 0; i < AVERAGE_OVER_N; ++i) {
        cleanState(&e);
        {
            QElapsedTimer et;
            et.start();
            QQmlComponent c(e, this);
            c.loadUrl(qmlfile); // just compile.
            QObject *o = c.create();
            qint64 etime = et.nsecsElapsed();
            nResults.append(etime);
            delete o;
        }
    }

    // sort the list
    qSort(nResults);

    // remove IGNORE_N_OUTLIERS*2 from ONLY the worst end (remove gc interference)
    for (int i = 0; i < IGNORE_N_OUTLIERS; ++i) {
        if (!nResults.isEmpty()) nResults.removeLast();
        if (!nResults.isEmpty()) nResults.removeLast();
    }

    // now generate an average
    qint64 totaltime = 0;
    if (nResults.size() == 0) nResults.append(9999);
    for (int i = 0; i < nResults.size(); ++i)
        totaltime += nResults.at(i);
    double average = ((double)totaltime) / nResults.count();

    // and return it as the result
    QTest::setBenchmarkResult(average, QTest::WalltimeNanoseconds);
    QTest::setBenchmarkResult(average, QTest::WalltimeNanoseconds); // twice to workaround bug in QTestLib
}

void tst_librarymetrics_performance::positioners_data()
{
    QTest::addColumn<QUrl>("qmlfile");
    QTest::newRow("01) positioning - no positioning") << testFileUrl("data/nopositioning.2.qml");
    QTest::newRow("02) positioning - absolute positioning") << testFileUrl("data/absolutepositioning.2.qml");
    QTest::newRow("03) positioning - anchored positioning") << testFileUrl("data/anchoredpositioning.2.qml");
    QTest::newRow("04) positioning - anchored (with grid) positioning") << testFileUrl("data/anchorwithgridpositioning.2.qml");
    QTest::newRow("05) positioning - binding positioning") << testFileUrl("data/bindingpositioning.2.qml");
    QTest::newRow("06) positioning - anchored (with binding) positioning") << testFileUrl("data/anchorwithbindingpositioning.2.qml");
    QTest::newRow("07) positioning - binding (with grid) positioning") << testFileUrl("data/bindingwithgridpositioning.2.qml");
}

// this test triggers repositioning a large number of times,
// so we can track the cost of different repositioning methods.
void tst_librarymetrics_performance::positioners()
{
    QFETCH(QUrl, qmlfile);

    cleanState(&e);
    QList<qint64> nResults;

    // generate AVERAGE_OVER_N results
    for (int i = 0; i < AVERAGE_OVER_N; ++i) {
        cleanState(&e);
        {
            QElapsedTimer et;
            et.start();
            QQmlComponent c(e, this);
            c.loadUrl(qmlfile); // just compile.
            QObject *o = c.create();
            qint64 etime = et.nsecsElapsed();
            nResults.append(etime);
            delete o;
        }
    }

    // sort the list
    qSort(nResults);

    // remove IGNORE_N_OUTLIERS*2 from ONLY the worst end (remove gc interference)
    for (int i = 0; i < IGNORE_N_OUTLIERS; ++i) {
        if (!nResults.isEmpty()) nResults.removeLast();
        if (!nResults.isEmpty()) nResults.removeLast();
    }

    // now generate an average
    qint64 totaltime = 0;
    if (nResults.size() == 0) nResults.append(9999);
    for (int i = 0; i < nResults.size(); ++i)
        totaltime += nResults.at(i);
    double average = ((double)totaltime) / nResults.count();

    // and return it as the result
    QTest::setBenchmarkResult(average, QTest::WalltimeNanoseconds);
    QTest::setBenchmarkResult(average, QTest::WalltimeNanoseconds); // twice to workaround bug in QTestLib
}

QTEST_MAIN(tst_librarymetrics_performance)

#include "tst_librarymetrics_performance.moc"
