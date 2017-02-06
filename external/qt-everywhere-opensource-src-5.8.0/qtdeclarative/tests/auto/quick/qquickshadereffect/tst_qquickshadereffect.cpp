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

#include <qtest.h>

#include <QList>
#include <QByteArray>
#include <private/qquickopenglshadereffect_p.h>
#include <QMatrix4x4>
#include <QtQuick/QQuickView>
#include <QtQml/QQmlEngine>
#include "../../shared/util.h"

class TestShaderEffect : public QQuickShaderEffect
{
    Q_OBJECT
    Q_PROPERTY(QVariant source READ dummyRead NOTIFY sourceChanged)
    Q_PROPERTY(QVariant _0aA9zZ READ dummyRead NOTIFY dummyChanged)
    Q_PROPERTY(QVariant x86 READ dummyRead NOTIFY dummyChanged)
    Q_PROPERTY(QVariant X READ dummyRead NOTIFY dummyChanged)
    Q_PROPERTY(QMatrix4x4 mat4x4 READ mat4x4Read NOTIFY dummyChanged)

public:
    QMatrix4x4 mat4x4Read() const { return QMatrix4x4(1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1); }
    QVariant dummyRead() const { return QVariant(); }

    int signalsConnected = 0;

protected:
    void connectNotify(const QMetaMethod &) { ++signalsConnected; }
    void disconnectNotify(const QMetaMethod &) { --signalsConnected; }

signals:
    void dummyChanged();
    void sourceChanged();

private:
};

class tst_qquickshadereffect : public QQmlDataTest
{
    Q_OBJECT
public:
    tst_qquickshadereffect();

private slots:
    void initTestCase();
    void cleanupTestCase();

    void lookThroughShaderCode_data();
    void lookThroughShaderCode();

    void deleteSourceItem();
    void deleteShaderEffectSource();
    void twoImagesOneShaderEffect();

private:
    enum PresenceFlags {
        VertexPresent = 0x01,
        TexCoordPresent = 0x02,
        MatrixPresent = 0x04,
        OpacityPresent = 0x08,
        SourcePresent = 0x10
    };
};

tst_qquickshadereffect::tst_qquickshadereffect()
{
    qmlRegisterType<TestShaderEffect>("ShaderEffectTest", 1, 0, "TestShaderEffect");
}

void tst_qquickshadereffect::initTestCase()
{
    QQmlDataTest::initTestCase();
}

void tst_qquickshadereffect::cleanupTestCase()
{
}

