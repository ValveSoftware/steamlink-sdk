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
#include <QtDeclarative/qdeclarativeengine.h>
#include <QtDeclarative/qdeclarativecomponent.h>
#include <QtDeclarative/qdeclarativeview.h>
#include <private/qdeclarativerectangle_p.h>
#include <private/qdeclarativeimage_p.h>
#include <private/qdeclarativeanimatedimage_p.h>
#include <QSignalSpy>
#include <QtDeclarative/qdeclarativecontext.h>

#include "../shared/testhttpserver.h"

class tst_qdeclarativeanimatedimage : public QDeclarativeDataTest
{
    Q_OBJECT
public:
    tst_qdeclarativeanimatedimage() {}

private slots:
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
    void sourceSizeReadOnly();
    void invalidSource();
    void qtbug_16520();
    void progressAndStatusChanges();

private:
    QPixmap grabScene(QGraphicsScene *scene, int width, int height);
};

QPixmap tst_qdeclarativeanimatedimage::grabScene(QGraphicsScene *scene, int width, int height)
{
    QPixmap screenshot(width, height);
    screenshot.fill();
    QPainter p_screenshot(&screenshot);
    scene->render(&p_screenshot, QRect(0, 0, width, height), QRect(0, 0, width, height));
    return screenshot;
}

void tst_qdeclarativeanimatedimage::play()
{
    QDeclarativeEngine engine;
    QDeclarativeComponent component(&engine, testFileUrl("stickman.qml"));
    QDeclarativeAnimatedImage *anim = qobject_cast<QDeclarativeAnimatedImage *>(component.create());
    QVERIFY(anim);
    QVERIFY(anim->isPlaying());

    delete anim;
}

void tst_qdeclarativeanimatedimage::pause()
{
    QDeclarativeEngine engine;
    QDeclarativeComponent component(&engine, testFileUrl("stickmanpause.qml"));
    QDeclarativeAnimatedImage *anim = qobject_cast<QDeclarativeAnimatedImage *>(component.create());
    QVERIFY(anim);
    QVERIFY(anim->isPlaying());
    QVERIFY(anim->isPaused());

    delete anim;
}

void tst_qdeclarativeanimatedimage::stopped()
{
    QDeclarativeEngine engine;
    QDeclarativeComponent component(&engine, testFileUrl("stickmanstopped.qml"));
    QDeclarativeAnimatedImage *anim = qobject_cast<QDeclarativeAnimatedImage *>(component.create());
    QVERIFY(anim);
    QVERIFY(!anim->isPlaying());
    QCOMPARE(anim->currentFrame(), 0);

    delete anim;
}

void tst_qdeclarativeanimatedimage::setFrame()
{
    QDeclarativeEngine engine;
    QDeclarativeComponent component(&engine, testFileUrl("stickmanpause.qml"));
    QDeclarativeAnimatedImage *anim = qobject_cast<QDeclarativeAnimatedImage *>(component.create());
    QVERIFY(anim);
    QVERIFY(anim->isPlaying());
    QCOMPARE(anim->currentFrame(), 2);

    delete anim;
}

void tst_qdeclarativeanimatedimage::frameCount()
{
    QDeclarativeEngine engine;
    QDeclarativeComponent component(&engine, testFileUrl("colors.qml"));
    QDeclarativeAnimatedImage *anim = qobject_cast<QDeclarativeAnimatedImage *>(component.create());
    QVERIFY(anim);
    QVERIFY(anim->isPlaying());
    QCOMPARE(anim->frameCount(), 3);

    delete anim;
}

void tst_qdeclarativeanimatedimage::mirror_running()
{
    // test where mirror is set to true after animation has started

    QDeclarativeEngine engine;
    QDeclarativeComponent component(&engine, testFileUrl("hearts.qml"));
    QDeclarativeAnimatedImage *anim = qobject_cast<QDeclarativeAnimatedImage *>(component.create());
    QVERIFY(anim);

    QGraphicsScene scene;
    int width = anim->property("width").toInt();
    int height = anim->property("height").toInt();
    scene.addItem(qobject_cast<QGraphicsObject *>(anim));

    QCOMPARE(anim->currentFrame(), 0);
    QPixmap frame0 = grabScene(&scene, width, height);
    anim->setCurrentFrame(1);
    QPixmap frame1 = grabScene(&scene, width, height);

    anim->setCurrentFrame(0);

    QSignalSpy spy(anim, SIGNAL(frameChanged()));
    anim->setPlaying(true);

    QTRY_VERIFY(spy.count() == 1); spy.clear();
    anim->setProperty("mirror", true);

    QCOMPARE(anim->currentFrame(), 1);
    QPixmap frame1_flipped = grabScene(&scene, width, height);

    QTRY_VERIFY(spy.count() == 1); spy.clear();
    QCOMPARE(anim->currentFrame(), 0);  // animation only has 2 frames, should cycle back to first
    QPixmap frame0_flipped = grabScene(&scene, width, height);

    QTransform transform;
    transform.translate(width, 0).scale(-1, 1.0);
    QPixmap frame0_expected = frame0.transformed(transform);
    QPixmap frame1_expected = frame1.transformed(transform);

    QCOMPARE(frame0_flipped, frame0_expected);
    QCOMPARE(frame1_flipped, frame1_expected);
}

