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
#include "testtypes.h"

#include <QUrl>
#include <QDir>
#include <QDebug>
#include <qtest.h>
#include <QPointer>
#include <QFileInfo>
#include <QQmlEngine>
#include <QQmlContext>
#include <QQmlProperty>
#include <QQmlComponent>
#include <QQmlIncubator>
#include "../../shared/util.h"
#include <private/qqmlincubator_p.h>
#include <private/qqmlobjectcreator_p.h>

class tst_qqmlincubator : public QQmlDataTest
{
    Q_OBJECT
public:
    tst_qqmlincubator() {}

private slots:
    void initTestCase();

    void incubationMode();
    void objectDeleted();
    void clear();
    void noIncubationController();
    void forceCompletion();
    void setInitialState();
    void clearDuringCompletion();
    void objectDeletionAfterInit();
    void recursiveClear();
    void statusChanged();
    void asynchronousIfNested();
    void nestedComponent();
    void chainedAsynchronousIfNested();
    void chainedAsynchronousIfNestedOnCompleted();
    void chainedAsynchronousClear();
    void selfDelete();
    void contextDelete();

private:
    QQmlIncubationController controller;
    QQmlEngine engine;
};

#define VERIFY_ERRORS(component, errorfile) \
    if (!errorfile) { \
        if (qgetenv("DEBUG") != "" && !component.errors().isEmpty()) \
            qWarning() << "Unexpected Errors:" << component.errors(); \
        QVERIFY(!component.isError()); \
        QVERIFY(component.errors().isEmpty()); \
    } else { \
        QFile file(QQmlDataTest::instance()->testFile(errorfile)); \
        QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text)); \
        QByteArray data = file.readAll(); \
        file.close(); \
        QList<QByteArray> expected = data.split('\n'); \
        expected.removeAll(QByteArray("")); \
        QList<QQmlError> errors = component.errors(); \
        QList<QByteArray> actual; \
        for (int ii = 0; ii < errors.count(); ++ii) { \
            const QQmlError &error = errors.at(ii); \
            QByteArray errorStr = QByteArray::number(error.line()) + ':' +  \
                                  QByteArray::number(error.column()) + ':' + \
                                  error.description().toUtf8(); \
            actual << errorStr; \
        } \
        if (qgetenv("DEBUG") != "" && expected != actual) \
            qWarning() << "Expected:" << expected << "Actual:" << actual;  \
        QCOMPARE(expected, actual); \
    }

void tst_qqmlincubator::initTestCase()
{
    QQmlDataTest::initTestCase();
    registerTypes();
    engine.setIncubationController(&controller);
}

void tst_qqmlincubator::incubationMode()
{
    {
    QQmlIncubator incubator;
    QCOMPARE(incubator.incubationMode(), QQmlIncubator::Asynchronous);
    }
    {
    QQmlIncubator incubator(QQmlIncubator::Asynchronous);
    QCOMPARE(incubator.incubationMode(), QQmlIncubator::Asynchronous);
    }
    {
    QQmlIncubator incubator(QQmlIncubator::Synchronous);
    QCOMPARE(incubator.incubationMode(), QQmlIncubator::Synchronous);
    }
    {
    QQmlIncubator incubator(QQmlIncubator::AsynchronousIfNested);
    QCOMPARE(incubator.incubationMode(), QQmlIncubator::AsynchronousIfNested);
    }
}

void tst_qqmlincubator::objectDeleted()
{
    {
        QQmlEngine engine;
        QQmlIncubationController controller;
        engine.setIncubationController(&controller);
        SelfRegisteringType::clearMe();

        QQmlComponent component(&engine, testFileUrl("objectDeleted.qml"));
        QVERIFY(component.isReady());

        QQmlIncubator incubator;
        component.create(incubator);

        QCOMPARE(incubator.status(), QQmlIncubator::Loading);
        QVERIFY(!SelfRegisteringType::me());

        while (SelfRegisteringOuterType::me() == 0 && incubator.isLoading()) {
            bool b = false;
            controller.incubateWhile(&b);
        }

        QVERIFY(SelfRegisteringOuterType::me() != 0);
        QVERIFY(incubator.isLoading());

        while (SelfRegisteringType::me() == 0 && incubator.isLoading()) {
            bool b = false;
            controller.incubateWhile(&b);
        }

        delete SelfRegisteringType::me();

        {
            bool b = true;
            controller.incubateWhile(&b);
        }

        QVERIFY(incubator.isError());
        VERIFY_ERRORS(incubator, "objectDeleted.errors.txt");
        QVERIFY(!incubator.object());
    }
    QVERIFY(SelfRegisteringOuterType::beenDeleted);
}

