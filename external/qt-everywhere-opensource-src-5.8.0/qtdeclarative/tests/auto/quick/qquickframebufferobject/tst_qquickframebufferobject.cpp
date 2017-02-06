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

#include <QtQuick/qquickitem.h>
#include <QtQuick/qquickview.h>
#include <QtGui/qopenglcontext.h>
#include <QtGui/qopenglframebufferobject.h>
#include <QtGui/qopenglfunctions.h>

#include <QtQuick/QQuickFramebufferObject>

#include "../../shared/util.h"

#ifndef GL_MAX_SAMPLES
#define GL_MAX_SAMPLES 0x8D57
#endif

struct FrameInfo {
    int renderCount;
    int createFBOCount;
    bool msaaSupported;
    bool msaaEnabled;
    QSize fboSize;
} frameInfo;

class ColorRenderer : public QQuickFramebufferObject::Renderer, protected QOpenGLFunctions
{
public:
    void render();
    void synchronize(QQuickFramebufferObject *item);
    QOpenGLFramebufferObject *createFramebufferObject(const QSize &size);

    QSize textureSize;
    QColor color;
    bool msaa;
};

class FBOItem : public QQuickFramebufferObject
{
    Q_OBJECT

    Q_PROPERTY(bool msaa READ msaa WRITE setMsaa NOTIFY msaaChanged)
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)

    Q_PROPERTY(QSize textureSize READ textureSize WRITE setTextureSize NOTIFY textureSizeChanged)

public:
    Renderer *createRenderer() const { return new ColorRenderer(); }

    void setColor(const QColor &color) { m_color = color; colorChanged(m_color); }
    QColor color() const { return m_color; }

    void setMsaa(bool msaa) { m_msaa = msaa; msaaChanged(m_msaa); }
    bool msaa() const { return m_msaa; }

    void setTextureSize(const QSize &size) { m_textureSize = size; textureSizeChanged(m_textureSize); }
    QSize textureSize() const { return m_textureSize; }

signals:
    void colorChanged(const QColor &color);
    void msaaChanged(bool msaa);
    void textureSizeChanged(const QSize &size);

public:
    bool m_msaa;
    QColor m_color;
    QSize m_textureSize;

};

void ColorRenderer::render()
{
    glClearColor(color.redF(), color.greenF(), color.blueF(), color.alphaF());
    glClear(GL_COLOR_BUFFER_BIT);

    int maxSamples = 0;
    glGetIntegerv(GL_MAX_SAMPLES, &maxSamples);

    QByteArray extensions((const char *) glGetString(GL_EXTENSIONS));
    frameInfo.msaaSupported = maxSamples > 0
                              && extensions.contains("GL_EXT_framebuffer_multisample")
                              && extensions.contains("GL_EXT_framebuffer_blit");

    int samples;
    glGetIntegerv(GL_SAMPLES, &samples);
    frameInfo.msaaEnabled = samples > 0;

    frameInfo.fboSize = framebufferObject()->size();

    frameInfo.renderCount++;
}

void ColorRenderer::synchronize(QQuickFramebufferObject *item)
{
    FBOItem *fboItem = qobject_cast<FBOItem *>(item);
    color = fboItem->color();
    msaa = fboItem->msaa();
    if (textureSize != fboItem->textureSize()) {
        textureSize = fboItem->textureSize();
        invalidateFramebufferObject();
    }
}

QOpenGLFramebufferObject *ColorRenderer::createFramebufferObject(const QSize &size)
{
    initializeOpenGLFunctions();
    frameInfo.createFBOCount++;
    QOpenGLFramebufferObjectFormat format;
    if (msaa)
        format.setSamples(4);
    QSize s = textureSize.isValid() ? textureSize : size;
    return new QOpenGLFramebufferObject(s, format);
}

class tst_QQuickFramebufferObject: public QQmlDataTest
{
    Q_OBJECT
public:
private slots:
    void testThatStuffWorks_data();
    void testThatStuffWorks();

    void testInvalidate();
};

void tst_QQuickFramebufferObject::testThatStuffWorks_data()
{
    QTest::addColumn<uint>("color");
    QTest::addColumn<bool>("msaa");
    QTest::addColumn<QSize>("textureSize");

    QTest::newRow("red, !aa, item-size") << 0xffff0000 << false << QSize();
    QTest::newRow("green, !aa, 80x80") << 0xff00ff00 << false << QSize(80, 80);
    QTest::newRow("blue, aa, item-size") << 0xff0000ff << true << QSize();
    QTest::newRow("pink, aa, 80x80") << 0xffff00ff << true << QSize(80, 80);
}

void tst_QQuickFramebufferObject::testThatStuffWorks()
{
    QFETCH(uint, color);
    QFETCH(bool, msaa);
    QFETCH(QSize, textureSize);

#if defined(Q_OS_WIN32) && defined(QT_OPENGL_ES_2_ANGLE)
    QSKIP("QTBUG-41815");
#endif

    frameInfo.renderCount = 0;
    frameInfo.msaaEnabled = false;
    frameInfo.msaaSupported = false;
    frameInfo.fboSize = QSize();

    qmlRegisterType<FBOItem>("FBOItem", 1, 0, "FBOItem");

    QQuickView view;
    view.setSource(testFileUrl("testStuff.qml"));

    FBOItem *item = view.rootObject()->findChild<FBOItem *>("fbo");

    item->setColor(color);
    if (textureSize.isValid()) {
        item->setTextureFollowsItemSize(false);
        item->setTextureSize(textureSize);
    }
    item->setMsaa(msaa);

    view.show();
    QTest::qWaitForWindowExposed(&view);

    QImage result = view.grabWindow();

    QCOMPARE(frameInfo.renderCount, 1);
    QCOMPARE(result.pixel(0, 0), color);
    if (textureSize.isValid())
        QCOMPARE(frameInfo.fboSize, textureSize);
    else
        QCOMPARE(frameInfo.fboSize, QSize(item->width(), item->height()) * view.devicePixelRatio());
    if (frameInfo.msaaSupported && msaa)
        QVERIFY(frameInfo.msaaEnabled);

    // Resize the item and grab again
    item->setSize(QSize(200, 200));
    result = view.grabWindow();

    QCOMPARE(frameInfo.renderCount, 2);
    QCOMPARE(result.pixel(150, 150), color);
    if (textureSize.isValid())
        QCOMPARE(frameInfo.fboSize, textureSize);
    else
        QCOMPARE(frameInfo.fboSize, QSize(item->width(), item->height()) * view.devicePixelRatio());
    if (frameInfo.msaaSupported && msaa)
        QVERIFY(frameInfo.msaaEnabled);
}

void tst_QQuickFramebufferObject::testInvalidate()
{
    qmlRegisterType<FBOItem>("FBOItem", 1, 0, "FBOItem");

    QQuickView view;
    view.setSource(testFileUrl("testStuff.qml"));

    FBOItem *item = view.rootObject()->findChild<FBOItem *>("fbo");
    item->setTextureFollowsItemSize(false);
    item->setTextureSize(QSize(200, 200));

    view.show();
    QTest::qWaitForWindowExposed(&view);

    QCOMPARE(frameInfo.fboSize, QSize(200, 200));

    frameInfo.createFBOCount = 0;
    item->setTextureSize(QSize(300, 300));
    item->update();

    QTRY_COMPARE(frameInfo.createFBOCount, 1);
    QCOMPARE(frameInfo.fboSize, QSize(300, 300));
}

QTEST_MAIN(tst_QQuickFramebufferObject)

#include "tst_qquickframebufferobject.moc"
