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

#include <qtest.h>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlProperty>
#include <QFile>
#include <QDebug>

// Conceptually, there are several different "holistic" areas to benchmark:
// 1) Loading
//     - read file from disk
//     - parse/lex file
//     - handle nested imports
// 2) Compilation
//     - create meta object templates etc
//     - compile to bytecode and cache it
// 3) Instantiation
//     - running the bytecode to create an object tree, assign properties, etc
//     - and, importantly, to evaluate bindings for the first time (incl. js expressions)
// 4) Dynamicism
//     - bindings evaluation
//     - signal handlers
//
// Aside from this, we also need to determine:
// 1) JavaScript Metrics
//     - simple expressions
//     - complex expressions
//     - instantiation vs evaluation time
//     - imports and nested imports
// 2) Context-switch costs
//     - how expensive is it to call a cpp function from QML
//     - how expensive is it to call a js function from cpp via QML
//     - how expensive is it to pass around objects between them
// 3) Complete creation time.
//     - loading + compilation + instantiation (for "application startup time" metric)
//
// In some cases, we want to include "initialization costs";
// i.e., we need to tell the engine not to cache type data resulting
// in compilation between rounds, and we need to tell the engine not
// to cache whatever it caches between instantiations of components.
// The reason for this is that it is often the "first start of application"
// performance which we're attempting to benchmark.

// define some custom types we use in test data functions.
typedef QList<QString> PropertyNameList;
Q_DECLARE_METATYPE(PropertyNameList);
typedef QList<QVariant> PropertyValueList;
Q_DECLARE_METATYPE(PropertyValueList);

class tst_holistic : public QObject
{
    Q_OBJECT

public:
    tst_holistic();

private slots:
    void initTestCase()
    {
        registerTypes();
        qRegisterMetaType<PropertyNameList>("PropertyNameList");
        qRegisterMetaType<PropertyValueList>("PropertyValueList");
    }

    void compilation_data();
    void compilation();
    void instantiation_data() { compilation_data(); }
    void instantiation();
    void creation_data() { compilation_data(); }
    void creation();
    void dynamicity_data();
    void dynamicity();

    void cppToJsDirect_data();
    void cppToJsDirect();
    void cppToJsIndirect();

    void typeResolution_data();
    void typeResolution();

private:
    QQmlEngine engine;
};

tst_holistic::tst_holistic()
{
}

inline QUrl TEST_FILE(const QString &filename)
{
    return QUrl::fromLocalFile(QLatin1String(SRCDIR) + QLatin1String("/data/") + filename);
}