void tst_qqmlincubator::clear()
{
    SelfRegisteringType::clearMe();

    QQmlComponent component(&engine, testFileUrl("clear.qml"));
    QVERIFY(component.isReady());

    // Clear in null state
    {
    QQmlIncubator incubator;
    QVERIFY(incubator.isNull());
    incubator.clear(); // no effect
    QVERIFY(incubator.isNull());
    }

    // Clear in loading state
    {
    QQmlIncubator incubator;
    component.create(incubator);
    QVERIFY(incubator.isLoading());
    incubator.clear();
    QVERIFY(incubator.isNull());
    }

    // Clear mid load
    {
    QQmlIncubator incubator;
    component.create(incubator);

    while (SelfRegisteringType::me() == 0 && incubator.isLoading()) {
        bool b = false;
        controller.incubateWhile(&b);
    }

    QVERIFY(incubator.isLoading());
    QVERIFY(SelfRegisteringType::me() != 0);
    QPointer<SelfRegisteringType> srt = SelfRegisteringType::me();

    incubator.clear();
    QVERIFY(incubator.isNull());
    QVERIFY(srt.isNull());
    }

    // Clear in ready state
    {
    QQmlIncubator incubator;
    component.create(incubator);

    {
        bool b = true;
        controller.incubateWhile(&b);
    }

    QVERIFY(incubator.isReady());
    QVERIFY(incubator.object() != 0);
    QPointer<QObject> obj = incubator.object();

    incubator.clear();
    QVERIFY(incubator.isNull());
    QVERIFY(!incubator.object());
    QVERIFY(!obj.isNull());

    delete obj;
    QVERIFY(obj.isNull());
    }
}

void tst_qqmlincubator::noIncubationController()
{
    // All incubators should behave synchronously when there is no controller

    QQmlEngine engine;
    QQmlComponent component(&engine, testFileUrl("noIncubationController.qml"));

    QVERIFY(component.isReady());

    {
    QQmlIncubator incubator(QQmlIncubator::Asynchronous);
    component.create(incubator);
    QVERIFY(incubator.isReady());
    QVERIFY(incubator.object());
    QCOMPARE(incubator.object()->property("testValue").toInt(), 1913);
    delete incubator.object();
    }

    {
    QQmlIncubator incubator(QQmlIncubator::AsynchronousIfNested);
    component.create(incubator);
    QVERIFY(incubator.isReady());
    QVERIFY(incubator.object());
    QCOMPARE(incubator.object()->property("testValue").toInt(), 1913);
    delete incubator.object();
    }

    {
    QQmlIncubator incubator(QQmlIncubator::Synchronous);
    component.create(incubator);
    QVERIFY(incubator.isReady());
    QVERIFY(incubator.object());
    QCOMPARE(incubator.object()->property("testValue").toInt(), 1913);
    delete incubator.object();
    }
}

void tst_qqmlincubator::forceCompletion()
{
    QQmlComponent component(&engine, testFileUrl("forceCompletion.qml"));
    QVERIFY(component.isReady());

    {
    // forceCompletion on a null incubator does nothing
    QQmlIncubator incubator;
    QVERIFY(incubator.isNull());
    incubator.forceCompletion();
    QVERIFY(incubator.isNull());
    }

    {
    // forceCompletion immediately after creating an asynchronous object completes it
    QQmlIncubator incubator;
    QVERIFY(incubator.isNull());
    component.create(incubator);
    QVERIFY(incubator.isLoading());

    incubator.forceCompletion();

    QVERIFY(incubator.isReady());
    QVERIFY(incubator.object() != 0);
    QCOMPARE(incubator.object()->property("testValue").toInt(), 3499);

    delete incubator.object();
    }

    {
    // forceCompletion during creation completes it
    SelfRegisteringType::clearMe();

    QQmlIncubator incubator;
    QVERIFY(incubator.isNull());
    component.create(incubator);
    QVERIFY(incubator.isLoading());

    while (SelfRegisteringType::me() == 0 && incubator.isLoading()) {
        bool b = false;
        controller.incubateWhile(&b);
    }

    QVERIFY(SelfRegisteringType::me() != 0);
    QVERIFY(incubator.isLoading());

    incubator.forceCompletion();

    QVERIFY(incubator.isReady());
    QVERIFY(incubator.object() != 0);
    QCOMPARE(incubator.object()->property("testValue").toInt(), 3499);

    delete incubator.object();
    }

    {
    // forceCompletion on a ready incubator has no effect
    QQmlIncubator incubator;
    QVERIFY(incubator.isNull());
    component.create(incubator);
    QVERIFY(incubator.isLoading());

    incubator.forceCompletion();

    QVERIFY(incubator.isReady());
    QVERIFY(incubator.object() != 0);
    QCOMPARE(incubator.object()->property("testValue").toInt(), 3499);

    incubator.forceCompletion();

    QVERIFY(incubator.isReady());
    QVERIFY(incubator.object() != 0);
    QCOMPARE(incubator.object()->property("testValue").toInt(), 3499);

    delete incubator.object();
    }
}

