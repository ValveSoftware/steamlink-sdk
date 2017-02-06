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

#ifndef QT_NO_OPENGL
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#endif

#include <QtQuick>
#include <QtQml>

#ifndef QT_NO_OPENGL
#include <private/qopenglcontext_p.h>
#endif

#include <private/qsgcontext_p.h>
#include <private/qsgrenderloop_p.h>

#include "../../shared/util.h"
#include "../shared/visualtestutil.h"

using namespace QQuickVisualTestUtil;

class PerPixelRect : public QQuickItem
{
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
    Q_OBJECT
public:
    PerPixelRect() {
        setFlag(ItemHasContents);
    }

    void setColor(const QColor &c) {
        if (c == m_color)
            return;
        m_color = c;
        emit colorChanged(c);
    }

    QColor color() const { return m_color; }

    QSGNode *updatePaintNode(QSGNode *old, UpdatePaintNodeData *)
    {
        if (old)
            delete old;

        QSGNode *node = new QSGNode();

        for (int y=0; y<height(); ++y) {
            for (int x=0; x<width(); ++x) {
                QSGSimpleRectNode *rn = new QSGSimpleRectNode();
                rn->setRect(x, y, 1, 1);
                rn->setColor(m_color);
                node->appendChildNode(rn);
            }
        }

        return node;
    }

Q_SIGNALS:
    void colorChanged(const QColor &c   );

private:
    QColor m_color;
};

class tst_SceneGraph : public QQmlDataTest
{
    Q_OBJECT

private slots:
    void initTestCase();

    void manyWindows_data();
    void manyWindows();

    void render_data();
    void render();
#ifndef QT_NO_OPENGL
    void hideWithOtherContext();
#endif
    void createTextureFromImage_data();
    void createTextureFromImage();

private:
    bool m_brokenMipmapSupport;
    QQuickView *createView(const QString &file, QWindow *parent = 0, int x = -1, int y = -1, int w = -1, int h = -1);
};

template <typename T> class ScopedList : public QList<T> {
public:
    ~ScopedList() { qDeleteAll(*this); }
};

void tst_SceneGraph::initTestCase()
{
    qmlRegisterType<PerPixelRect>("SceneGraphTest", 1, 0, "PerPixelRect");

    QQmlDataTest::initTestCase();

    QSGRenderLoop *loop = QSGRenderLoop::instance();
    qDebug() << "RenderLoop:        " << loop;

#ifndef QT_NO_OPENGL
    QOpenGLContext context;
    context.setFormat(loop->sceneGraphContext()->defaultSurfaceFormat());
    context.create();
    QSurfaceFormat format = context.format();

    QOffscreenSurface surface;
    surface.setFormat(format);
    surface.create();
    if (!context.makeCurrent(&surface))
        qFatal("Failed to create a GL context...");

    QOpenGLFunctions *funcs = QOpenGLContext::currentContext()->functions();
    qDebug() << "R/G/B/A Buffers:   " << format.redBufferSize() << format.greenBufferSize() << format.blueBufferSize() << format.alphaBufferSize();
    qDebug() << "Depth Buffer:      " << format.depthBufferSize();
    qDebug() << "Stencil Buffer:    " << format.stencilBufferSize();
    qDebug() << "Samples:           " << format.samples();
    int textureSize;
    funcs->glGetIntegerv(GL_MAX_TEXTURE_SIZE, &textureSize);
    qDebug() << "Max Texture Size:  " << textureSize;
    qDebug() << "GL_VENDOR:         " << (const char *) funcs->glGetString(GL_VENDOR);
    qDebug() << "GL_RENDERER:       " << (const char *) funcs->glGetString(GL_RENDERER);
    QByteArray version = (const char *) funcs->glGetString(GL_VERSION);
    qDebug() << "GL_VERSION:        " << version.constData();
    QSet<QByteArray> exts = context.extensions();
    QByteArray all;
    foreach (const QByteArray &e, exts) all += ' ' + e;
    qDebug() << "GL_EXTENSIONS:    " << all.constData();

    m_brokenMipmapSupport = version.contains("Mesa 10.1") || version.contains("Mesa 9.");
    qDebug() << "Broken Mipmap:    " << m_brokenMipmapSupport;

    context.doneCurrent();
#endif
}