void tst_holistic::compilation_data()
{
    QTest::addColumn<QStringList>("files");
    QTest::addColumn<int>("repetitions");

    QStringList f;

    // Benchmarks: a single, small component once with no caching.
    f << QString(SRCDIR + QLatin1String("/data/smallTargets/SmallOne.qml"));
    QTest::newRow("single small component") << f << 1;

    // Benchmarks: a single, small component ten times with caching.
    QTest::newRow("single small component cached") << f << 10; f.clear();

    // Benchmarks: a single, large component once with no caching.
    f << QString(SRCDIR + QLatin1String("/data/largeTargets/mousearea-example.qml"));
    QTest::newRow("single large component") << f << 1;

    // Benchmarks: a single, large component ten times with caching.
    QTest::newRow("single large component cached") << f << 10; f.clear();

    // Benchmarks: 4 small components once each with no caching
    f << QString(SRCDIR + QLatin1String("/data/smallTargets/SmallOne.qml"));
    f << QString(SRCDIR + QLatin1String("/data/smallTargets/SmallTwo.qml"));
    f << QString(SRCDIR + QLatin1String("/data/smallTargets/SmallThree.qml"));
    f << QString(SRCDIR + QLatin1String("/data/smallTargets/SmallFour.qml"));
    QTest::newRow("multiple small components") << f << 1;

    // Benchmarks: 4 small components ten times each with caching
    QTest::newRow("multiple small components cached") << f << 10; f.clear();

    // Benchmarks: 3 large components once each with no caching.
    f << QString(SRCDIR + QLatin1String("/data/largeTargets/mousearea-example.qml"));
    f << QString(SRCDIR + QLatin1String("/data/largeTargets/gridview-example.qml"));
    f << QString(SRCDIR + QLatin1String("/data/largeTargets/layoutdirection.qml"));
    QTest::newRow("multiple large components") << f << 1;

    // Benchmarks: 3 large components ten times each with caching.
    QTest::newRow("multiple large components cached") << f << 10; f.clear();

    // Benchmarks: single small component which imports a single small js file, no caching
    f << QString(SRCDIR + QLatin1String("/data/jsImports/Sssi.qml"));
    QTest::newRow("single small js import") << f << 1;

    // Benchmarks: single small component which imports a single small js file, 10 reps, with caching
    QTest::newRow("single small js import, cached") << f << 10; f.clear();

    // Benchmarks: single small component which imports multiple small js files (no deep nesting), no caching
    f << QString(SRCDIR + QLatin1String("/data/jsImports/Msbsi.qml"));
    QTest::newRow("multiple small js imports, shallow") << f << 1;

    // Benchmarks: single small component which imports multiple small js files (no deep nesting), 10 reps, with caching
    QTest::newRow("multiple small js imports, shallow, cached") << f << 10; f.clear();

    // Benchmarks: single small component which imports multiple small js files (with deep nesting), no caching
    f << QString(SRCDIR + QLatin1String("/data/jsImports/Msdsi.qml"));
    QTest::newRow("multiple small js imports, deeply nested") << f << 1;

    // Benchmarks: single small component which imports multiple small js files (with deep nesting), 10 reps, with caching
    QTest::newRow("multiple small js imports, deeply nested, cached") << f << 10; f.clear();

    // Benchmarks: single small component which imports multiple small js files (nested and unnested), no caching
    f << QString(SRCDIR + QLatin1String("/data/jsImports/Mssi.qml"));
    QTest::newRow("muliple small js imports, both") << f << 1;

    // Benchmarks: single small component which imports multiple small js files (nested and unnested), 10 reps, with caching
    QTest::newRow("muliple small js imports, both, cached") << f << 10; f.clear();

    // Benchmarks: single small component which imports a single large js file, no caching
    f << QString(SRCDIR + QLatin1String("/data/jsImports/Slsi.qml"));
    QTest::newRow("single large js import") << f << 1;

    // Benchmarks: single small component which imports a single large js file, 10 reps, with caching
    QTest::newRow("single large js import, cached") << f << 10; f.clear();

    // Benchmarks: single small component which imports multiple large js files (no deep nesting), no caching
    f << QString(SRCDIR + QLatin1String("/data/jsImports/Mlbsi.qml"));
    QTest::newRow("multiple large js imports, shallow") << f << 1;

    // Benchmarks: single small component which imports multiple large js files (no deep nesting), 10 reps, with caching
    QTest::newRow("multiple large js imports, shallow, cached") << f << 10; f.clear();

    // Benchmarks: single small component which imports multiple large js files (with deep nesting), no caching
    f << QString(SRCDIR + QLatin1String("/data/jsImports/Mldsi.qml"));
    QTest::newRow("multiple large js imports, deeply nested") << f << 1;

    // Benchmarks: single small component which imports multiple large js files (with deep nesting), 10 reps, with caching
    QTest::newRow("multiple large js imports, deeply nested, cached") << f << 10; f.clear();

    // Benchmarks: single small component which imports multiple large js files (nested and unnested), no caching
    f << QString(SRCDIR + QLatin1String("/data/jsImports/Mlsi.qml"));
    QTest::newRow("multiple large js imports, both") << f << 1;

    // Benchmarks: single small component which imports multiple large js files (nested and unnested), 10 reps, with caching
    QTest::newRow("multiple large js imports, both, cached") << f << 10; f.clear();

    // Benchmarks: single small component which imports multiple js files which all import a .pragma library js file, no caching
    f << QString(SRCDIR + QLatin1String("/data/jsImports/PragmaBm.qml"));
    QTest::newRow(".pragma library js import") << f << 1;

    // Benchmarks: single small component which imports multiple js files which all import a .pragma library js file, 10 reps, with caching
    QTest::newRow(".pragma library js import, cached") << f << 10; f.clear();

    // Benchmarks: single small component which imports a js file which imports a QML module, no caching
    f << QString(SRCDIR + QLatin1String("/data/jsImports/ModuleBm.qml"));
    QTest::newRow("import js with QML import") << f << 1;

    // Benchmarks: single small component which imports a js file which imports a QML module, 10 reps, with caching
    QTest::newRow("import js with QML import, cached") << f << 10; f.clear();

    // Benchmarks: single small component which imports multiple js files which all import a .pragma library js file and a QML module, no caching
    f << QString(SRCDIR + QLatin1String("/data/jsImports/PragmaModuleBm.qml"));
    QTest::newRow("import js with QML import and .pragma library") << f << 1;

    // Benchmarks: single small component which imports multiple js files which all import a .pragma library js file and a QML module, 10 reps, with caching
    QTest::newRow("import js with QML import and .pragma library, cached") << f << 10; f.clear();
}