void tst_qqmlincubator::setInitialState()
{
    QQmlComponent component(&engine, testFileUrl("setInitialState.qml"));
    QVERIFY(component.isReady());

    struct MyIncubator : public QQmlIncubator
    {
        MyIncubator(QQmlIncubator::IncubationMode mode)
        : QQmlIncubator(mode) {}

        virtual void setInitialState(QObject *o) {
            QQmlProperty::write(o, "test2", 19);
            QQmlProperty::write(o, "testData1", 201);
        }
    };

    {
    MyIncubator incubator(QQmlIncubator::Asynchronous);
    component.create(incubator);
    QVERIFY(incubator.isLoading());
    bool b = true;
    controller.incubateWhile(&b);
    QVERIFY(incubator.isReady());
    QVERIFY(incubator.object());
    QCOMPARE(incubator.object()->property("myValueFunctionCalled").toBool(), false);
    QCOMPARE(incubator.object()->property("test1").toInt(), 502);
    QCOMPARE(incubator.object()->property("test2").toInt(), 19);
    delete incubator.object();
    }

    {
    MyIncubator incubator(QQmlIncubator::Synchronous);
    component.create(incubator);
    QVERIFY(incubator.isReady());
    QVERIFY(incubator.object());
    QCOMPARE(incubator.object()->property("myValueFunctionCalled").toBool(), false);
    QCOMPARE(incubator.object()->property("test1").toInt(), 502);
    QCOMPARE(incubator.object()->property("test2").toInt(), 19);
    delete incubator.object();
    }
}

void tst_qqmlincubator::clearDuringCompletion()
{
    CompletionRegisteringType::clearMe();
    SelfRegisteringType::clearMe();

    QQmlComponent component(&engine, testFileUrl("clearDuringCompletion.qml"));
    QVERIFY(component.isReady());

    QQmlIncubator incubator;
    component.create(incubator);

    QCOMPARE(incubator.status(), QQmlIncubator::Loading);
    QVERIFY(!CompletionRegisteringType::me());

    while (CompletionRegisteringType::me() == 0 && incubator.isLoading()) {
        bool b = false;
        controller.incubateWhile(&b);
    }

    QVERIFY(CompletionRegisteringType::me() != 0);
    QVERIFY(SelfRegisteringType::me() != 0);
    QVERIFY(incubator.isLoading());

    QPointer<QObject> srt = SelfRegisteringType::me();

    incubator.clear();
    QCoreApplication::sendPostedEvents(0, QEvent::DeferredDelete);
    QCoreApplication::processEvents();
    QVERIFY(incubator.isNull());
    QVERIFY(srt.isNull());
}

void tst_qqmlincubator::objectDeletionAfterInit()
{
    QQmlComponent component(&engine, testFileUrl("clear.qml"));
    QVERIFY(component.isReady());

    struct MyIncubator : public QQmlIncubator
    {
        MyIncubator(QQmlIncubator::IncubationMode mode)
        : QQmlIncubator(mode), obj(0) {}

        virtual void setInitialState(QObject *o) {
            obj = o;
        }

        QObject *obj;
    };

    SelfRegisteringType::clearMe();
    MyIncubator incubator(QQmlIncubator::Asynchronous);
    component.create(incubator);

    while (!incubator.obj && incubator.isLoading()) {
        bool b = false;
        controller.incubateWhile(&b);
    }

    QVERIFY(incubator.isLoading());
    QVERIFY(SelfRegisteringType::me() != 0);

    delete incubator.obj;

    incubator.clear();
    QCoreApplication::sendPostedEvents(0, QEvent::DeferredDelete);
    QCoreApplication::processEvents();
    QVERIFY(incubator.isNull());
}

