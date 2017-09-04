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
#include <QtQml/qqmlengine.h>
#include <QtQml/qqmlcomponent.h>
#include <QtQuick/qquickview.h>
#include <QtQuick/private/qquickrectangle_p.h>
#include <private/qquickimage_p.h>
#include <private/qquickanimatedimage_p.h>
#include <QSignalSpy>
#include <QtQml/qqmlcontext.h>

#include "../../shared/testhttpserver.h"
#include "../../shared/util.h"

Q_DECLARE_METATYPE(QQuickImageBase::Status)

class tst_qquickanimatedimage : public QQmlDataTest
{
    Q_OBJECT
public:
    tst_qquickanimatedimage() {}

private slots:
    void cleanup();
    void play();
    void pause();
    void stopped();
    void setFrame();
    void frameCount();
    void mirror_running();
    void mirror_notRunning();
    void mirror_notRunning_data();
    void remote();
    void remote_data();
    void sourceSize();
    void sourceSizeChanges();
    void sourceSizeChanges_intermediate();
    void sourceSizeReadOnly();
    void invalidSource();
    void qtbug_16520();
    void progressAndStatusChanges();
    void playingAndPausedChanges();
    void noCaching();
};

void tst_qquickanimatedimage::cleanup()
{
    QQuickWindow window;
    window.releaseResources();
}

void tst_qquickanimatedimage::play()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, testFileUrl("stickman.qml"));
    QQuickAnimatedImage *anim = qobject_cast<QQuickAnimatedImage *>(component.create());
    QVERIFY(anim);
    QVERIFY(anim->isPlaying());

    delete anim;
}

void tst_qquickanimatedimage::pause()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, testFileUrl("stickmanpause.qml"));
    QQuickAnimatedImage *anim = qobject_cast<QQuickAnimatedImage *>(component.create());
    QVERIFY(anim);

    QTRY_VERIFY(anim->isPaused());
    QTRY_VERIFY(anim->isPlaying());

    delete anim;
}

void tst_qquickanimatedimage::stopped()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, testFileUrl("stickmanstopped.qml"));
    QQuickAnimatedImage *anim = qobject_cast<QQuickAnimatedImage *>(component.create());
    QVERIFY(anim);
    QTRY_VERIFY(!anim->isPlaying());
    QCOMPARE(anim->currentFrame(), 0);

    delete anim;
}

void tst_qquickanimatedimage::setFrame()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, testFileUrl("stickmanpause.qml"));
    QQuickAnimatedImage *anim = qobject_cast<QQuickAnimatedImage *>(component.create());
    QVERIFY(anim);
    QVERIFY(anim->isPlaying());
    QCOMPARE(anim->currentFrame(), 2);

    delete anim;
}

void tst_qquickanimatedimage::frameCount()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, testFileUrl("colors.qml"));
    QQuickAnimatedImage *anim = qobject_cast<QQuickAnimatedImage *>(component.create());
    QVERIFY(anim);
    QVERIFY(anim->isPlaying());
    QCOMPARE(anim->frameCount(), 3);

    delete anim;
}

void tst_qquickanimatedimage::mirror_running()
{
    // test where mirror is set to true after animation has started

    QQuickView window;
    window.setSource(testFileUrl("hearts.qml"));
    window.show();
    QTest::qWaitForWindowExposed(&window);

    QQuickAnimatedImage *anim = qobject_cast<QQuickAnimatedImage *>(window.rootObject());
    QVERIFY(anim);

    int width = anim->property("width").toInt();

    QCOMPARE(anim->frameCount(), 2);

    QCOMPARE(anim->currentFrame(), 0);
    QImage frame0 = window.grabWindow();

    anim->setCurrentFrame(1);
    QCOMPARE(anim->currentFrame(), 1);
    QImage frame1 = window.grabWindow();

    anim->setCurrentFrame(0);

    QSignalSpy spy(anim, SIGNAL(frameChanged()));
    QVERIFY(spy.isValid());
    anim->setPlaying(true);

    QTRY_VERIFY(spy.count() == 1); spy.clear();
    anim->setMirror(true);

    QCOMPARE(anim->currentFrame(), 1);
    QImage frame1_flipped = window.grabWindow();

    QTRY_VERIFY(spy.count() == 1); spy.clear();
    QCOMPARE(anim->currentFrame(), 0);  // animation only has 2 frames, should cycle back to first
    QImage frame0_flipped = window.grabWindow();

    QTransform transform;
    transform.translate(width, 0).scale(-1, 1.0);
    QImage frame0_expected = frame0.transformed(transform);
    QImage frame1_expected = frame1.transformed(transform);

    if (window.devicePixelRatio() != 1.0 && window.rendererInterface()->graphicsApi() == QSGRendererInterface::Software)
        QSKIP("QTBUG-53823");
    QCOMPARE(frame0_flipped, frame0_expected);
    if (window.devicePixelRatio() != 1.0 && window.rendererInterface()->graphicsApi() == QSGRendererInterface::Software)
        QSKIP("QTBUG-53823");
    QCOMPARE(frame1_flipped, frame1_expected);

    delete anim;
}