void tst_qquickshadereffect::lookThroughShaderCode_data()
{
    QTest::addColumn<QByteArray>("vertexShader");
    QTest::addColumn<QByteArray>("fragmentShader");
    QTest::addColumn<int>("presenceFlags");

    QTest::newRow("default")
            << QByteArray("uniform highp mat4 qt_Matrix;                                  \n"
                          "attribute highp vec4 qt_Vertex;                                \n"
                          "attribute highp vec2 qt_MultiTexCoord0;                        \n"
                          "varying highp vec2 qt_TexCoord0;                               \n"
                          "void main() {                                                  \n"
                          "    qt_TexCoord0 = qt_MultiTexCoord0;                          \n"
                          "    gl_Position = qt_Matrix * qt_Vertex;                       \n"
                          "}")
            << QByteArray("varying highp vec2 qt_TexCoord0;                                   \n"
                          "uniform sampler2D source;                                          \n"
                          "uniform lowp float qt_Opacity;                                     \n"
                          "void main() {                                                      \n"
                          "    gl_FragColor = texture2D(source, qt_TexCoord0) * qt_Opacity;   \n"
                          "}")
            << (VertexPresent | TexCoordPresent | MatrixPresent | OpacityPresent | SourcePresent);

    QTest::newRow("empty")
            << QByteArray(" ") // one space -- if completely empty, default will be used instead.
            << QByteArray(" ")
            << 0;


    QTest::newRow("inside line comments")
            << QByteArray("//uniform highp mat4 qt_Matrix;\n"
                          "attribute highp vec4 qt_Vertex;\n"
                          "// attribute highp vec2 qt_MultiTexCoord0;")
            << QByteArray("uniform int source; // uniform lowp float qt_Opacity;")
            << (VertexPresent | SourcePresent);

    QTest::newRow("inside block comments")
            << QByteArray("/*uniform highp mat4 qt_Matrix;\n"
                          "*/attribute highp vec4 qt_Vertex;\n"
                          "/*/attribute highp vec2 qt_MultiTexCoord0;//**/")
            << QByteArray("/**/uniform int source; /* uniform lowp float qt_Opacity; */")
            << (VertexPresent | SourcePresent);

    QTest::newRow("inside preprocessor directive")
            << QByteArray("#define uniform\nhighp mat4 qt_Matrix;\n"
                          "attribute highp vec4 qt_Vertex;\n"
                          "#if\\\nattribute highp vec2 qt_MultiTexCoord0;")
            << QByteArray("uniform int source;\n"
                          "    #    undef uniform lowp float qt_Opacity;")
            << (VertexPresent | SourcePresent);


    QTest::newRow("line comments between")
            << QByteArray("uniform//foo\nhighp//bar\nmat4//baz\nqt_Matrix;\n"
                          "attribute//\nhighp//\nvec4//\nqt_Vertex;\n"
                          " //*/ uniform \n attribute //\\ \n highp //// \n vec2 //* \n qt_MultiTexCoord0;")
            << QByteArray("uniform// lowp float qt_Opacity;\nsampler2D source;")
            << (VertexPresent | TexCoordPresent | MatrixPresent | SourcePresent);

    QTest::newRow("block comments between")
            << QByteArray("uniform/*foo*/highp/*/bar/*/mat4/**//**/qt_Matrix;\n"
                          "attribute/**/highp/**/vec4/**/qt_Vertex;\n"
                          " /* * */ attribute /*///*/ highp /****/ vec2 /**/ qt_MultiTexCoord0;")
            << QByteArray("uniform/*/ uniform//lowp/*float qt_Opacity;*/sampler2D source;")
            << (VertexPresent | TexCoordPresent | MatrixPresent | SourcePresent);

    QTest::newRow("preprocessor directive between")
            << QByteArray("uniform\n#foo\nhighp\n#bar\nmat4\n#baz\\\nblimey\nqt_Matrix;\n"
                          "attribute\n#\nhighp\n#\nvec4\n#\nqt_Vertex;\n"
                          " #uniform \n attribute \n # foo \n highp \n #  bar \n vec2 \n#baz \n qt_MultiTexCoord0;")
            << QByteArray("uniform\n#if lowp float qt_Opacity;\nsampler2D source;")
            << (VertexPresent | TexCoordPresent | MatrixPresent | SourcePresent);

    QTest::newRow("newline between")
            << QByteArray("uniform\nhighp\nmat4\nqt_Matrix\n;\n"
                          "attribute  \t\r\n  highp  \n  vec4  \n\n  qt_Vertex  ;\n"
                          "   \n   attribute   \n   highp   \n   vec2   \n   qt_Multi\nTexCoord0  \n  ;")
            << QByteArray("uniform\nsampler2D\nsource;"
                          "uniform lowp float qt_Opacity;")
            << (VertexPresent | MatrixPresent | OpacityPresent | SourcePresent);


    QTest::newRow("extra characters #1")
            << QByteArray("funiform highp mat4 qt_Matrix;\n"
                          "attribute highp vec4 qt_Vertex_;\n"
                          "attribute highp vec2 qqt_MultiTexCoord0;")
            << QByteArray("uniformm int source;\n"
                          "uniform4 lowp float qt_Opacity;")
            << 0;

    QTest::newRow("extra characters #2")
            << QByteArray("attribute phighp vec4 qt_Vertex;\n"
                          "attribute highpi vec2 qt_MultiTexCoord0;"
                          "fattribute highp vec4 qt_Vertex;\n"
                          "attributed highp vec2 qt_MultiTexCoord0;")
            << QByteArray(" ")
            << 0;

    QTest::newRow("missing characters #1")
            << QByteArray("unifor highp mat4 qt_Matrix;\n"
                          "attribute highp vec4 qt_Vert;\n"
                          "attribute highp vec2 MultiTexCoord0;")
            << QByteArray("niform int source;\n"
                          "uniform qt_Opacity;")
            << 0;

    QTest::newRow("missing characters #2")
            << QByteArray("attribute high vec4 qt_Vertex;\n"
                          "attribute ighp vec2 qt_MultiTexCoord0;"
                          "tribute highp vec4 qt_Vertex;\n"
                          "attrib highp vec2 qt_MultiTexCoord0;")
            << QByteArray(" ")
            << 0;

    QTest::newRow("precision")
            << QByteArray("uniform mat4 qt_Matrix;\n"
                          "attribute kindofhighp vec4 qt_Vertex;\n"
                          "attribute highp qt_MultiTexCoord0;\n")
            << QByteArray("uniform lowp float qt_Opacity;\n"
                          "uniform mediump float source;\n")
            << (MatrixPresent | OpacityPresent | SourcePresent);


    QTest::newRow("property name #1")
            << QByteArray("uniform highp vec3 _0aA9zZ;")
            << QByteArray(" ")
            << int(SourcePresent);

    QTest::newRow("property name #2")
            << QByteArray("uniform mediump vec2 x86;")
            << QByteArray(" ")
            << int(SourcePresent);

    QTest::newRow("property name #3")
            << QByteArray("uniform lowp float X;")
            << QByteArray(" ")
            << int(SourcePresent);

    QTest::newRow("property name #4")
            << QByteArray("uniform highp mat4 mat4x4;")
            << QByteArray(" ")
            << int(SourcePresent);
}