class Switcher : public QObject
{
    Q_OBJECT
public:
    Switcher(QQmlEngine *e) : QObject(), engine(e) { }

    struct MyIncubator : public QQmlIncubator
    {
        MyIncubator(QQmlIncubator::IncubationMode mode, QObject *s)
        : QQmlIncubator(mode), switcher(s) {}

        virtual void setInitialState(QObject *o) {
            if (o->objectName() == "switchMe")
                connect(o, SIGNAL(switchMe()), switcher, SLOT(switchIt()));
        }

        QObject *switcher;
    };

    void start()
    {
        incubator = new MyIncubator(QQmlIncubator::Synchronous, this);
        component = new QQmlComponent(engine, QQmlDataTest::instance()->testFileUrl("recursiveClear.1.qml"));
        component->create(*incubator);
    }

    QQmlEngine *engine;
    MyIncubator *incubator;
    QQmlComponent *component;

public slots:
    void switchIt() {
        component->deleteLater();
        incubator->clear();
        component = new QQmlComponent(engine, QQmlDataTest::instance()->testFileUrl("recursiveClear.2.qml"));
        component->create(*incubator);
    }
};

void tst_qqmlincubator::recursiveClear()
{
    Switcher switcher(&engine);
    switcher.start();
}

void tst_qqmlincubator::statusChanged()
{
    class MyIncubator : public QQmlIncubator
    {
    public:
        MyIncubator(QQmlIncubator::IncubationMode mode = QQmlIncubator::Asynchronous)
        : QQmlIncubator(mode) {}

        QList<int> statuses;
    protected:
        virtual void statusChanged(Status s) { statuses << s; }
        virtual void setInitialState(QObject *) { statuses << -1; }
    };

    {
    QQmlComponent component(&engine, testFileUrl("statusChanged.qml"));
    QVERIFY(component.isReady());

    MyIncubator incubator(QQmlIncubator::Synchronous);
    component.create(incubator);
    QVERIFY(incubator.isReady());
    QCOMPARE(incubator.statuses.count(), 3);
    QCOMPARE(incubator.statuses.at(0), int(QQmlIncubator::Loading));
    QCOMPARE(incubator.statuses.at(1), -1);
    QCOMPARE(incubator.statuses.at(2), int(QQmlIncubator::Ready));
    delete incubator.object();
    }

    {
    QQmlComponent component(&engine, testFileUrl("statusChanged.qml"));
    QVERIFY(component.isReady());

    MyIncubator incubator(QQmlIncubator::Asynchronous);
    component.create(incubator);
    QVERIFY(incubator.isLoading());
    QCOMPARE(incubator.statuses.count(), 1);
    QCOMPARE(incubator.statuses.at(0), int(QQmlIncubator::Loading));

    {
    bool b = true;
    controller.incubateWhile(&b);
    }

    QCOMPARE(incubator.statuses.count(), 3);
    QCOMPARE(incubator.statuses.at(0), int(QQmlIncubator::Loading));
    QCOMPARE(incubator.statuses.at(1), -1);
    QCOMPARE(incubator.statuses.at(2), int(QQmlIncubator::Ready));
    delete incubator.object();
    }

    {
    QQmlComponent component2(&engine, testFileUrl("statusChanged.nested.qml"));
    QVERIFY(component2.isReady());

    MyIncubator incubator(QQmlIncubator::Asynchronous);
    component2.create(incubator);
    QVERIFY(incubator.isLoading());
    QCOMPARE(incubator.statuses.count(), 1);
    QCOMPARE(incubator.statuses.at(0), int(QQmlIncubator::Loading));

    {
    bool b = true;
    controller.incubateWhile(&b);
    }

    QVERIFY(incubator.isReady());
    QCOMPARE(incubator.statuses.count(), 3);
    QCOMPARE(incubator.statuses.at(0), int(QQmlIncubator::Loading));
    QCOMPARE(incubator.statuses.at(1), -1);
    QCOMPARE(incubator.statuses.at(2), int(QQmlIncubator::Ready));
    delete incubator.object();
    }
}