void tst_qdeclarativeanimatedimage::mirror_notRunning()
{
    QFETCH(QUrl, fileUrl);

    QDeclarativeEngine engine;
    QDeclarativeComponent component(&engine, fileUrl);
    QDeclarativeAnimatedImage *anim = qobject_cast<QDeclarativeAnimatedImage *>(component.create());
    QVERIFY(anim);

    QGraphicsScene scene;
    int width = anim->property("width").toInt();
    int height = anim->property("height").toInt();
    scene.addItem(qobject_cast<QGraphicsObject *>(anim));
    QPixmap screenshot = grabScene(&scene, width, height);

    QTransform transform;
    transform.translate(width, 0).scale(-1, 1.0);
    QPixmap expected = screenshot.transformed(transform);

    int frame = anim->currentFrame();
    bool playing = anim->isPlaying();
    bool paused = anim->isPlaying();

    anim->setProperty("mirror", true);
    screenshot = grabScene(&scene, width, height);

    QCOMPARE(screenshot, expected);

    // mirroring should not change the current frame or playing status
    QCOMPARE(anim->currentFrame(), frame);
    QCOMPARE(anim->isPlaying(), playing);
    QCOMPARE(anim->isPaused(), paused);

    delete anim;
}

void tst_qdeclarativeanimatedimage::mirror_notRunning_data()
{
    QTest::addColumn<QUrl>("fileUrl");

    QTest::newRow("paused") << testFileUrl("stickmanpause.qml");
    QTest::newRow("stopped") << testFileUrl("stickmanstopped.qml");
}

void tst_qdeclarativeanimatedimage::remote()
{
    QFETCH(QString, fileName);
    QFETCH(bool, paused);

    TestHTTPServer server(14449);
    QVERIFY(server.isValid());
    server.serveDirectory(dataDirectory());

    QDeclarativeEngine engine;
    QDeclarativeComponent component(&engine, QUrl("http://127.0.0.1:14449/" + fileName));
    QTRY_VERIFY(component.isReady());

    QDeclarativeAnimatedImage *anim = qobject_cast<QDeclarativeAnimatedImage *>(component.create());
    QVERIFY(anim);

    QTRY_VERIFY(anim->isPlaying());
    if (paused) {
        QTRY_VERIFY(anim->isPaused());
        QCOMPARE(anim->currentFrame(), 2);
    }
    QVERIFY(anim->status() != QDeclarativeAnimatedImage::Error);

    delete anim;
}

void tst_qdeclarativeanimatedimage::sourceSize()
{
    QDeclarativeEngine engine;
    QDeclarativeComponent component(&engine, testFileUrl("stickmanscaled.qml"));
    QDeclarativeAnimatedImage *anim = qobject_cast<QDeclarativeAnimatedImage *>(component.create());
    QVERIFY(anim);
    QCOMPARE(anim->width(),240.0);
    QCOMPARE(anim->height(),180.0);
    QCOMPARE(anim->sourceSize(),QSize(160,120));

    delete anim;
}

void tst_qdeclarativeanimatedimage::sourceSizeReadOnly()
{
    QDeclarativeEngine engine;
    QDeclarativeComponent component(&engine, testFileUrl("stickmanerror1.qml"));
    QVERIFY(component.isError());
    QCOMPARE(component.errors().at(0).description(), QString("Invalid property assignment: \"sourceSize\" is a read-only property"));
}

void tst_qdeclarativeanimatedimage::remote_data()
{
    QTest::addColumn<QString>("fileName");
    QTest::addColumn<bool>("paused");

    QTest::newRow("playing") << "stickman.qml" << false;
    QTest::newRow("paused") << "stickmanpause.qml" << true;
}