QQuickView *tst_SceneGraph::createView(const QString &file, QWindow *parent, int x, int y, int w, int h)
{
    QQuickView *view = new QQuickView(parent);
    view->setSource(testFileUrl(file));
    if (x >= 0 && y >= 0) view->setPosition(x, y);
    if (w >= 0 && h >= 0) view->resize(w, h);
    view->show();
    return view;
}

// Assumes the images are opaque white...
bool containsSomethingOtherThanWhite(const QImage &image)
{
    QImage img;
    if (image.format() != QImage::Format_ARGB32_Premultiplied
             || image.format() != QImage::Format_RGB32)
        img = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    else
        img = image;

    int w = img.width();
    int h = img.height();
    for (int y=0; y<h; ++y) {
        const uint *pixels = (const uint *) img.constScanLine(y);
        for (int x=0; x<w; ++x)
            if (pixels[x] != 0xffffffff)
                return true;
    }
    return false;
}

void tst_SceneGraph::manyWindows_data()
{
    QTest::addColumn<QString>("file");
    QTest::addColumn<bool>("toplevel");
    QTest::addColumn<bool>("shared");

    QTest::newRow("image,toplevel") << QStringLiteral("manyWindows_image.qml") << true << false;
    QTest::newRow("image,subwindow") << QStringLiteral("manyWindows_image.qml") << false << false;
    QTest::newRow("dftext,toplevel") << QStringLiteral("manyWindows_dftext.qml") << true << false;
    QTest::newRow("dftext,subwindow") << QStringLiteral("manyWindows_dftext.qml") << false << false;
    QTest::newRow("ntext,toplevel") << QStringLiteral("manyWindows_ntext.qml") << true << false;
    QTest::newRow("ntext,subwindow") << QStringLiteral("manyWindows_ntext.qml") << false << false;
    QTest::newRow("rects,toplevel") << QStringLiteral("manyWindows_rects.qml") << true << false;
    QTest::newRow("rects,subwindow") << QStringLiteral("manyWindows_rects.qml") << false << false;

    QTest::newRow("image,toplevel,sharing") << QStringLiteral("manyWindows_image.qml") << true << true;
    QTest::newRow("image,subwindow,sharing") << QStringLiteral("manyWindows_image.qml") << false << true;
    QTest::newRow("dftext,toplevel,sharing") << QStringLiteral("manyWindows_dftext.qml") << true << true;
    QTest::newRow("dftext,subwindow,sharing") << QStringLiteral("manyWindows_dftext.qml") << false << true;
    QTest::newRow("ntext,toplevel,sharing") << QStringLiteral("manyWindows_ntext.qml") << true << true;
    QTest::newRow("ntext,subwindow,sharing") << QStringLiteral("manyWindows_ntext.qml") << false << true;
    QTest::newRow("rects,toplevel,sharing") << QStringLiteral("manyWindows_rects.qml") << true << true;
    QTest::newRow("rects,subwindow,sharing") << QStringLiteral("manyWindows_rects.qml") << false << true;
}

#ifndef QT_NO_OPENGL
struct ShareContextResetter {
public:
    ~ShareContextResetter() { qt_gl_set_global_share_context(0); }
};
#endif