void tst_qquickanimatedimage::mirror_notRunning()
{
    QFETCH(QUrl, fileUrl);

    QQuickView window;
    window.setSource(fileUrl);
    window.show();
    QTRY_VERIFY(window.isExposed());

    QQuickAnimatedImage *anim = qobject_cast<QQuickAnimatedImage *>(window.rootObject());
    QVERIFY(anim);

    int width = anim->property("width").toInt();
    QImage screenshot = window.grabWindow();

    QTransform transform;
    transform.translate(width, 0).scale(-1, 1.0);
    QImage expected = screenshot.transformed(transform);

    int frame = anim->currentFrame();
    bool playing = anim->isPlaying();
    bool paused = anim->isPaused();

    anim->setProperty("mirror", true);
    screenshot = window.grabWindow();

    screenshot.save("screen.png");
    if (window.devicePixelRatio() != 1.0 && window.rendererInterface()->graphicsApi() == QSGRendererInterface::Software)
        QSKIP("QTBUG-53823");
    QCOMPARE(screenshot, expected);

    // mirroring should not change the current frame or playing status
    QCOMPARE(anim->currentFrame(), frame);
    QCOMPARE(anim->isPlaying(), playing);
    QCOMPARE(anim->isPaused(), paused);

    delete anim;
}

void tst_qquickanimatedimage::mirror_notRunning_data()
{
    QTest::addColumn<QUrl>("fileUrl");

    QTest::newRow("paused") << testFileUrl("stickmanpause.qml");
    QTest::newRow("stopped") << testFileUrl("stickmanstopped.qml");
}

void tst_qquickanimatedimage::remote_data()
{
    QTest::addColumn<QString>("fileName");
    QTest::addColumn<bool>("paused");

    QTest::newRow("playing") << "stickman.qml" << false;
    QTest::newRow("paused") << "stickmanpause.qml" << true;
}

void tst_qquickanimatedimage::remote()
{
    QFETCH(QString, fileName);
    QFETCH(bool, paused);

    ThreadedTestHTTPServer server(dataDirectory());

    QQmlEngine engine;
    QQmlComponent component(&engine, server.url(fileName));
    QTRY_VERIFY(component.isReady());

    QQuickAnimatedImage *anim = qobject_cast<QQuickAnimatedImage *>(component.create());
    QVERIFY(anim);

    QTRY_VERIFY(anim->isPlaying());
    if (paused) {
        QTRY_VERIFY(anim->isPaused());
        QCOMPARE(anim->currentFrame(), 2);
    }
    QVERIFY(anim->status() != QQuickAnimatedImage::Error);

    delete anim;
}

void tst_qquickanimatedimage::sourceSize()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, testFileUrl("stickmanscaled.qml"));
    QQuickAnimatedImage *anim = qobject_cast<QQuickAnimatedImage *>(component.create());
    QVERIFY(anim);
    QCOMPARE(anim->width(),240.0);
    QCOMPARE(anim->height(),180.0);
    QCOMPARE(anim->sourceSize(),QSize(160,120));

    delete anim;
}

void tst_qquickanimatedimage::sourceSizeReadOnly()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, testFileUrl("stickmanerror1.qml"));
    QVERIFY(component.isError());
    QCOMPARE(component.errors().at(0).description(), QString("Invalid property assignment: \"sourceSize\" is a read-only property"));
}