void tst_qdeclarativeanimatedimage::invalidSource()
{
    QDeclarativeEngine engine;
    QDeclarativeComponent component(&engine);
    component.setData("import QtQuick 1.0\n AnimatedImage { source: \"no-such-file.gif\" }", QUrl::fromLocalFile("relative"));
    QVERIFY(component.isReady());

    QTest::ignoreMessage(QtWarningMsg, "file:relative:2:2: QML AnimatedImage: Error Reading Animated Image File file:no-such-file.gif");

    QDeclarativeAnimatedImage *anim = qobject_cast<QDeclarativeAnimatedImage *>(component.create());
    QVERIFY(anim);

    QVERIFY(!anim->isPlaying());
    QVERIFY(!anim->isPaused());
    QCOMPARE(anim->currentFrame(), 0);
    QCOMPARE(anim->frameCount(), 0);
    QTRY_VERIFY(anim->status() == 3);
}

void tst_qdeclarativeanimatedimage::qtbug_16520()
{
    TestHTTPServer server(14449);
    QVERIFY(server.isValid());
    server.serveDirectory(dataDirectory());

    QDeclarativeEngine engine;
    QDeclarativeComponent component(&engine, testFileUrl("qtbug-16520.qml"));
    QTRY_VERIFY(component.isReady());

    QDeclarativeRectangle *root = qobject_cast<QDeclarativeRectangle *>(component.create());
    QVERIFY(root);
    QDeclarativeAnimatedImage *anim = root->findChild<QDeclarativeAnimatedImage*>("anim");

    anim->setProperty("source", "http://127.0.0.1:14449/stickman.gif");

    QTRY_VERIFY(anim->opacity() == 0);
    QTRY_VERIFY(anim->opacity() == 1);

    delete anim;
}

void tst_qdeclarativeanimatedimage::progressAndStatusChanges()
{
    TestHTTPServer server(14449);
    QVERIFY(server.isValid());
    server.serveDirectory(dataDirectory());

    QDeclarativeEngine engine;
    QString componentStr = "import QtQuick 1.0\nAnimatedImage { source: srcImage }";
    QDeclarativeContext *ctxt = engine.rootContext();
    ctxt->setContextProperty("srcImage", testFileUrl("stickman.gif"));
    QDeclarativeComponent component(&engine);
    component.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QDeclarativeImage *obj = qobject_cast<QDeclarativeImage*>(component.create());
    QVERIFY(obj != 0);
    QVERIFY(obj->status() == QDeclarativeImage::Ready);
    QTRY_VERIFY(obj->progress() == 1.0);

    QSignalSpy sourceSpy(obj, SIGNAL(sourceChanged(const QUrl &)));
    QSignalSpy progressSpy(obj, SIGNAL(progressChanged(qreal)));
    QSignalSpy statusSpy(obj, SIGNAL(statusChanged(QDeclarativeImageBase::Status)));

    // Loading local file
    ctxt->setContextProperty("srcImage", testFileUrl("colors.gif"));
    QTRY_VERIFY(obj->status() == QDeclarativeImage::Ready);
    QTRY_VERIFY(obj->progress() == 1.0);
    QTRY_COMPARE(sourceSpy.count(), 1);
    QTRY_COMPARE(progressSpy.count(), 0);
    QTRY_COMPARE(statusSpy.count(), 0);

    // Loading remote file
    ctxt->setContextProperty("srcImage", "http://127.0.0.1:14449/stickman.gif");
    QTRY_VERIFY(obj->status() == QDeclarativeImage::Loading);
    QTRY_VERIFY(obj->progress() == 0.0);
    QTRY_VERIFY(obj->status() == QDeclarativeImage::Ready);
    QTRY_VERIFY(obj->progress() == 1.0);
    QTRY_COMPARE(sourceSpy.count(), 2);
    QTRY_VERIFY(progressSpy.count() > 1);
    QTRY_COMPARE(statusSpy.count(), 2);

    ctxt->setContextProperty("srcImage", "");
    QTRY_VERIFY(obj->status() == QDeclarativeImage::Null);
    QTRY_VERIFY(obj->progress() == 0.0);
    QTRY_COMPARE(sourceSpy.count(), 3);
    QTRY_VERIFY(progressSpy.count() > 2);
    QTRY_COMPARE(statusSpy.count(), 3);
}

QTEST_MAIN(tst_qdeclarativeanimatedimage)

#include "tst_qdeclarativeanimatedimage.moc"