void tst_qqmlincubator::asynchronousIfNested()
{
    // Asynchronous if nested within a finalized context behaves synchronously
    {
    QQmlComponent component(&engine, testFileUrl("asynchronousIfNested.1.qml"));
    QVERIFY(component.isReady());

    QObject *object = component.create();
    QVERIFY(object != 0);
    QCOMPARE(object->property("a").toInt(), 10);

    QQmlIncubator incubator(QQmlIncubator::AsynchronousIfNested);
    component.create(incubator, 0, qmlContext(object));

    QVERIFY(incubator.isReady());
    QVERIFY(incubator.object());
    QCOMPARE(incubator.object()->property("a").toInt(), 10);
    delete incubator.object();
    delete object;
    }

    // Asynchronous if nested within an executing context behaves asynchronously, but prevents
    // the parent from finishing
    {
    SelfRegisteringType::clearMe();

    QQmlComponent component(&engine, testFileUrl("asynchronousIfNested.2.qml"));
    QVERIFY(component.isReady());

    QQmlIncubator incubator;
    component.create(incubator);

    QVERIFY(incubator.isLoading());
    QVERIFY(!SelfRegisteringType::me());
    while (SelfRegisteringType::me() == 0 && incubator.isLoading()) {
        bool b = false;
        controller.incubateWhile(&b);
    }

    QVERIFY(SelfRegisteringType::me() != 0);
    QVERIFY(incubator.isLoading());

    QQmlIncubator nested(QQmlIncubator::AsynchronousIfNested);
    component.create(nested, 0, qmlContext(SelfRegisteringType::me()));
    QVERIFY(nested.isLoading());

    while (nested.isLoading()) {
        QVERIFY(incubator.isLoading());
        bool b = false;
        controller.incubateWhile(&b);
    }

    QVERIFY(nested.isReady());
    QVERIFY(incubator.isLoading());

    {
        bool b = true;
        controller.incubateWhile(&b);
    }

    QVERIFY(nested.isReady());
    QVERIFY(incubator.isReady());

    delete nested.object();
    delete incubator.object();
    }

    // AsynchronousIfNested within a synchronous AsynchronousIfNested behaves synchronously
    {
    SelfRegisteringType::clearMe();

    QQmlComponent component(&engine, testFileUrl("asynchronousIfNested.3.qml"));
    QVERIFY(component.isReady());

    struct CallbackData {
        CallbackData(QQmlEngine *e) : engine(e), pass(false) {}
        QQmlEngine *engine;
        bool pass;
        static void callback(CallbackRegisteringType *o, void *data) {
            CallbackData *d = (CallbackData *)data;

            QQmlComponent c(d->engine, QQmlDataTest::instance()->testFileUrl("asynchronousIfNested.1.qml"));
            if (!c.isReady()) return;

            QQmlIncubator incubator(QQmlIncubator::AsynchronousIfNested);
            c.create(incubator, 0, qmlContext(o));

            if (!incubator.isReady()) return;

            if (incubator.object()->property("a").toInt() != 10) return;

            d->pass = true;
        }
    };

    CallbackData cd(&engine);
    CallbackRegisteringType::registerCallback(&CallbackData::callback, &cd);

    QQmlIncubator incubator(QQmlIncubator::AsynchronousIfNested);
    component.create(incubator);

    QVERIFY(incubator.isReady());
    QCOMPARE(cd.pass, true);

    delete incubator.object();
    }
}

void tst_qqmlincubator::nestedComponent()
{
    QQmlComponent component(&engine, testFileUrl("nestedComponent.qml"));
    QVERIFY(component.isReady());

    QObject *object = component.create();

    QQmlComponent *nested = object->property("c").value<QQmlComponent*>();
    QVERIFY(nested);
    QVERIFY(nested->isReady());

    // Test without incubator
    {
    QObject *nestedObject = nested->create();
    QCOMPARE(nestedObject->property("value").toInt(), 19988);
    delete nestedObject;
    }

    // Test with incubator
    {
    QQmlIncubator incubator(QQmlIncubator::Synchronous);
    nested->create(incubator);
    QVERIFY(incubator.isReady());
    QVERIFY(incubator.object());
    QCOMPARE(incubator.object()->property("value").toInt(), 19988);
    delete incubator.object();
    }

    delete object;
}