void tst_qquickanimatedimage::invalidSource()
{
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData("import QtQuick 2.0\n AnimatedImage { source: \"no-such-file.gif\" }", QUrl::fromLocalFile("relative"));
    QVERIFY(component.isReady());

    QTest::ignoreMessage(QtWarningMsg, "file:relative:2:2: QML AnimatedImage: Error Reading Animated Image File file:no-such-file.gif");

    QQuickAnimatedImage *anim = qobject_cast<QQuickAnimatedImage *>(component.create());
    QVERIFY(anim);

    QVERIFY(anim->isPlaying());
    QVERIFY(!anim->isPaused());
    QCOMPARE(anim->currentFrame(), 0);
    QCOMPARE(anim->frameCount(), 0);
    QTRY_COMPARE(anim->status(), QQuickAnimatedImage::Error);

    delete anim;
}

void tst_qquickanimatedimage::sourceSizeChanges()
{
    TestHTTPServer server;
    QVERIFY2(server.listen(), qPrintable(server.errorString()));
    server.serveDirectory(dataDirectory());

    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData("import QtQuick 2.0\nAnimatedImage { source: srcImage }", QUrl::fromLocalFile(""));
    QTRY_VERIFY(component.isReady());
    QQmlContext *ctxt = engine.rootContext();
    ctxt->setContextProperty("srcImage", "");
    QQuickAnimatedImage *anim = qobject_cast<QQuickAnimatedImage*>(component.create());
    QVERIFY(anim != 0);

    QSignalSpy sourceSizeSpy(anim, SIGNAL(sourceSizeChanged()));

    // Local
    ctxt->setContextProperty("srcImage", QUrl(""));
    QTRY_COMPARE(anim->status(), QQuickAnimatedImage::Null);
    QTRY_COMPARE(sourceSizeSpy.count(), 0);

    ctxt->setContextProperty("srcImage", testFileUrl("hearts.gif"));
    QTRY_COMPARE(anim->status(), QQuickAnimatedImage::Ready);
    QTRY_COMPARE(sourceSizeSpy.count(), 1);

    ctxt->setContextProperty("srcImage", testFileUrl("hearts.gif"));
    QTRY_COMPARE(anim->status(), QQuickAnimatedImage::Ready);
    QTRY_COMPARE(sourceSizeSpy.count(), 1);

    ctxt->setContextProperty("srcImage", testFileUrl("hearts_copy.gif"));
    QTRY_COMPARE(anim->status(), QQuickAnimatedImage::Ready);
    QTRY_COMPARE(sourceSizeSpy.count(), 1);

    ctxt->setContextProperty("srcImage", testFileUrl("colors.gif"));
    QTRY_COMPARE(anim->status(), QQuickAnimatedImage::Ready);
    QTRY_COMPARE(sourceSizeSpy.count(), 2);

    ctxt->setContextProperty("srcImage", QUrl(""));
    QTRY_COMPARE(anim->status(), QQuickAnimatedImage::Null);
    QTRY_COMPARE(sourceSizeSpy.count(), 3);

    // Remote
    ctxt->setContextProperty("srcImage", server.url("/hearts.gif"));
    QTRY_COMPARE(anim->status(), QQuickAnimatedImage::Ready);
    QTRY_COMPARE(sourceSizeSpy.count(), 4);

    ctxt->setContextProperty("srcImage", server.url("/hearts.gif"));
    QTRY_COMPARE(anim->status(), QQuickAnimatedImage::Ready);
    QTRY_COMPARE(sourceSizeSpy.count(), 4);

    ctxt->setContextProperty("srcImage", server.url("/hearts_copy.gif"));
    QTRY_COMPARE(anim->status(), QQuickAnimatedImage::Ready);
    QTRY_COMPARE(sourceSizeSpy.count(), 4);

    ctxt->setContextProperty("srcImage", server.url("/colors.gif"));
    QTRY_COMPARE(anim->status(), QQuickAnimatedImage::Ready);
    QTRY_COMPARE(sourceSizeSpy.count(), 5);

    ctxt->setContextProperty("srcImage", QUrl(""));
    QTRY_COMPARE(anim->status(), QQuickAnimatedImage::Null);
    QTRY_COMPARE(sourceSizeSpy.count(), 6);

    delete anim;
}

void tst_qquickanimatedimage::sourceSizeChanges_intermediate()
{
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData("import QtQuick 2.0\nAnimatedImage { readonly property int testWidth: status === AnimatedImage.Ready ? sourceSize.width : -1; source: srcImage }", QUrl::fromLocalFile(""));
    QTRY_VERIFY(component.isReady());
    QQmlContext *ctxt = engine.rootContext();
    ctxt->setContextProperty("srcImage", "");

    QScopedPointer<QQuickAnimatedImage> anim(qobject_cast<QQuickAnimatedImage*>(component.create()));
    QVERIFY(anim != 0);

    ctxt->setContextProperty("srcImage", testFileUrl("hearts.gif"));
    QTRY_COMPARE(anim->status(), QQuickAnimatedImage::Ready);
    QTRY_COMPARE(anim->property("testWidth").toInt(), anim->sourceSize().width());

    ctxt->setContextProperty("srcImage", testFileUrl("hearts_copy.gif"));
    QTRY_COMPARE(anim->status(), QQuickAnimatedImage::Ready);
    QTRY_COMPARE(anim->property("testWidth").toInt(), anim->sourceSize().width());
}