void tst_holistic::compilation()
{
    // This function benchmarks the cost of loading and compiling specified QML files.
    // If "repetitions" is non-zero, each file from "files" will be compiled "repetitions"
    // times, without clearing the engine's component cache between compilations.

    QFETCH(QStringList, files);
    QFETCH(int, repetitions);
    Q_ASSERT(files.size() > 0);
    Q_ASSERT(repetitions > 0);

    QBENCHMARK {
        engine.clearComponentCache();
        for (int i = 0; i < repetitions; ++i) {
            for (int j = 0; j < files.size(); ++j) {
                QQmlComponent c(&engine, QUrl::fromLocalFile(files.at(j)));
            }
        }
    }
}

void tst_holistic::instantiation()
{
    // This function benchmarks the cost of instantiating components compiled from specified QML files.
    // If "repetitions" is non-zero, each component compiled from "files" will be instantiated "repetitions"
    // times, without clearing the component's instantiation cache between instantiations.

    QFETCH(QStringList, files);
    QFETCH(int, repetitions);
    Q_ASSERT(files.size() > 0);
    Q_ASSERT(repetitions > 0);

    QList<QQmlComponent*> components;
    for (int i = 0; i < files.size(); ++i) {
        QQmlComponent *c = new QQmlComponent(&engine, QUrl::fromLocalFile(files.at(i)));
        components.append(c);
    }

    QBENCHMARK {
        // XXX TODO: clear each component's instantiation cache

        for (int i = 0; i < repetitions; ++i) {
            for (int j = 0; j < components.size(); ++j) {
                QObject *obj = components.at(j)->create();
                delete obj;
            }
        }
    }

    // cleanup
    for (int i = 0; i < components.size(); ++i) {
        delete components.at(i);
    }
}

void tst_holistic::creation()
{
    // This function benchmarks the cost of loading, compiling  and instantiating specified QML files.
    // If "repetitions" is non-zero, each file from "files" will be created "repetitions"
    // times, without clearing the engine's component cache between component creation.

    QFETCH(QStringList, files);
    QFETCH(int, repetitions);
    Q_ASSERT(files.size() > 0);
    Q_ASSERT(repetitions > 0);

    QBENCHMARK {
        engine.clearComponentCache();
        for (int i = 0; i < repetitions; ++i) {
            for (int j = 0; j < files.size(); ++j) {
                QQmlComponent c(&engine, QUrl::fromLocalFile(files.at(j)));
                QObject *obj = c.create();
                delete obj;
            }
        }
    }
}