void tst_qquickshadereffect::lookThroughShaderCode()
{
    QFETCH(QByteArray, vertexShader);
    QFETCH(QByteArray, fragmentShader);
    QFETCH(int, presenceFlags);

    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData("import QtQuick 2.0\nimport ShaderEffectTest 1.0\nTestShaderEffect {}", QUrl());
    QScopedPointer<TestShaderEffect> item(qobject_cast<TestShaderEffect*>(component.create()));
    QCOMPARE(item->signalsConnected, 1);

    QString expected;
    if ((presenceFlags & VertexPresent) == 0)
        expected += "Warning: Missing reference to \'qt_Vertex\'.\n";
    if ((presenceFlags & TexCoordPresent) == 0)
        expected += "Warning: Missing reference to \'qt_MultiTexCoord0\'.\n";
    if ((presenceFlags & MatrixPresent) == 0)
        expected += "Warning: Vertex shader is missing reference to \'qt_Matrix\'.\n";
    if ((presenceFlags & OpacityPresent) == 0)
        expected += "Warning: Shaders are missing reference to \'qt_Opacity\'.\n";

    item->setVertexShader(vertexShader);
    item->setFragmentShader(fragmentShader);
    QCOMPARE(item->parseLog(), expected);

    // If the uniform was successfully parsed, the notify signal has been connected to an update slot.
    QCOMPARE(item->signalsConnected, (presenceFlags & SourcePresent) ? 2 : 1);
}

void tst_qquickshadereffect::deleteSourceItem()
{
    // purely to ensure that deleting the sourceItem of a shader doesn't cause a crash
    QQuickView *view = new QQuickView(0);
    view->setSource(QUrl::fromLocalFile(testFile("deleteSourceItem.qml")));
    view->show();
    QVERIFY(QTest::qWaitForWindowExposed(view));
    QVERIFY(view);
    QObject *obj = view->rootObject();
    QVERIFY(obj);
    QMetaObject::invokeMethod(obj, "setDeletedSourceItem");
    QTest::qWait(50);
    delete view;
}

void tst_qquickshadereffect::deleteShaderEffectSource()
{
    // purely to ensure that deleting the sourceItem of a shader doesn't cause a crash
    QQuickView *view = new QQuickView(0);
    view->setSource(QUrl::fromLocalFile(testFile("deleteShaderEffectSource.qml")));
    view->show();
    QVERIFY(QTest::qWaitForWindowExposed(view));
    QVERIFY(view);
    QObject *obj = view->rootObject();
    QVERIFY(obj);
    QMetaObject::invokeMethod(obj, "setDeletedShaderEffectSource");
    QTest::qWait(50);
    delete view;
}

void tst_qquickshadereffect::twoImagesOneShaderEffect()
{
    // purely to ensure that deleting the sourceItem of a shader doesn't cause a crash
    QQuickView *view = new QQuickView(0);
    view->setSource(QUrl::fromLocalFile(testFile("twoImagesOneShaderEffect.qml")));
    view->show();
    QVERIFY(QTest::qWaitForWindowExposed(view));
    QVERIFY(view);
    QObject *obj = view->rootObject();
    QVERIFY(obj);
    delete view;
}

QTEST_MAIN(tst_qquickshadereffect)

#include "tst_qquickshadereffect.moc"