void tst_SceneGraph::manyWindows()
{
    QFETCH(QString, file);
    QFETCH(bool, toplevel);
    QFETCH(bool, shared);
#ifndef QT_NO_OPENGL
    QOpenGLContext sharedGLContext;
    ShareContextResetter cleanup; // To avoid dangling pointer in case of test-failure.
    if (shared) {
        QVERIFY(sharedGLContext.create());
        qt_gl_set_global_share_context(&sharedGLContext);
    }
#endif
    QScopedPointer<QWindow> parent;
    if (!toplevel) {
        parent.reset(new QWindow());
        parent->resize(200, 200);
        parent->show();
    }

    ScopedList <QQuickView *> views;

    const int COUNT = 4;

    QImage baseLine;
    for (int i=0; i<COUNT; ++i) {
        views << createView(file, parent.data(), (i % 2) * 100, (i / 2) * 100, 100, 100);
    }
    for (int i=0; i<COUNT; ++i) {
        QQuickView *view = views.at(i);
        QTest::qWaitForWindowExposed(view);
        QImage content = view->grabWindow();
        if (i == 0) {
            baseLine = content;
            QVERIFY(containsSomethingOtherThanWhite(baseLine));
        } else {
            QVERIFY(compareImages(content, baseLine));
        }
    }

    // Wipe and recreate one (scope pointer delets it...)
    delete views.takeLast();
    QQuickView *last = createView(file, parent.data(), 100, 100, 100, 100);
    QTest::qWaitForWindowExposed(last);
    views << last;
    QVERIFY(compareImages(baseLine, last->grabWindow()));

    // Wipe and recreate all
    qDeleteAll(views);
    views.clear();

    for (int i=0; i<COUNT; ++i) {
        views << createView(file, parent.data(), (i % 2) * 100, (i / 2) * 100, 100, 100);
    }
    for (int i=0; i<COUNT; ++i) {
        QQuickView *view = views.at(i);
        QTest::qWaitForWindowExposed(view);
        QImage content = view->grabWindow();
        QVERIFY(compareImages(content, baseLine));
    }
}

struct Sample {
    Sample(int xx, int yy, qreal rr, qreal gg, qreal bb, qreal errorMargin = 0.05)
        : x(xx)
        , y(yy)
        , r(rr)
        , g(gg)
        , b(bb)
        , tolerance(errorMargin)
    {
    }
    Sample(const Sample &o) : x(o.x), y(o.y), r(o.r), g(o.g), b(o.b), tolerance(o.tolerance) { }
    Sample() : x(0), y(0), r(0), g(0), b(0), tolerance(0) { }

    QString toString(const QImage &image) const {
        QColor color(image.pixel(x,y));
        return QString::fromLatin1("pixel(%1,%2), rgb(%3,%4,%5), tolerance=%6 -- image(%7,%8,%9)")
                .arg(x).arg(y)
                .arg(r).arg(g).arg(b)
                .arg(tolerance)
                .arg(color.redF()).arg(color.greenF()).arg(color.blueF());
    }

    bool check(const QImage &image, qreal scale) {
        QColor color(image.pixel(x * scale, y * scale));
        return qAbs(color.redF() - r) <= tolerance
                && qAbs(color.greenF() - g) <= tolerance
                && qAbs(color.blueF() - b) <= tolerance;
    }


    int x, y;
    qreal r, g, b;
    qreal tolerance;
};

static Sample sample_from_regexp(QRegExp *re) {
    return Sample(re->cap(1).toInt(),
                  re->cap(2).toInt(),
                  re->cap(3).toFloat(),
                  re->cap(4).toFloat(),
                  re->cap(5).toFloat(),
                  re->cap(6).toFloat()
                  );
}

Q_DECLARE_METATYPE(Sample);

/*
  The render() test implements a small test framework for itself with
  the purpose of testing odds and ends of the scene graph
  rendering. Each .qml file can consist of one or two stages. The
  first stage is when the file is first displayed. The content is
  grabbed and matched against 'base' samples defined in the .qml file
  itself.  The samples contain a pixel position, and RGB value and a
  margin of error. The samples are defined in the .qml file so it is
  easy to make the connection between colors and positions on the screen.

  If the base stage samples all succeed, the test emits
  'enterFinalStage' on the root item and waits for the .qml file to
  update the value of 'finalStageComplete' The test can set this
  directly or run an animation and set it later. Once the
  'finalStageComplete' variable is true, we grab and match against the
  second set of samples 'final'

  The samples in the .qml file are defined in comments on the format:
      #base: x y r g b error-tolerance
      #final: x y r g b error-tolerance
      - x and y are integers
      - r g b are floats in the range of 0.0-1.0
      - error-tolerance is a float in the range of 0.0-1.0

  We also include a
      #samples: count
  to sanity check that all base/final samples were matched correctly
  as the matching regexp is a bit crude.

  To add new tests, add them to the 'files' list and put #base,
  #final, #samples tags into the .qml file
*/