void tst_holistic::dynamicity_data()
{
    QTest::addColumn<QString>("file");
    QTest::addColumn<QString>("writeProperty");
    QTest::addColumn<QVariant>("writeValueOne");
    QTest::addColumn<QVariant>("writeValueTwo");
    QTest::addColumn<QString>("readProperty");

    QString f;

    // Benchmarks: single simple property binding
    f = QString(SRCDIR + QLatin1String("/data/dynamicTargets/DynamicOne.qml"));
    QTest::newRow("single simple property binding") << f << QString(QLatin1String("dynamicWidth")) << QVariant(300) << QVariant(500) << QString(QLatin1String("height"));

    // Benchmarks: multiple simple property bindings in one component
    f = QString(SRCDIR + QLatin1String("/data/dynamicTargets/DynamicTwo.qml"));
    QTest::newRow("multiple simple property bindings") << f << QString(QLatin1String("dynamicWidth")) << QVariant(300) << QVariant(500) << QString(QLatin1String("dynamicWidth"));

    // Benchmarks: single simple property binding plus onPropertyChanged slot
    f = QString(SRCDIR + QLatin1String("/data/dynamicTargets/DynamicThree.qml"));
    QTest::newRow("single simple plus slot") << f << QString(QLatin1String("dynamicWidth")) << QVariant(300) << QVariant(500) << QString(QLatin1String("dynamicWidth"));

    // Benchmarks: multiple simple property bindings plus multiple onPropertyChanged slots in one component
    f = QString(SRCDIR + QLatin1String("/data/dynamicTargets/DynamicFour.qml"));
    QTest::newRow("multiple simple plus slots") << f << QString(QLatin1String("dynamicWidth")) << QVariant(300) << QVariant(500) << QString(QLatin1String("dynamicHeight"));

    // Benchmarks: single simple js expression in a slot
    f = QString(SRCDIR + QLatin1String("/data/jsTargets/JsOne.qml"));
    QTest::newRow("single simple js expression slot") << f << QString(QLatin1String("dynamicWidth")) << QVariant(300) << QVariant(500) << QString(QLatin1String("dynamicWidth"));

    // Benchmarks: single complex js expression in a slot
    f = QString(SRCDIR + QLatin1String("/data/jsTargets/JsTwo.qml"));
    QTest::newRow("single complex js expression slot") << f << QString(QLatin1String("dynamicWidth")) << QVariant(300) << QVariant(500) << QString(QLatin1String("dynamicWidth"));

    // Benchmarks: simple property assignment and bindings update
    f = QString(SRCDIR + QLatin1String("/data/scopeSwitching/CppToQml.qml"));
    QTest::newRow("single simple property binding") << f << QString(QLatin1String("arbitrary")) << QVariant(36) << QVariant(35) << QString(QLatin1String("arbitrary"));
}

void tst_holistic::dynamicity()
{
    // This function benchmarks the cost of "continued operation" - signal invocation,
    // updating bindings, etc.  Note that we take two different writeValues in order
    // to force updates to occur, and we read to force lazy evaluation to occur.

    QFETCH(QString, file);
    QFETCH(QString, writeProperty);
    QFETCH(QVariant, writeValueOne);
    QFETCH(QVariant, writeValueTwo);
    QFETCH(QString, readProperty);

    QQmlComponent c(&engine, file);
    QObject *obj = c.create();

    QVariant readValue;
    QVariant writeValue;
    bool usedFirst = false;

    QBENCHMARK {
        if (usedFirst) {
            writeValue = writeValueTwo;
            usedFirst = false;
        } else {
            writeValue = writeValueOne;
            usedFirst = true;
        }

        obj->setProperty(writeProperty.toLatin1().constData(), writeValue);
        readValue = obj->property(readProperty.toLatin1().constData());
    }

    delete obj;
}