// Checks that a new AsynchronousIfNested incubator can be correctly started in the
// statusChanged() callback of another.
void tst_qqmlincubator::chainedAsynchronousIfNested()
{
    SelfRegisteringType::clearMe();

    QQmlComponent component(&engine, testFileUrl("chainedAsynchronousIfNested.qml"));
    QVERIFY(component.isReady());

    QQmlIncubator incubator(QQmlIncubator::Asynchronous);
    component.create(incubator);

    QVERIFY(incubator.isLoading());
    QVERIFY(!SelfRegisteringType::me());

    while (SelfRegisteringType::me() == 0 && incubator.isLoading()) {
        bool b = false;
        controller.incubateWhile(&b);
    }

    QVERIFY(SelfRegisteringType::me() != 0);
    QVERIFY(incubator.isLoading());

    struct MyIncubator : public QQmlIncubator {
        MyIncubator(MyIncubator *next, QQmlComponent *component, QQmlContext *ctxt)
        : QQmlIncubator(AsynchronousIfNested), next(next), component(component), ctxt(ctxt) {}

    protected:
        virtual void statusChanged(Status s) {
            if (s == Ready && next)
                component->create(*next, 0, ctxt);
        }

    private:
        MyIncubator *next;
        QQmlComponent *component;
        QQmlContext *ctxt;
    };

    MyIncubator incubator2(0, &component, 0);
    MyIncubator incubator1(&incubator2, &component, qmlContext(SelfRegisteringType::me()));

    component.create(incubator1, 0, qmlContext(SelfRegisteringType::me()));

    QVERIFY(incubator.isLoading());
    QVERIFY(incubator1.isLoading());
    QVERIFY(incubator2.isNull());

    while (incubator1.isLoading()) {
        QVERIFY(incubator.isLoading());
        QVERIFY(incubator1.isLoading());
        QVERIFY(incubator2.isNull());

        bool b = false;
        controller.incubateWhile(&b);
    }

    QVERIFY(incubator.isLoading());
    QVERIFY(incubator1.isReady());
    QVERIFY(incubator2.isLoading());

    while (incubator2.isLoading()) {
        QVERIFY(incubator.isLoading());
        QVERIFY(incubator1.isReady());
        QVERIFY(incubator2.isLoading());

        bool b = false;
        controller.incubateWhile(&b);
    }

    QVERIFY(incubator1.isReady());
    QVERIFY(incubator2.isReady());
    if (incubator.isLoading()) {
        bool b = true;
        controller.incubateWhile(&b);
    }

    QVERIFY(incubator.isReady());
    QVERIFY(incubator1.isReady());
    QVERIFY(incubator2.isReady());
}

// Checks that new AsynchronousIfNested incubators can be correctly chained if started in
// componentCompleted().
void tst_qqmlincubator::chainedAsynchronousIfNestedOnCompleted()
{
    SelfRegisteringType::clearMe();

    QQmlComponent component(&engine, testFileUrl("chainInCompletion.qml"));
    QVERIFY(component.isReady());

    QQmlComponent c1(&engine, testFileUrl("chainedAsynchronousIfNested.qml"));
    QVERIFY(c1.isReady());

    struct MyIncubator : public QQmlIncubator {
        MyIncubator(MyIncubator *next, QQmlComponent *component, QQmlContext *ctxt)
        : QQmlIncubator(AsynchronousIfNested), next(next), component(component), ctxt(ctxt) {}

    protected:
        virtual void statusChanged(Status s) {
            if (s == Ready && next) {
                component->create(*next, 0, ctxt);
            }
        }

    private:
        MyIncubator *next;
        QQmlComponent *component;
        QQmlContext *ctxt;
    };

    struct CallbackData {
        CallbackData(QQmlComponent *c, MyIncubator *i, QQmlContext *ct)
            : component(c), incubator(i), ctxt(ct) {}
        QQmlComponent *component;
        MyIncubator *incubator;
        QQmlContext *ctxt;
        static void callback(CompletionCallbackType *, void *data) {
            CallbackData *d = (CallbackData *)data;
            d->component->create(*d->incubator, 0, d->ctxt);
        }
    };

    QQmlIncubator incubator(QQmlIncubator::Asynchronous);
    component.create(incubator);

    QVERIFY(incubator.isLoading());
    QVERIFY(!SelfRegisteringType::me());

    while (SelfRegisteringType::me() == 0 && incubator.isLoading()) {
        bool b = false;
        controller.incubateWhile(&b);
    }

    QVERIFY(SelfRegisteringType::me() != 0);
    QVERIFY(incubator.isLoading());

    MyIncubator incubator3(0, &c1, qmlContext(SelfRegisteringType::me()));
    MyIncubator incubator2(&incubator3, &c1, qmlContext(SelfRegisteringType::me()));
    MyIncubator incubator1(&incubator2, &c1, qmlContext(SelfRegisteringType::me()));

    // start incubator1 in componentComplete
    CallbackData cd(&c1, &incubator1, qmlContext(SelfRegisteringType::me()));
    CompletionCallbackType::registerCallback(&CallbackData::callback, &cd);

    while (!incubator1.isLoading()) {
        QVERIFY(incubator.isLoading());
        QVERIFY(incubator2.isNull());
        QVERIFY(incubator3.isNull());

        bool b = false;
        controller.incubateWhile(&b);
    }

    QVERIFY(incubator.isLoading());
    QVERIFY(incubator1.isLoading());
    QVERIFY(incubator2.isNull());
    QVERIFY(incubator3.isNull());

    while (incubator1.isLoading()) {
        QVERIFY(incubator.isLoading());
        QVERIFY(incubator1.isLoading());
        QVERIFY(incubator2.isNull());
        QVERIFY(incubator3.isNull());

        bool b = false;
        controller.incubateWhile(&b);
    }

    QVERIFY(incubator.isLoading());
    QVERIFY(incubator1.isReady());
    QVERIFY(incubator2.isLoading());
    QVERIFY(incubator3.isNull());

    while (incubator2.isLoading()) {
        QVERIFY(incubator.isLoading());
        QVERIFY(incubator1.isReady());
        QVERIFY(incubator2.isLoading());
        QVERIFY(incubator3.isNull());

        bool b = false;
        controller.incubateWhile(&b);
    }

    QVERIFY(incubator.isLoading());
    QVERIFY(incubator1.isReady());
    QVERIFY(incubator2.isReady());
    QVERIFY(incubator3.isLoading());

    while (incubator3.isLoading()) {
        QVERIFY(incubator.isLoading());
        QVERIFY(incubator1.isReady());
        QVERIFY(incubator2.isReady());
        QVERIFY(incubator3.isLoading());

        bool b = false;
        controller.incubateWhile(&b);
    }

    {
    bool b = true;
    controller.incubateWhile(&b);
    }

    QVERIFY(incubator.isReady());
    QVERIFY(incubator1.isReady());
    QVERIFY(incubator2.isReady());
    QVERIFY(incubator3.isReady());
}