void tst_qquickanimatedimage::qtbug_16520()
{
    TestHTTPServer server;
    QVERIFY2(server.listen(), qPrintable(server.errorString()));
    server.serveDirectory(dataDirectory());

    QQmlEngine engine;
    QQmlComponent component(&engine, testFileUrl("qtbug-16520.qml"));
    QTRY_VERIFY(component.isReady());

    QQuickRectangle *root = qobject_cast<QQuickRectangle *>(component.create());
    QVERIFY(root);
    QQuickAnimatedImage *anim = root->findChild<QQuickAnimatedImage*>("anim");
    QVERIFY(anim != 0);

    anim->setProperty("source", server.urlString("/stickman.gif"));
    QTRY_COMPARE(anim->opacity(), qreal(0));
    QTRY_COMPARE(anim->opacity(), qreal(1));

    delete anim;
    delete root;
}

void tst_qquickanimatedimage::progressAndStatusChanges()
{
    TestHTTPServer server;
    QVERIFY2(server.listen(), qPrintable(server.errorString()));
    server.serveDirectory(dataDirectory());

    QQmlEngine engine;
    QString componentStr = "import QtQuick 2.0\nAnimatedImage { source: srcImage }";
    QQmlContext *ctxt = engine.rootContext();
    ctxt->setContextProperty("srcImage", testFileUrl("stickman.gif"));
    QQmlComponent component(&engine);
    component.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QQuickImage *obj = qobject_cast<QQuickImage*>(component.create());
    QVERIFY(obj != 0);
    QCOMPARE(obj->status(), QQuickImage::Ready);
    QTRY_COMPARE(obj->progress(), 1.0);

    qRegisterMetaType<QQuickImageBase::Status>();
    QSignalSpy sourceSpy(obj, SIGNAL(sourceChanged(QUrl)));
    QSignalSpy progressSpy(obj, SIGNAL(progressChanged(qreal)));
    QSignalSpy statusSpy(obj, SIGNAL(statusChanged(QQuickImageBase::Status)));

    // Same image
    ctxt->setContextProperty("srcImage", testFileUrl("stickman.gif"));
    QTRY_COMPARE(obj->status(), QQuickImage::Ready);
    QTRY_COMPARE(obj->progress(), 1.0);
    QTRY_COMPARE(sourceSpy.count(), 0);
    QTRY_COMPARE(progressSpy.count(), 0);
    QTRY_COMPARE(statusSpy.count(), 0);

    // Loading local file
    ctxt->setContextProperty("srcImage", testFileUrl("colors.gif"));
    QTRY_COMPARE(obj->status(), QQuickImage::Ready);
    QTRY_COMPARE(obj->progress(), 1.0);
    QTRY_COMPARE(sourceSpy.count(), 1);
    QTRY_COMPARE(progressSpy.count(), 0);
    QTRY_COMPARE(statusSpy.count(), 1);

    // Loading remote file
    ctxt->setContextProperty("srcImage", server.url("/stickman.gif"));
    QTRY_COMPARE(obj->status(), QQuickImage::Loading);
    QTRY_COMPARE(obj->progress(), 0.0);
    QTRY_COMPARE(obj->status(), QQuickImage::Ready);
    QTRY_COMPARE(obj->progress(), 1.0);
    QTRY_COMPARE(sourceSpy.count(), 2);
    QTRY_VERIFY(progressSpy.count() > 1);
    QTRY_COMPARE(statusSpy.count(), 3);

    ctxt->setContextProperty("srcImage", "");
    QTRY_COMPARE(obj->status(), QQuickImage::Null);
    QTRY_COMPARE(obj->progress(), 0.0);
    QTRY_COMPARE(sourceSpy.count(), 3);
    QTRY_VERIFY(progressSpy.count() > 2);
    QTRY_COMPARE(statusSpy.count(), 4);

    delete obj;
}