void tst_holistic::cppToJsDirect_data()
{
    QTest::addColumn<QString>("file");
    QTest::addColumn<QString>("methodName");

    QString f;

    // Benchmarks: cost of calling a js function from cpp directly
    f = QString(SRCDIR + QLatin1String("/data/scopeSwitching/CppToJs.qml"));
    QTest::newRow("cpp-to-js") << f << QString(QLatin1String("callJsFunction"));

    // Benchmarks: cost of calling js function which calls cpp function:
    // const CPP function with no return value and no arguments.
    f = QString(SRCDIR + QLatin1String("/data/scopeSwitching/JsToCppOne.qml"));
    QTest::newRow("cpp-to-js-to-cpp: no retn, no args") << f << QString(QLatin1String("callCppFunction"));

    // Benchmarks: cost of calling js function which calls cpp function:
    // nonconst CPP function with no return value and no arguments.
    f = QString(SRCDIR + QLatin1String("/data/scopeSwitching/JsToCppTwo.qml"));
    QTest::newRow("cpp-to-js-to-cpp: nonconst, no retn, no args") << f << QString(QLatin1String("callCppFunction"));

    // Benchmarks: cost of calling js function which calls cpp function:
    // const CPP function with no return value and a single integer argument.
    f = QString(SRCDIR + QLatin1String("/data/scopeSwitching/JsToCppThree.qml"));
    QTest::newRow("cpp-to-js-to-cpp: const, no retn, int arg") << f << QString(QLatin1String("callCppFunction"));

    // Benchmarks: cost of calling js function which calls cpp function:
    // nonconst CPP function with no return value and a single integer argument.
    f = QString(SRCDIR + QLatin1String("/data/scopeSwitching/JsToCppFour.qml"));
    QTest::newRow("cpp-to-js-to-cpp: nonconst, no retn, int arg") << f << QString(QLatin1String("callCppFunction"));

    // Benchmarks: cost of calling js function which calls cpp function:
    // const CPP function with an integer return value and no arguments.
    f = QString(SRCDIR + QLatin1String("/data/scopeSwitching/JsToCppFive.qml"));
    QTest::newRow("cpp-to-js-to-cpp: const, int retn, no args") << f << QString(QLatin1String("callCppFunction"));

    // Benchmarks: cost of calling js function which calls cpp function:
    // nonconst CPP function with an integer return value and no arguments.
    f = QString(SRCDIR + QLatin1String("/data/scopeSwitching/JsToCppSix.qml"));
    QTest::newRow("cpp-to-js-to-cpp: nonconst, int retn, no args") << f << QString(QLatin1String("callCppFunction"));

    // Benchmarks: cost of calling js function which calls cpp function:
    // const CPP function with an integer return value and a single integer argument.
    f = QString(SRCDIR + QLatin1String("/data/scopeSwitching/JsToCppSeven.qml"));
    QTest::newRow("cpp-to-js-to-cpp: const, int retn, int arg") << f << QString(QLatin1String("callCppFunction"));

    // Benchmarks: cost of calling js function which calls cpp function:
    // nonconst CPP function with an integer return value and a single integer argument.
    f = QString(SRCDIR + QLatin1String("/data/scopeSwitching/JsToCppEight.qml"));
    QTest::newRow("cpp-to-js-to-cpp: nonconst, int retn, int arg") << f << QString(QLatin1String("callCppFunction"));

    // Benchmarks: cost of calling js function which calls cpp function:
    // const CPP function with a variant return value and multiple integer arguments.
    f = QString(SRCDIR + QLatin1String("/data/scopeSwitching/JsToCppNine.qml"));
    QTest::newRow("cpp-to-js-to-cpp: const, variant retn, int args") << f << QString(QLatin1String("callCppFunction"));

    // Benchmarks: cost of calling js function which calls cpp function:
    // nonconst CPP function with a variant return value and multiple integer arguments.
    f = QString(SRCDIR + QLatin1String("/data/scopeSwitching/JsToCppTen.qml"));
    QTest::newRow("cpp-to-js-to-cpp: nonconst, variant retn, int args") << f << QString(QLatin1String("callCppFunction"));

    // Benchmarks: cost of calling js function which calls cpp function:
    // nonconst CPP function with a variant return value and multiple integer arguments.
    f = QString(SRCDIR + QLatin1String("/data/scopeSwitching/JsToCppEleven.qml"));
    QTest::newRow("cpp-to-js-to-cpp: nonconst, variant retn, variant + int args") << f << QString(QLatin1String("callCppFunction"));

    // Benchmarks: calling js function which copies scarce resources by calling back into cpp scope
    f = QString(SRCDIR + QLatin1String("/data/scopeSwitching/ScarceOne.qml"));
    QTest::newRow("cpp-to-js-to-coo: copy scarce resources") << f << QString(QLatin1String("copyScarceResources"));
}


void tst_holistic::cppToJsDirect()
{
    // This function benchmarks the cost of calling from CPP scope to JS scope
    // (and possibly vice versa, if the invoked js method then calls to cpp).

    QFETCH(QString, file);
    QFETCH(QString, methodName);

    QQmlComponent c(&engine, file);
    QObject *obj = c.create();

    QBENCHMARK {
        QMetaObject::invokeMethod(obj, methodName.toLatin1().constData());
    }

    delete obj;
}