void tst_SceneGraph::render_data()
{
    QTest::addColumn<QString>("file");
    QTest::addColumn<QList<Sample> >("baseStage");
    QTest::addColumn<QList<Sample> >("finalStage");

    QList<QString> files;
    files << "render_DrawSets.qml"
          << "render_Overlap.qml"
          << "render_MovingOverlap.qml"
          << "render_BreakOpacityBatch.qml"
          << "render_OutOfFloatRange.qml"
          << "render_StackingOrder.qml"
          << "render_ImageFiltering.qml"
          << "render_bug37422.qml"
          << "render_OpacityThroughBatchRoot.qml";
    if (!m_brokenMipmapSupport)
          files << "render_Mipmap.qml";

    QRegExp sampleCount("#samples: *(\\d+)");
    //                          X:int   Y:int   R:float       G:float       B:float       Error:float
    QRegExp baseSamples("#base: *(\\d+) *(\\d+) *(\\d\\.\\d+) *(\\d\\.\\d+) *(\\d\\.\\d+) *(\\d\\.\\d+)");
    QRegExp finalSamples("#final: *(\\d+) *(\\d+) *(\\d\\.\\d+) *(\\d\\.\\d+) *(\\d\\.\\d+) *(\\d\\.\\d+)");

    foreach (QString fileName, files) {
        QFile file(testFile(fileName));
        if (!file.open(QFile::ReadOnly)) {
            qFatal("render_data: QFile::open failed! file=%s, error=%s",
                   qPrintable(fileName), qPrintable(file.errorString()));
        }
        QStringList contents = QString::fromLatin1(file.readAll()).split(QLatin1Char('\n'));

        int samples = -1;
        foreach (QString line, contents) {
            if (sampleCount.indexIn(line) >= 0) {
                samples = sampleCount.cap(1).toInt();
                break;
            }
        }
        if (samples == -1)
            qFatal("render_data: failed to find string '#samples: [count], file=%s", qPrintable(fileName));

        QList<Sample> baseStage, finalStage;
        foreach (QString line, contents) {
            if (baseSamples.indexIn(line) >= 0)
                baseStage << sample_from_regexp(&baseSamples);
            else if (finalSamples.indexIn(line) >= 0)
                finalStage << sample_from_regexp(&finalSamples);
        }

        if (baseStage.size() + finalStage.size() != samples)
            qFatal("render_data: #samples does not add up to number of counted samples, file=%s", qPrintable(fileName));

        QTest::newRow(qPrintable(fileName)) << fileName << baseStage << finalStage;
    }
}