void tst_qquickanimatedimage::playingAndPausedChanges()
{
    QQmlEngine engine;
    QString componentStr = "import QtQuick 2.0\nAnimatedImage { source: srcImage }";
    QQmlContext *ctxt = engine.rootContext();
    ctxt->setContextProperty("srcImage", QUrl(""));
    QQmlComponent component(&engine);
    component.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QQuickAnimatedImage *obj = qobject_cast<QQuickAnimatedImage*>(component.create());
    QVERIFY(obj != 0);
    QCOMPARE(obj->status(), QQuickAnimatedImage::Null);
    QTRY_VERIFY(obj->isPlaying());
    QTRY_VERIFY(!obj->isPaused());
    QSignalSpy playingSpy(obj, SIGNAL(playingChanged()));
    QSignalSpy pausedSpy(obj, SIGNAL(pausedChanged()));

    // initial state
    obj->setProperty("playing", true);
    obj->setProperty("paused", false);
    QTRY_VERIFY(obj->isPlaying());
    QTRY_VERIFY(!obj->isPaused());
    QTRY_COMPARE(playingSpy.count(), 0);
    QTRY_COMPARE(pausedSpy.count(), 0);

    obj->setProperty("playing", false);
    obj->setProperty("paused", true);
    QTRY_VERIFY(!obj->isPlaying());
    QTRY_VERIFY(obj->isPaused());
    QTRY_COMPARE(playingSpy.count(), 1);
    QTRY_COMPARE(pausedSpy.count(), 1);

    obj->setProperty("playing", true);
    obj->setProperty("paused", false);
    QTRY_VERIFY(obj->isPlaying());
    QTRY_VERIFY(!obj->isPaused());
    QTRY_COMPARE(playingSpy.count(), 2);
    QTRY_COMPARE(pausedSpy.count(), 2);

    ctxt->setContextProperty("srcImage", testFileUrl("stickman.gif"));
    QTRY_VERIFY(obj->isPlaying());
    QTRY_VERIFY(!obj->isPaused());
    QTRY_COMPARE(playingSpy.count(), 2);
    QTRY_COMPARE(pausedSpy.count(), 2);

    obj->setProperty("paused", true);
    QTRY_VERIFY(obj->isPlaying());
    QTRY_VERIFY(obj->isPaused());
    QTRY_COMPARE(playingSpy.count(), 2);
    QTRY_COMPARE(pausedSpy.count(), 3);

    obj->setProperty("playing", false);
    QTRY_VERIFY(!obj->isPlaying());
    QTRY_VERIFY(!obj->isPaused());
    QTRY_COMPARE(playingSpy.count(), 3);
    QTRY_COMPARE(pausedSpy.count(), 4);

    obj->setProperty("playing", true);

    // Cannot animate this image, playing will be false
    ctxt->setContextProperty("srcImage", testFileUrl("green.png"));
    QTRY_VERIFY(!obj->isPlaying());
    QTRY_VERIFY(!obj->isPaused());
    QTRY_COMPARE(playingSpy.count(), 5);
    QTRY_COMPARE(pausedSpy.count(), 4);

    delete obj;
}

void tst_qquickanimatedimage::noCaching()
{
    QQuickView window, window_nocache;
    window.setSource(testFileUrl("colors.qml"));
    window_nocache.setSource(testFileUrl("colors_nocache.qml"));
    window.show();
    window_nocache.show();
    QTest::qWaitForWindowExposed(&window);
    QTest::qWaitForWindowExposed(&window_nocache);

    QQuickAnimatedImage *anim = qobject_cast<QQuickAnimatedImage *>(window.rootObject());
    QVERIFY(anim);

    QQuickAnimatedImage *anim_nocache = qobject_cast<QQuickAnimatedImage *>(window_nocache.rootObject());
    QVERIFY(anim_nocache);

    QCOMPARE(anim->frameCount(), anim_nocache->frameCount());

    // colors.gif only has 3 frames so this should be fast
    for (int loops = 0; loops <= 2; ++loops) {
        for (int frame = 0; frame < anim->frameCount(); ++frame) {
            anim->setCurrentFrame(frame);
            anim_nocache->setCurrentFrame(frame);

            QImage image_cache = window.grabWindow();
            QImage image_nocache = window_nocache.grabWindow();

            QCOMPARE(image_cache, image_nocache);
        }
    }
}

QTEST_MAIN(tst_qquickanimatedimage)

#include "tst_qquickanimatedimage.moc"