void tst_holistic::cppToJsIndirect()
{
    // This function benchmarks the cost of binding scarce resources
    // to properties of a QML component.  The engine should automatically release such
    // resources when they are no longer used.
    // The benchmark deliberately causes change signals to be emitted (and
    // modifies the scarce resources) so that the properties are updated.

    QQmlComponent c(&engine, QString(SRCDIR + QLatin1String("/data/scopeSwitching/ScarceTwo.qml")));
    QObject *obj = c.create();

    ScarceResourceProvider *srp = 0;
    srp = qobject_cast<ScarceResourceProvider*>(QQmlProperty::read(obj, "a").value<QObject*>());

    QBENCHMARK {
        srp->changeResources(); // will cause small+large scarce resources changed signals to be emitted.
    }

    delete obj;
}





void tst_holistic::typeResolution_data()
{
    QTest::addColumn<QString>("file");
    QTest::addColumn<PropertyNameList>("propertyNameOne");
    QTest::addColumn<PropertyValueList>("propertyValueOne");
    QTest::addColumn<PropertyNameList>("propertyNameTwo");
    QTest::addColumn<PropertyValueList>("propertyValueTwo");
    QTest::addColumn<int>("repetitions");

    QString f;
    PropertyNameList pn1;
    PropertyValueList pv1;
    PropertyNameList pn2;
    PropertyValueList pv2;

    // Benchmarks: resolving nested ids and types, no caching
    f = QString(SRCDIR + QLatin1String("/data/resolutionTargets/ResolveOne.qml"));
    pn1 << QString(QLatin1String("baseWidth")) << QString(QLatin1String("baseHeight")) << QString(QLatin1String("baseColor"));
    pv1 << QVariant(401) << QVariant(402) << QVariant(QString(QLatin1String("brown")));
    pn2 << QString(QLatin1String("baseWidth")) << QString(QLatin1String("baseHeight")) << QString(QLatin1String("baseColor"));
    pv2 << QVariant(403) << QVariant(404) << QVariant(QString(QLatin1String("orange")));
    QTest::newRow("nested id resolution") << f << pn1 << pv1 << pn2 << pv2 << 1;

    // Benchmarks: resolving nested ids and types, 10 reps with caching
    QTest::newRow("nested id resolution, cached") << f << pn1 << pv1 << pn2 << pv2 << 10;
    pn1.clear(); pn2.clear(); pv1.clear(); pv2.clear();
}

void tst_holistic::typeResolution()
{
    // This function benchmarks the cost of "continued operation" (signal invocation,
    // updating bindings, etc) where the component has lots of nested items with
    // lots of resolving required.  Note that we take two different writeValues in order
    // to force updates to occur.

    QFETCH(QString, file);
    QFETCH(PropertyNameList, propertyNameOne);
    QFETCH(PropertyValueList, propertyValueOne);
    QFETCH(PropertyNameList, propertyNameTwo);
    QFETCH(PropertyValueList, propertyValueTwo);
    QFETCH(int, repetitions);

    Q_ASSERT(propertyNameOne.size() == propertyValueOne.size());
    Q_ASSERT(propertyNameTwo.size() == propertyValueTwo.size());
    Q_ASSERT(repetitions > 0);

    QQmlComponent c(&engine, file);
    QObject *obj = c.create();

    PropertyNameList writeProperty;
    PropertyValueList writeValue;
    bool usedFirst = false;

    QBENCHMARK {
        for (int i = 0; i < repetitions; ++i) {
            if (usedFirst) {
                writeProperty = propertyNameOne;
                writeValue = propertyValueOne;
                usedFirst = false;
            } else {
                writeProperty = propertyNameTwo;
                writeValue = propertyValueTwo;
                usedFirst = true;
            }

            for (int j = 0; j < writeProperty.size(); ++j) {
                obj->setProperty(writeProperty.at(j).toLatin1().constData(), writeValue.at(j));
            }
        }
    }

    delete obj;
}


QTEST_MAIN(tst_holistic)

#include "tst_holistic.moc"