void tst_SceneGraph::render()
{
    QQuickView dummy;
    dummy.show();
    QTest::qWaitForWindowExposed(&dummy);
    if (dummy.rendererInterface()->graphicsApi() != QSGRendererInterface::OpenGL)
        QSKIP("Skipping complex rendering tests due to not running with OpenGL");
    dummy.hide();

    QFETCH(QString, file);
    QFETCH(QList<Sample>, baseStage);
    QFETCH(QList<Sample>, finalStage);

    QObject suite;
    suite.setObjectName("The Suite");

    QQuickView view;
    view.rootContext()->setContextProperty("suite", &suite);
    view.setSource(testFileUrl(file));
    view.setResizeMode(QQuickView::SizeViewToRootObject);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));

    // We're checking actual pixels, so scale up the sample point to the top-left of the
    // 2x2 pixel block and hope that this is good enough. Ideally, view and content
    // would be in identical coordinate space, meaning pixels, but we're not in an
    // ideal world.
    // Just keep this in mind when writing tests.
    qreal scale = view.devicePixelRatio();

    // Grab the window and check all our base stage samples
    QImage content = view.grabWindow();
    for (int i=0; i<baseStage.size(); ++i) {
        Sample sample = baseStage.at(i);
        QVERIFY2(sample.check(content, scale), qPrintable(sample.toString(content)));
    }

    // Put the qml file into the final stage and wait for it to
    // complete it.
    QQuickItem *rootItem = view.rootObject();
    QMetaObject::invokeMethod(rootItem, "enterFinalStage");
    QTRY_VERIFY(rootItem->property("finalStageComplete").toBool());

    // The grab the results and verify the samples in the end state.
    content = view.grabWindow();
    for (int i=0; i<finalStage.size(); ++i) {
        Sample sample = finalStage.at(i);
        QVERIFY2(sample.check(content, scale), qPrintable(sample.toString(content)));
    }
}

#ifndef QT_NO_OPENGL
// Testcase for QTBUG-34898. We make another context current on another surface
// in the GUI thread and hide the QQuickWindow while the other context is
// current on the other window.
void tst_SceneGraph::hideWithOtherContext()
{
    QWindow window;
    window.setSurfaceType(QWindow::OpenGLSurface);
    window.resize(100, 100);
    window.create();
    QOpenGLContext context;
    QVERIFY(context.create());
    bool renderingOnMainThread = false;

    {
        QQuickView view;
        view.setSource(testFileUrl("simple.qml"));
        view.setResizeMode(QQuickView::SizeViewToRootObject);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));

        if (view.rendererInterface()->graphicsApi() != QSGRendererInterface::OpenGL)
            QSKIP("Skipping OpenGL context test due to not running with OpenGL");

        renderingOnMainThread = view.openglContext()->thread() == QGuiApplication::instance()->thread();

        // Make the local context current on the local window...
        context.makeCurrent(&window);
    }

    // The local context should no longer be the current one. It is not
    // rebound because all well behaving Qt/OpenGL applications are
    // required to makeCurrent their context again before making any
    // GL calls to a new frame (see QOpenGLContext docs).
    QVERIFY(!renderingOnMainThread || QOpenGLContext::currentContext() != &context);
}
#endif

void tst_SceneGraph::createTextureFromImage_data()
{
    QImage rgba(64, 64, QImage::Format_ARGB32_Premultiplied);
    QImage rgb(64, 64, QImage::Format_RGB32);

    QTest::addColumn<QImage>("image");
    QTest::addColumn<uint>("flags");
    QTest::addColumn<bool>("expectedAlpha");

    QTest::newRow("rgb") << rgb << uint(0) << false;
    QTest::newRow("argb") << rgba << uint(0) << true;
    QTest::newRow("rgb,alpha") << rgb << uint(QQuickWindow::TextureHasAlphaChannel) << false;
    QTest::newRow("argb,alpha") << rgba << uint(QQuickWindow::TextureHasAlphaChannel) << true;
    QTest::newRow("rgb,!alpha") << rgb << uint(QQuickWindow::TextureIsOpaque) << false;
    QTest::newRow("argb,!alpha") << rgba << uint(QQuickWindow::TextureIsOpaque) << false;
}

void tst_SceneGraph::createTextureFromImage()
{
    QFETCH(QImage, image);
    QFETCH(uint, flags);
    QFETCH(bool, expectedAlpha);

    QQuickView view;
    view.show();
    QTest::qWaitForWindowExposed(&view);
    QTRY_VERIFY(view.isSceneGraphInitialized());

    QScopedPointer<QSGTexture> texture(view.createTextureFromImage(image, (QQuickWindow::CreateTextureOptions) flags));
    QCOMPARE(texture->hasAlphaChannel(), expectedAlpha);
}


#include "tst_scenegraph.moc"

QTEST_MAIN(tst_SceneGraph)

