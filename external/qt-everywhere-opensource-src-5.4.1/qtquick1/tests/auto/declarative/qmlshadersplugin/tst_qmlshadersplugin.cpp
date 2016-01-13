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
#include <qdeclarativedatatest.h>
#include <QtDeclarative>
#include "../../../../src/imports/shaders/shadereffectitem.h"
#include "../../../../src/imports/shaders/shadereffectsource.h"
#include "../../../../src/imports/shaders/shadereffect.h"

static const char qt_default_vertex_code[] =
        "uniform highp mat4 qt_ModelViewProjectionMatrix;\n"
        "attribute highp vec4 qt_Vertex;\n"
        "attribute highp vec2 qt_MultiTexCoord0;\n"
        "varying highp vec2 qt_TexCoord0;\n"
        "void main(void)\n"
        "{\n"
            "qt_TexCoord0 = qt_MultiTexCoord0;\n"
            "gl_Position = qt_ModelViewProjectionMatrix * qt_Vertex;\n"
        "}\n";

static const char qt_default_fragment_code[] =
        "varying highp vec2 qt_TexCoord0;\n"
        "uniform lowp sampler2D source;\n"
        "void main(void)\n"
        "{\n"
            "gl_FragColor = texture2D(source, qt_TexCoord0.st);\n"
        "}\n";

class tst_qmlshadersplugin : public QDeclarativeDataTest
{
    Q_OBJECT

private slots:
    void initTestCase();
    void shaderEffectItemAPI();
    void shaderEffectSourceAPI();
    void combined();

private:
    QDeclarativeEngine engine;
};

void tst_qmlshadersplugin::initTestCase()
{
    QDeclarativeDataTest::initTestCase();

    const char *uri ="Qt.labs.shaders";
    qmlRegisterType<ShaderEffectItem>(uri, 1, 0, "ShaderEffectItem");
    qmlRegisterType<ShaderEffectSource>(uri, 1, 0, "ShaderEffectSource");
}


void tst_qmlshadersplugin::shaderEffectItemAPI()
{
    // Creation
    QString componentStr = "import QtQuick 1.0\n"
            "import Qt.labs.shaders 1.0\n"
            "ShaderEffectItem {\n"
            "property variant source\n"
            "width: 200; height: 300\n"
            "}";
    QDeclarativeComponent component(&engine);
    component.setData(componentStr.toLatin1(), testFileUrl(""));

    QObject *obj = component.create();
    QTest::qWait(100);
    QVERIFY(obj != 0);

    // Default values
    QCOMPARE(obj->property("width").toDouble(), 200.);
    QCOMPARE(obj->property("height").toDouble(), 300.);
    QCOMPARE(obj->property("fragmentShader").toString(), QString(""));
    QCOMPARE(obj->property("vertexShader").toString(), QString(""));
    QCOMPARE(obj->property("blending").toBool(), true);
    QCOMPARE(obj->property("meshResolution").toSize(), QSize(1, 1));
    QCOMPARE(obj->property("visible").toBool(), true);

    // Seting the values
    QVERIFY(obj->setProperty("fragmentShader", QString(qt_default_fragment_code)));
    QVERIFY(obj->setProperty("vertexShader", QString(qt_default_vertex_code)));
    QVERIFY(obj->setProperty("blending", false));
    QVERIFY(obj->setProperty("meshResolution", QSize(20, 10)));
    QVERIFY(obj->setProperty("visible", false));

    QCOMPARE(obj->property("fragmentShader").toString(), QString(qt_default_fragment_code));
    QCOMPARE(obj->property("vertexShader").toString(), QString(qt_default_vertex_code));
    QCOMPARE(obj->property("blending").toBool(), false);
    QCOMPARE(obj->property("meshResolution").toSize(), QSize(20, 10));
    QCOMPARE(obj->property("visible").toBool(), false);

    delete obj;
}

void tst_qmlshadersplugin::shaderEffectSourceAPI()
{
    // Creation
    QString componentStr = "import QtQuick 1.0\n"
            "import Qt.labs.shaders 1.0\n"
            "ShaderEffectSource {}";
    QDeclarativeComponent shaderEffectSourceComponent(&engine);
    shaderEffectSourceComponent.setData(componentStr.toLatin1(), testFileUrl(""));

    QObject *obj = shaderEffectSourceComponent.create();
    QTest::qWait(100);
    QVERIFY(obj != 0);

    // Default values
    QCOMPARE(obj->property("sourceRect").toRect(), QRect(0, 0, 0, 0));
    QCOMPARE(obj->property("textureSize").toSize(), QSize(0, 0));
    QCOMPARE(obj->property("live").toBool(), true);
    QCOMPARE(obj->property("hideSource").toBool(), false);
    QCOMPARE(obj->property("wrapMode").toUInt(), static_cast<unsigned int>(ShaderEffectSource::ClampToEdge));

    // Seting the values
    componentStr = "import QtQuick 1.0\n"
        "Item {}";
    QDeclarativeComponent itemComponent(&engine);
    itemComponent.setData(componentStr.toLatin1(), testFileUrl(""));
    QDeclarativeItem *item = qobject_cast<QDeclarativeItem *> (itemComponent.create());
    QVERIFY(item != 0);

    QVERIFY(obj->setProperty("sourceItem", QVariant::fromValue(item)));
    QVERIFY(obj->setProperty("sourceRect", QRect(10, 20, 30, 40)));
    QVERIFY(obj->setProperty("textureSize", QSize(50, 100)));
    QVERIFY(obj->setProperty("live", false));
    QVERIFY(obj->setProperty("hideSource", true));
    QVERIFY(obj->setProperty("wrapMode", static_cast<unsigned int>(ShaderEffectSource::Repeat)));

    QCOMPARE(obj->property("sourceItem"), QVariant::fromValue(item));
    QCOMPARE(obj->property("sourceRect").toRect(), QRect(10, 20, 30, 40));
    QCOMPARE(obj->property("textureSize").toSize(), QSize(50, 100));
    QCOMPARE(obj->property("live").toBool(), false);
    QCOMPARE(obj->property("hideSource").toBool(), true);
    QCOMPARE(obj->property("wrapMode").toUInt(), static_cast<unsigned int>(ShaderEffectSource::Repeat));

    delete item;
    delete obj;
}

void tst_qmlshadersplugin::combined()
{
    QGLFormat format = QGLFormat::defaultFormat();
    format.setSampleBuffers(false);
    format.setSwapInterval(1);

    QGLWidget* glWidget = new QGLWidget(format);
    glWidget->setAutoFillBackground(false);

    QDeclarativeView view;
    view.setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    view.setAttribute(Qt::WA_OpaquePaintEvent);
    view.setAttribute(Qt::WA_NoSystemBackground);
    view.setViewport(glWidget);
    view.setSource(testFileUrl("main.qml"));
    view.show();
    QTest::qWait(1000);

    QObject *item = view.rootObject()->findChild<QDeclarativeItem*>("effectItem");
    QVERIFY(item != 0);

    QObject *src = view.rootObject()->findChild<QDeclarativeItem*>("effectSource");
    QVERIFY(src != 0);

    QCOMPARE(item->property("source"), QVariant::fromValue(src));
}

QTEST_MAIN(tst_qmlshadersplugin)

#include "tst_qmlshadersplugin.moc"