// Checks that new AsynchronousIfNested incubators can be correctly cleared if started in
// componentCompleted().
void tst_qqmlincubator::chainedAsynchronousClear()
{
    SelfRegisteringType::clearMe();

    QQmlComponent component(&engine, testFileUrl("chainInCompletion.qml"));
    QVERIFY(component.isReady());

    QQmlComponent c1(&engine, testFileUrl("chainedAsynchronousIfNested.qml"));
    QVERIFY(c1.isReady());

    struct MyIncubator : public QQmlIncubator {
        MyIncubator(MyIncubator *next, QQmlComponent *component, QQmlContext *ctxt)
        : QQmlIncubator(AsynchronousIfNested), next(next), component(component), ctxt(ctxt) {}

    protected:
        virtual void statusChanged(Status s) {
            if (s == Ready && next) {
                component->create(*next, 0, ctxt);
            }
        }

    private:
        MyIncubator *next;
        QQmlComponent *component;
        QQmlContext *ctxt;
    };

    struct CallbackData {
        CallbackData(QQmlComponent *c, MyIncubator *i, QQmlContext *ct)
            : component(c), incubator(i), ctxt(ct) {}
        QQmlComponent *component;
        MyIncubator *incubator;
        QQmlContext *ctxt;
        static void callback(CompletionCallbackType *, void *data) {
            CallbackData *d = (CallbackData *)data;
            d->component->create(*d->incubator, 0, d->ctxt);
        }
    };

    QQmlIncubator incubator(QQmlIncubator::Asynchronous);
    component.create(incubator);

    QVERIFY(incubator.isLoading());
    QVERIFY(!SelfRegisteringType::me());

    while (SelfRegisteringType::me() == 0 && incubator.isLoading()) {
        bool b = false;
        controller.incubateWhile(&b);
    }

    QVERIFY(SelfRegisteringType::me() != 0);
    QVERIFY(incubator.isLoading());

    MyIncubator incubator3(0, &c1, qmlContext(SelfRegisteringType::me()));
    MyIncubator incubator2(&incubator3, &c1, qmlContext(SelfRegisteringType::me()));
    MyIncubator incubator1(&incubator2, &c1, qmlContext(SelfRegisteringType::me()));

    // start incubator1 in componentComplete
    CallbackData cd(&c1, &incubator1, qmlContext(SelfRegisteringType::me()));
    CompletionCallbackType::registerCallback(&CallbackData::callback, &cd);

    while (!incubator1.isLoading()) {
        QVERIFY(incubator.isLoading());
        QVERIFY(incubator2.isNull());
        QVERIFY(incubator3.isNull());

        bool b = false;
        controller.incubateWhile(&b);
    }

    QVERIFY(incubator.isLoading());
    QVERIFY(incubator1.isLoading());
    QVERIFY(incubator2.isNull());
    QVERIFY(incubator3.isNull());

    while (incubator1.isLoading()) {
        QVERIFY(incubator.isLoading());
        QVERIFY(incubator1.isLoading());
        QVERIFY(incubator2.isNull());
        QVERIFY(incubator3.isNull());

        bool b = false;
        controller.incubateWhile(&b);
    }

    QVERIFY(incubator.isLoading());
    QVERIFY(incubator1.isReady());
    QVERIFY(incubator2.isLoading());
    QVERIFY(incubator3.isNull());

    while (incubator2.isLoading()) {
        QVERIFY(incubator.isLoading());
        QVERIFY(incubator1.isReady());
        QVERIFY(incubator2.isLoading());
        QVERIFY(incubator3.isNull());

        bool b = false;
        controller.incubateWhile(&b);
    }

    QVERIFY(incubator.isLoading());
    QVERIFY(incubator1.isReady());
    QVERIFY(incubator2.isReady());
    QVERIFY(incubator3.isLoading());

    // Any in loading state will become null when cleared.
    incubator.clear();

    QVERIFY(incubator.isNull());
    QVERIFY(incubator1.isReady());
    QVERIFY(incubator2.isReady());
    QVERIFY(incubator3.isNull());
}

