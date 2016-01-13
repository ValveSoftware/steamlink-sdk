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
#include <QQmlEngine>
#include <QQmlComponent>
#include <QTranslator>
#include <QQmlContext>
#include <private/qqmlcompiler_p.h>
#include <private/qqmlengine_p.h>
#include "../../shared/util.h"

class tst_qqmltranslation : public QQmlDataTest
{
    Q_OBJECT
public:
    tst_qqmltranslation() {}

private slots:
    void translation_data();
    void translation();
    void idTranslation();
};

void tst_qqmltranslation::translation_data()
{
    QTest::addColumn<QString>("translation");
    QTest::addColumn<QUrl>("testFile");
    QTest::addColumn<bool>("verifyCompiledData");

    QTest::newRow("qml") << QStringLiteral("qml_fr") << testFileUrl("translation.qml") << true;
    QTest::newRow("qrc") << QStringLiteral(":/qml_fr.qm") << QUrl("qrc:/translation.qml") << true;
    QTest::newRow("js") << QStringLiteral("qml_fr") << testFileUrl("jstranslation.qml") << false;
}

void tst_qqmltranslation::translation()
{
    QFETCH(QString, translation);
    QFETCH(QUrl, testFile);
    QFETCH(bool, verifyCompiledData);

    QTranslator translator;
    translator.load(translation, dataDirectory());
    QCoreApplication::installTranslator(&translator);

    QQmlEngine engine;
    QQmlComponent component(&engine, testFile);
    QObject *object = component.create();
    QVERIFY(object != 0);

    if (verifyCompiledData) {
        QQmlContext *context = qmlContext(object);
        QQmlEnginePrivate *engine = QQmlEnginePrivate::get(context->engine());
        QQmlTypeData *typeData = engine->typeLoader.getType(context->baseUrl());
        QQmlCompiledData *cdata = typeData->compiledData();
        QVERIFY(cdata);

        QSet<QString> compiledTranslations;
        compiledTranslations << QStringLiteral("basic")
                             << QStringLiteral("disambiguation")
                             << QStringLiteral("singular") << QStringLiteral("plural");

        const QV4::CompiledData::Unit *unit = cdata->compilationUnit->data;
        const QV4::CompiledData::Object *rootObject = unit->objectAt(unit->indexOfRootObject);
        const QV4::CompiledData::Binding *binding = rootObject->bindingTable();
        for (quint32 i = 0; i < rootObject->nBindings; ++i, ++binding) {
            const QString propertyName = unit->stringAt(binding->propertyNameIndex);

            const bool expectCompiledTranslation = compiledTranslations.contains(propertyName);

            if (expectCompiledTranslation) {
                if (binding->type != QV4::CompiledData::Binding::Type_Translation)
                    qDebug() << "binding for property" << propertyName << "is not a compiled translation";
                QCOMPARE(binding->type, quint32(QV4::CompiledData::Binding::Type_Translation));
            } else {
                if (binding->type == QV4::CompiledData::Binding::Type_Translation)
                    qDebug() << "binding for property" << propertyName << "is not supposed to be a compiled translation";
                QVERIFY(binding->type != QV4::CompiledData::Binding::Type_Translation);
            }
        }
    }

    QCOMPARE(object->property("basic").toString(), QLatin1String("bonjour"));
    QCOMPARE(object->property("basic2").toString(), QLatin1String("au revoir"));
    QCOMPARE(object->property("basic3").toString(), QLatin1String("bonjour"));
    QCOMPARE(object->property("disambiguation").toString(), QLatin1String("salut"));
    QCOMPARE(object->property("disambiguation2").toString(), QString::fromUtf8("\xc3\xa0 plus tard"));
    QCOMPARE(object->property("disambiguation3").toString(), QLatin1String("salut"));
    QCOMPARE(object->property("noop").toString(), QLatin1String("bonjour"));
    QCOMPARE(object->property("noop2").toString(), QLatin1String("au revoir"));
    QCOMPARE(object->property("singular").toString(), QLatin1String("1 canard"));
    QCOMPARE(object->property("singular2").toString(), QLatin1String("1 canard"));
    QCOMPARE(object->property("plural").toString(), QLatin1String("2 canards"));
    QCOMPARE(object->property("plural2").toString(), QLatin1String("2 canards"));

    QCoreApplication::removeTranslator(&translator);
    delete object;
}

void tst_qqmltranslation::idTranslation()
{
    QTranslator translator;
    translator.load(QLatin1String("qmlid_fr"), dataDirectory());
    QCoreApplication::installTranslator(&translator);

    QQmlEngine engine;
    QQmlComponent component(&engine, testFileUrl("idtranslation.qml"));
    QObject *object = component.create();
    QVERIFY(object != 0);

    {
        QQmlContext *context = qmlContext(object);
        QQmlEnginePrivate *engine = QQmlEnginePrivate::get(context->engine());
        QQmlTypeData *typeData = engine->typeLoader.getType(context->baseUrl());
        QQmlCompiledData *cdata = typeData->compiledData();
        QVERIFY(cdata);

        const QV4::CompiledData::Unit *unit = cdata->compilationUnit->data;
        const QV4::CompiledData::Object *rootObject = unit->objectAt(unit->indexOfRootObject);
        const QV4::CompiledData::Binding *binding = rootObject->bindingTable();
        for (quint32 i = 0; i < rootObject->nBindings; ++i, ++binding) {
            const QString propertyName = unit->stringAt(binding->propertyNameIndex);
            if (propertyName == "idTranslation") {
                if (binding->type != QV4::CompiledData::Binding::Type_TranslationById)
                    qDebug() << "binding for property" << propertyName << "is not a compiled translation";
                QCOMPARE(binding->type, quint32(QV4::CompiledData::Binding::Type_TranslationById));
            } else {
                QVERIFY(binding->type != QV4::CompiledData::Binding::Type_Translation);
            }
        }
    }

    QCOMPARE(object->property("idTranslation").toString(), QLatin1String("bonjour tout le monde"));
    QCOMPARE(object->property("idTranslation2").toString(), QLatin1String("bonjour tout le monde"));
    QCOMPARE(object->property("idTranslation3").toString(), QLatin1String("bonjour tout le monde"));

    QCoreApplication::removeTranslator(&translator);
    delete object;
}

QTEST_MAIN(tst_qqmltranslation)

#include "tst_qqmltranslation.moc"