void tst_qqmlincubator::selfDelete()
{
    struct MyIncubator : public QQmlIncubator {
        MyIncubator(bool *done, Status status, IncubationMode mode)
        : QQmlIncubator(mode), done(done), status(status) {}

    protected:
        virtual void statusChanged(Status s) {
            if (s == status) {
                *done = true;
                if (s == Ready) delete object();
                delete this;
            }
        }

    private:
        bool *done;
        Status status;
    };

    {
    QQmlComponent component(&engine, testFileUrl("selfDelete.qml"));

#define DELETE_TEST(status, mode) { \
    bool done = false; \
    component.create(*(new MyIncubator(&done, status, mode))); \
    bool True = true; \
    controller.incubateWhile(&True); \
    QVERIFY(done == true); \
    }

    DELETE_TEST(QQmlIncubator::Loading, QQmlIncubator::Synchronous);
    DELETE_TEST(QQmlIncubator::Ready, QQmlIncubator::Synchronous);
    DELETE_TEST(QQmlIncubator::Loading, QQmlIncubator::Asynchronous);
    DELETE_TEST(QQmlIncubator::Ready, QQmlIncubator::Asynchronous);

#undef DELETE_TEST
    }

    // Delete within error status
    {
    SelfRegisteringType::clearMe();

    QQmlComponent component(&engine, testFileUrl("objectDeleted.qml"));
    QVERIFY(component.isReady());

    bool done = false;
    MyIncubator *incubator = new MyIncubator(&done, QQmlIncubator::Error,
                                             QQmlIncubator::Asynchronous);
    component.create(*incubator);

    QCOMPARE(incubator->QQmlIncubator::status(), QQmlIncubator::Loading);
    QVERIFY(!SelfRegisteringType::me());

    while (SelfRegisteringType::me() == 0 && incubator->isLoading()) {
        bool b = false;
        controller.incubateWhile(&b);
    }

    QVERIFY(SelfRegisteringType::me() != 0);
    QVERIFY(incubator->isLoading());

    // We have to cheat and manually remove it from the creator->allCreatedObjects
    // otherwise we will do a double delete
    QQmlIncubatorPrivate *incubatorPriv = QQmlIncubatorPrivate::get(incubator);
    incubatorPriv->creator->allCreatedObjects().pop();
    delete SelfRegisteringType::me();

    {
    bool b = true;
    controller.incubateWhile(&b);
    }

    QVERIFY(done);
    }
}

// Test that QML doesn't crash if the context is deleted prior to the incubator
// first executing.
void tst_qqmlincubator::contextDelete()
{
    QQmlContext *context = new QQmlContext(engine.rootContext());
    QQmlComponent component(&engine, testFileUrl("contextDelete.qml"));

    QQmlIncubator incubator;
    component.create(incubator, context);

    delete context;

    {
        bool b = false;
        controller.incubateWhile(&b);
    }
}

QTEST_MAIN(tst_qqmlincubator)

#include "tst_qqmlincubator.moc"
