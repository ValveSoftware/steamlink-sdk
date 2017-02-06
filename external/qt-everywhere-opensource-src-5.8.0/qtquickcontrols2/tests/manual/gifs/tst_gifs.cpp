/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QtTest>
#include <QtQuick>

#include "gifrecorder.h"
#include "eventcapturer.h"

//#define GENERATE_EVENT_CODE

class tst_Gifs : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void tumblerWrap();
    void slider();
    void sliderSnap_data();
    void sliderSnap();
    void rangeSlider();
    void busyIndicator();
    void switchGif();
    void button_data();
    void button();
    void tabBar();
    void menu();
    void swipeView();
    void swipeDelegate_data();
    void swipeDelegate();
    void swipeDelegateBehind();
    void delegates_data();
    void delegates();
    void dial_data();
    void dial();
    void scrollBar();
    void scrollIndicator();
    void progressBar_data();
    void progressBar();
    void triState_data();
    void triState();
    void checkables_data();
    void checkables();
    void comboBox();
    void stackView_data();
    void stackView();
    void drawer();

private:
    void moveSmoothly(QQuickWindow *window, const QPoint &from, const QPoint &to, int movements,
        QEasingCurve::Type easingCurveType = QEasingCurve::OutQuint, int movementDelay = 15);
    void moveSmoothlyAlongArc(QQuickWindow *window, QPoint arcCenter, qreal distanceFromCenter,
        qreal startAngleRadians, qreal endAngleRadians, QEasingCurve::Type easingCurveType = QEasingCurve::OutQuint);

    QString dataDirPath;
    QDir outputDir;
};

void tst_Gifs::initTestCase()
{
    dataDirPath = QFINDTESTDATA("data");
    QVERIFY(!dataDirPath.isEmpty());
    qInfo() << "data directory:" << dataDirPath;

    outputDir = QDir(QDir::current().filePath("gifs"));
    QVERIFY(outputDir.exists() || QDir::current().mkpath("gifs"));
    qInfo() << "output directory:" << outputDir.absolutePath();
}

void tst_Gifs::moveSmoothly(QQuickWindow *window, const QPoint &from, const QPoint &to,
    int movements, QEasingCurve::Type easingCurveType, int movementDelay)
{
    QEasingCurve curve(easingCurveType);
    int xDifference = to.x() - from.x();
    int yDifference = to.y() - from.y();
    for (int movement = 0; movement < movements; ++movement) {
        QPoint pos = QPoint(
            from.x() + qRound(curve.valueForProgress(movement / qreal(qAbs(xDifference))) * xDifference),
            from.y() + qRound(curve.valueForProgress(movement / qreal(qAbs(yDifference))) * yDifference));
        QTest::mouseMove(window, pos, movementDelay);
    }
}

QPoint posAlongArc(QPoint arcCenter, qreal startAngleRadians, qreal endAngleRadians,
    qreal distanceFromCenter, qreal progress, QEasingCurve::Type easingCurveType)
{
    QEasingCurve curve(easingCurveType);
    const qreal angle = startAngleRadians + curve.valueForProgress(progress) * (endAngleRadians - startAngleRadians);
    return (arcCenter - QTransform().rotateRadians(angle).map(QPointF(0, distanceFromCenter))).toPoint();
}

void tst_Gifs::moveSmoothlyAlongArc(QQuickWindow *window, QPoint arcCenter, qreal distanceFromCenter,
    qreal startAngleRadians, qreal endAngleRadians, QEasingCurve::Type easingCurveType)
{
    QEasingCurve curve(easingCurveType);
    const qreal angleSpan = endAngleRadians - startAngleRadians;
    const int movements = qAbs(angleSpan) * 20 + 20;

    for (int movement = 0; movement < movements; ++movement) {
        const qreal progress = movement / qreal(movements);
        const QPoint pos = posAlongArc(arcCenter, startAngleRadians, endAngleRadians,
            distanceFromCenter, progress, easingCurveType);
        QTest::mouseMove(window, pos, 15);
    }
}

void tst_Gifs::tumblerWrap()
{
    GifRecorder gifRecorder;
    gifRecorder.setDataDirPath(dataDirPath);
    gifRecorder.setOutputDir(outputDir);
    gifRecorder.setRecordingDuration(5);
    gifRecorder.setQmlFileName("qtquickcontrols2-tumbler-wrap.qml");

    gifRecorder.start();

    // Left as an example. Usually EventCapturer code would be removed after
    // the GIF has been generated.
    QQuickWindow *window = gifRecorder.window();
    EventCapturer eventCapturer;
#ifdef GENERATE_EVENT_CODE
    eventCapturer.setMoveEventTrimFlags(EventCapturer::TrimAll);
    eventCapturer.startCapturing(window, 4000);
#else
    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, QPoint(89, 75), 326);
    QTest::mouseMove(window, QPoint(89, 76), 31);
    QTest::mouseMove(window, QPoint(89, 80), 10);
    QTest::mouseMove(window, QPoint(93, 93), 10);
    QTest::mouseMove(window, QPoint(95, 101), 10);
    QTest::mouseMove(window, QPoint(97, 109), 11);
    QTest::mouseMove(window, QPoint(101, 125), 10);
    QTest::mouseMove(window, QPoint(103, 133), 11);
    QTest::mouseMove(window, QPoint(103, 141), 11);
    QTest::mouseMove(window, QPoint(105, 158), 10);
    QTest::mouseMove(window, QPoint(105, 162), 13);
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, QPoint(105, 162), 0);
    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, QPoint(154, 100), 1098);
    QTest::mouseMove(window, QPoint(154, 99), 16);
    QTest::mouseMove(window, QPoint(153, 98), 16);
    QTest::mouseMove(window, QPoint(153, 95), 16);
    QTest::mouseMove(window, QPoint(152, 91), 15);
    QTest::mouseMove(window, QPoint(152, 87), 14);
    QTest::mouseMove(window, QPoint(151, 83), 13);
    QTest::mouseMove(window, QPoint(151, 86), 13);
    QTest::mouseMove(window, QPoint(150, 79), 12);
    QTest::mouseMove(window, QPoint(148, 73), 12);
    QTest::mouseMove(window, QPoint(148, 68), 12);
    QTest::mouseMove(window, QPoint(148, 60), 10);
    QTest::mouseMove(window, QPoint(147, 50), 10);
    QTest::mouseMove(window, QPoint(147, 40), 9);
    QTest::mouseMove(window, QPoint(147, 30), 8);
    QTest::mouseMove(window, QPoint(147, 20), 7);
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, QPoint(147, 20), 0);
    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, QPoint(154, 100), 1000);
    QTest::mouseMove(window, QPoint(147, 101), 16);
    QTest::mouseMove(window, QPoint(147, 102), 16);
    QTest::mouseMove(window, QPoint(147, 105), 16);
    QTest::mouseMove(window, QPoint(148, 109), 15);
    QTest::mouseMove(window, QPoint(148, 115), 14);
    QTest::mouseMove(window, QPoint(148, 120), 13);
    QTest::mouseMove(window, QPoint(150, 125), 13);
    QTest::mouseMove(window, QPoint(151, 130), 12);
    QTest::mouseMove(window, QPoint(151, 135), 12);
    QTest::mouseMove(window, QPoint(153, 140), 12);
    QTest::mouseMove(window, QPoint(153, 150), 10);
    QTest::mouseMove(window, QPoint(153, 160), 10);
    QTest::mouseMove(window, QPoint(153, 170), 9);
    QTest::mouseMove(window, QPoint(155, 180), 8);
    QTest::mouseMove(window, QPoint(155, 188), 7);
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, QPoint(20, 188), 0);
#endif

    gifRecorder.waitForFinish();

    const auto capturedEvents = eventCapturer.capturedEvents();
    for (CapturedEvent event : capturedEvents)
        qDebug().noquote() << event.cppCommand();
}

void tst_Gifs::slider()
{
    GifRecorder gifRecorder;
    gifRecorder.setDataDirPath(dataDirPath);
    gifRecorder.setOutputDir(outputDir);
    gifRecorder.setRecordingDuration(5);
    gifRecorder.setHighQuality(true);
    gifRecorder.setQmlFileName("qtquickcontrols2-slider.qml");

    gifRecorder.start();

    QQuickWindow *window = gifRecorder.window();
    QQuickItem *slider = window->property("slider").value<QQuickItem*>();
    QVERIFY(slider);
    QQuickItem *handle = slider->property("handle").value<QQuickItem*>();
    QVERIFY(handle);

    const QPoint handleCenter = handle->mapToItem(window->contentItem(),
        QPoint(handle->width() / 2, handle->height() / 2)).toPoint();

    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, handleCenter, 100);
    QPoint pos1 = handleCenter + QPoint(slider->width() * 0.3, 0);
    moveSmoothly(window, handleCenter, pos1, pos1.x() - handleCenter.x(), QEasingCurve::OutQuint, 10);
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, pos1, 20);

    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, handleCenter, 100);
    const QPoint pos2 = QPoint(slider->width() - handleCenter.x() + slider->property("rightPadding").toInt(), handleCenter.y());
    moveSmoothly(window, pos1, pos2, pos2.x() - pos1.x(), QEasingCurve::OutQuint, 10);
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, pos2, 20);

    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, pos2, 100);
    moveSmoothly(window, pos2, handleCenter, qAbs(handleCenter.x() - pos2.x()), QEasingCurve::OutQuint, 10);
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, handleCenter, 20);

    gifRecorder.waitForFinish();
}

void tst_Gifs::sliderSnap_data()
{
    QTest::addColumn<QString>("gifBaseName");
    QTest::addColumn<int>("snapMode");
    QTest::newRow("NoSnap") << "qtquickcontrols2-slider-nosnap" << 0;
    QTest::newRow("SnapAlways") << "qtquickcontrols2-slider-snapalways" << 1;
    QTest::newRow("SnapOnRelease") << "qtquickcontrols2-slider-snaponrelease" << 2;
}

void tst_Gifs::sliderSnap()
{
    QFETCH(QString, gifBaseName);
    QFETCH(int, snapMode);

    GifRecorder gifRecorder;
    gifRecorder.setDataDirPath(dataDirPath);
    gifRecorder.setOutputDir(outputDir);
    gifRecorder.setRecordingDuration(8);
    gifRecorder.setHighQuality(true);
    gifRecorder.setQmlFileName("qtquickcontrols2-slider-snap.qml");
    gifRecorder.setOutputFileBaseName(gifBaseName);

    gifRecorder.start();

    QQuickWindow *window = gifRecorder.window();
    QQuickItem *slider = window->property("slider").value<QQuickItem*>();
    QVERIFY(slider);
    QVERIFY(slider->setProperty("snapMode", QVariant(snapMode)));
    QCOMPARE(slider->property("snapMode").toInt(), snapMode);
    QQuickItem *handle = slider->property("handle").value<QQuickItem*>();
    QVERIFY(handle);

    const QPoint startPos(slider->property("leftPadding").toReal(), slider->height() / 2);
    const int trackWidth = slider->property("availableWidth").toReal();

    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, startPos, 200);
    QPoint pos1 = startPos + QPoint(trackWidth * 0.3, 0);
    moveSmoothly(window, startPos, pos1, pos1.x() - startPos.x(), QEasingCurve::OutQuint, 30);
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, pos1, 0);

    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, startPos, 400);
    const QPoint pos2 = startPos + QPoint(trackWidth * 0.6, 0);
    moveSmoothly(window, pos1, pos2, pos2.x() - pos1.x(), QEasingCurve::OutQuint, 30);
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, pos2, 0);

    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, pos2, 400);
    moveSmoothly(window, pos2, startPos, qAbs(startPos.x() - pos2.x()) / 2, QEasingCurve::OutQuint, 30);
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, startPos, 0);

    gifRecorder.waitForFinish();
}

void tst_Gifs::rangeSlider()
{
    GifRecorder gifRecorder;
    gifRecorder.setDataDirPath(dataDirPath);
    gifRecorder.setOutputDir(outputDir);
    gifRecorder.setRecordingDuration(7);
    gifRecorder.setHighQuality(true);
    gifRecorder.setQmlFileName("qtquickcontrols2-rangeslider.qml");

    gifRecorder.start();

    QQuickWindow *window = gifRecorder.window();
    QQuickItem *slider = window->property("slider").value<QQuickItem*>();
    QVERIFY(slider);
    QObject *first = slider->property("first").value<QObject*>();
    QVERIFY(first);
    QQuickItem *firstHandle = first->property("handle").value<QQuickItem*>();
    QVERIFY(firstHandle);
    QObject *second = slider->property("second").value<QObject*>();
    QVERIFY(second);
    QQuickItem *secondHandle = second->property("handle").value<QQuickItem*>();
    QVERIFY(secondHandle);

    const QPoint firstCenter = firstHandle->mapToItem(slider,
        QPoint(firstHandle->width() / 2, firstHandle->height() / 2)).toPoint();
    const QPoint secondCenter = secondHandle->mapToItem(slider,
        QPoint(secondHandle->width() / 2, secondHandle->height() / 2)).toPoint();

    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, firstCenter, 100);
    const QPoint firstTarget = firstCenter + QPoint(slider->width() * 0.25, 0);
    moveSmoothly(window, firstCenter, firstTarget, firstTarget.x() - firstCenter.x());
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, firstTarget, 20);

    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, secondCenter, 100);
    const QPoint secondTarget = secondCenter - QPoint(slider->width() * 0.25, 0);
    moveSmoothly(window, secondCenter, secondTarget, qAbs(secondTarget.x() - secondCenter.x()));
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, secondTarget, 20);

    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, secondTarget, 100);
    moveSmoothly(window, secondTarget, secondCenter, qAbs(secondTarget.x() - secondCenter.x()));
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, secondCenter, 20);

    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, firstTarget, 100);
    moveSmoothly(window, firstTarget, firstCenter, firstTarget.x() - firstCenter.x());
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, firstCenter, 20);

    gifRecorder.waitForFinish();
}

void tst_Gifs::busyIndicator()
{
    GifRecorder gifRecorder;
    gifRecorder.setDataDirPath(dataDirPath);
    gifRecorder.setOutputDir(outputDir);
    gifRecorder.setRecordingDuration(6);
    gifRecorder.setHighQuality(true);
    gifRecorder.setQmlFileName("qtquickcontrols2-busyindicator.qml");

    gifRecorder.start();

    QQuickWindow *window = gifRecorder.window();
    // Record nothing for a bit to make it smoother.
    QTest::qWait(800 * 2);

    QQuickItem *busyIndicator = window->property("busyIndicator").value<QQuickItem*>();
    QVERIFY(busyIndicator);

    busyIndicator->setProperty("running", false);

    // 800 ms is the duration of one rotation animation cycle for BusyIndicator.
    QTest::qWait(800 * 2);

    busyIndicator->setProperty("running", true);

    gifRecorder.waitForFinish();
}

void tst_Gifs::switchGif()
{
    GifRecorder gifRecorder;
    gifRecorder.setDataDirPath(dataDirPath);
    gifRecorder.setOutputDir(outputDir);
    gifRecorder.setRecordingDuration(3);
    gifRecorder.setQmlFileName("qtquickcontrols2-switch.qml");
    gifRecorder.setHighQuality(true);

    gifRecorder.start();

    QQuickWindow *window = gifRecorder.window();
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, QPoint(window->width() * 0.8, window->height() / 2), 0);
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, QPoint(window->width() * 0.2, window->height() / 2), 800);

    gifRecorder.waitForFinish();
}

void tst_Gifs::button_data()
{
    QTest::addColumn<QString>("qmlFileName");
    QTest::newRow("button") << QString::fromLatin1("qtquickcontrols2-button.qml");
    QTest::newRow("button-flat") << QString::fromLatin1("qtquickcontrols2-button-flat.qml");
    QTest::newRow("button-highlighted") << QString::fromLatin1("qtquickcontrols2-button-highlighted.qml");
}

void tst_Gifs::button()
{
    QFETCH(QString, qmlFileName);

    GifRecorder gifRecorder;
    gifRecorder.setDataDirPath(dataDirPath);
    gifRecorder.setOutputDir(outputDir);
    gifRecorder.setRecordingDuration(3);
    gifRecorder.setQmlFileName(qmlFileName);

    gifRecorder.start();

    QQuickWindow *window = gifRecorder.window();
    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, QPoint(window->width() / 2, window->height() / 2), 0);
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, QPoint(window->width() / 2, window->height() / 2), 700);

    gifRecorder.waitForFinish();
}

void tst_Gifs::tabBar()
{
    const QString qmlFileName = QStringLiteral("qtquickcontrols2-tabbar.qml");

    GifRecorder gifRecorder;
    gifRecorder.setDataDirPath(dataDirPath);
    gifRecorder.setOutputDir(outputDir);
    gifRecorder.setRecordingDuration(4);
    gifRecorder.setQmlFileName(qmlFileName);
    gifRecorder.setHighQuality(true);

    gifRecorder.start();

    QQuickWindow *window = gifRecorder.window();
    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, QPoint(window->width() * 0.6, window->height() / 2), 0);
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, QPoint(window->width() * 0.6, window->height() / 2), 50);

    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, QPoint(window->width() * 0.9, window->height() / 2), 400);
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, QPoint(window->width() * 0.9, window->height() / 2), 50);

    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, QPoint(window->width() * 0.6, window->height() / 2), 800);
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, QPoint(window->width() * 0.6, window->height() / 2), 50);

    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, QPoint(window->width() * 0.3, window->height() / 2), 400);
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, QPoint(window->width() * 0.3, window->height() / 2), 50);

    gifRecorder.waitForFinish();
}

void tst_Gifs::menu()
{
    const QString qmlFileName = QStringLiteral("qtquickcontrols2-menu.qml");

    GifRecorder gifRecorder;
    gifRecorder.setDataDirPath(dataDirPath);
    gifRecorder.setOutputDir(outputDir);
    gifRecorder.setRecordingDuration(3);
    gifRecorder.setQmlFileName(qmlFileName);
    gifRecorder.setHighQuality(true);

    gifRecorder.start();

    QQuickWindow *window = gifRecorder.window();
    const QQuickItem *fileButton = window->property("fileButton").value<QQuickItem*>();
    QVERIFY(fileButton);

    const QPoint fileButtonCenter = fileButton->mapToScene(QPointF(fileButton->width() / 2, fileButton->height() / 2)).toPoint();
    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, fileButtonCenter, 0);
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, fileButtonCenter, 200);

    const QObject *menu = window->property("menu").value<QObject*>();
    QVERIFY(menu);
    const QQuickItem *menuContentItem = menu->property("contentItem").value<QQuickItem*>();
    QVERIFY(menuContentItem);

    const QPoint lastItemPos = menuContentItem->mapToScene(QPointF(menuContentItem->width() / 2, menuContentItem->height() - 10)).toPoint();
    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, lastItemPos, 1000);
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, lastItemPos, 300);

    gifRecorder.waitForFinish();
}

void tst_Gifs::swipeView()
{
    GifRecorder gifRecorder;
    gifRecorder.setDataDirPath(dataDirPath);
    gifRecorder.setOutputDir(outputDir);
    gifRecorder.setRecordingDuration(8);
    gifRecorder.setQmlFileName(QStringLiteral("qtquickcontrols2-swipeview.qml"));
    gifRecorder.setHighQuality(true);

    gifRecorder.start();

    QQuickWindow *window = gifRecorder.window();
    QQuickItem *swipeView = window->property("swipeView").value<QQuickItem*>();
    QVERIFY(swipeView);

    QTest::qWait(1200);
    swipeView->setProperty("currentIndex", 1);
    QTest::qWait(2000);
    swipeView->setProperty("currentIndex", 2);
    QTest::qWait(2000);
    swipeView->setProperty("currentIndex", 0);

    gifRecorder.waitForFinish();
}

void tst_Gifs::swipeDelegate_data()
{
    QTest::addColumn<QString>("qmlFileName");
    QTest::newRow("qtquickcontrols2-swipedelegate.qml") << QString::fromLatin1("qtquickcontrols2-swipedelegate.qml");
    QTest::newRow("qtquickcontrols2-swipedelegate-leading-trailing.qml") << QString::fromLatin1("qtquickcontrols2-swipedelegate-leading-trailing.qml");
}

void tst_Gifs::swipeDelegate()
{
    QFETCH(QString, qmlFileName);

    GifRecorder gifRecorder;
    gifRecorder.setDataDirPath(dataDirPath);
    gifRecorder.setOutputDir(outputDir);
    gifRecorder.setRecordingDuration(10);
    gifRecorder.setQmlFileName(qmlFileName);
    gifRecorder.setHighQuality(true);

    gifRecorder.start();

    QQuickWindow *window = gifRecorder.window();
    QQuickItem *swipeDelegate = window->property("swipeDelegate").value<QQuickItem*>();
    QVERIFY(swipeDelegate);

    // Show left item.
    const QPoint leftTarget = QPoint(swipeDelegate->width() * 0.2, 0);
    const QPoint rightTarget = QPoint(swipeDelegate->width() * 0.8, 0);
    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, leftTarget, 100);
    const int movements = rightTarget.x() - leftTarget.x();
    moveSmoothly(window, leftTarget, rightTarget, movements, QEasingCurve::OutQuint, 5);
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, rightTarget, 20);

    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, rightTarget, 1000);
    moveSmoothly(window, rightTarget, leftTarget, movements, QEasingCurve::OutQuint, 5);
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, leftTarget, 20);

    QTest::qWait(1000);

    // Show right item.
    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, rightTarget, 1000);
    moveSmoothly(window, rightTarget, leftTarget, movements, QEasingCurve::OutQuint, 5);
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, leftTarget, 20);

    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, leftTarget, 1000);
    moveSmoothly(window, leftTarget, rightTarget, movements, QEasingCurve::OutQuint, 5);
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, rightTarget, 20);

    gifRecorder.waitForFinish();
}

void tst_Gifs::swipeDelegateBehind()
{
    GifRecorder gifRecorder;
    gifRecorder.setDataDirPath(dataDirPath);
    gifRecorder.setOutputDir(outputDir);
    gifRecorder.setRecordingDuration(14);
    gifRecorder.setQmlFileName(QStringLiteral("qtquickcontrols2-swipedelegate-behind.qml"));
    gifRecorder.setHighQuality(true);

    gifRecorder.start();

    QQuickWindow *window = gifRecorder.window();
    QQuickItem *swipeDelegate = window->property("swipeDelegate").value<QQuickItem*>();
    QVERIFY(swipeDelegate);

    // Show wrapping around left item.
    const QPoint leftTarget = QPoint(swipeDelegate->width() * 0.2, 0);
    const QPoint rightTarget = QPoint(swipeDelegate->width() * 0.8, 0);
    const int movements = rightTarget.x() - leftTarget.x();
    for (int i = 0; i < 4; ++i) {
        QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, leftTarget, 100);
        moveSmoothly(window, leftTarget, rightTarget, movements, QEasingCurve::OutQuint, 5);
        QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, rightTarget, 20);

        QTest::qWait(500);
    }

    QTest::qWait(1000);

    // Show wrapping around right item.
    for (int i = 0; i < 4; ++i) {
        QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, rightTarget, 100);
        moveSmoothly(window, rightTarget, leftTarget, movements, QEasingCurve::OutQuint, 5);
        QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, leftTarget, 20);

        QTest::qWait(500);
    }

    gifRecorder.waitForFinish();
}

void tst_Gifs::delegates_data()
{
    QTest::addColumn<QString>("name");
    QTest::addColumn<QVector<int> >("pressIndices");
    QTest::addColumn<int>("duration");

    QTest::newRow("ItemDelegate") << "itemdelegate" << (QVector<int>() << 0 << 1 << 2) << 5;
    QTest::newRow("CheckDelegate") << "checkdelegate" << (QVector<int>() << 0 << 0) << 5;
    QTest::newRow("RadioDelegate") << "radiodelegate" << (QVector<int>() << 1 << 0) << 5;
    QTest::newRow("SwitchDelegate") << "switchdelegate" << (QVector<int>() << 0 << 0) << 5;
}

void tst_Gifs::delegates()
{
    QFETCH(QString, name);
    QFETCH(QVector<int>, pressIndices);
    QFETCH(int, duration);

    GifRecorder gifRecorder;
    gifRecorder.setDataDirPath(dataDirPath);
    gifRecorder.setOutputDir(outputDir);
    gifRecorder.setRecordingDuration(duration);
    gifRecorder.setQmlFileName(QString::fromLatin1("qtquickcontrols2-%1.qml").arg(name));
    gifRecorder.setHighQuality(true);

    gifRecorder.start();

    QQuickWindow *window = gifRecorder.window();
    QQuickItem *delegate = window->property("delegate").value<QQuickItem*>();
    QVERIFY(delegate);

    for (int i = 0; i < pressIndices.size(); ++i) {
        const int pressIndex = pressIndices.at(i);
        const QPoint delegateCenter(delegate->mapToScene(QPointF(
            delegate->width() / 2, delegate->height() / 2 + delegate->height() * pressIndex)).toPoint());
        QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, delegateCenter, i == 0 ? 200 : 1000);
        QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, delegateCenter, 400);
    }

    gifRecorder.waitForFinish();
}

void tst_Gifs::dial_data()
{
    QTest::addColumn<QString>("name");

    QTest::newRow("dial-wrap") << "wrap";
    QTest::newRow("dial-no-wrap") << "no-wrap";
}

void tst_Gifs::dial()
{
    QFETCH(QString, name);

    GifRecorder gifRecorder;
    gifRecorder.setDataDirPath(dataDirPath);
    gifRecorder.setOutputDir(outputDir);
    gifRecorder.setRecordingDuration(10);
    gifRecorder.setQmlFileName(QString::fromLatin1("qtquickcontrols2-dial-%1.qml").arg(name));
    gifRecorder.setHighQuality(false);

    gifRecorder.start();

    QQuickWindow *window = gifRecorder.window();
    QQuickItem *dial = window->property("dial").value<QQuickItem*>();
    QVERIFY(dial);

    const QPoint arcCenter = dial->mapToScene(QPoint(dial->width() / 2, dial->height() / 2)).toPoint();
    const qreal distanceFromCenter = dial->height() * 0.25;
    // Go a bit past the actual min/max to ensure that we get the full range.
    const qreal minAngle = qDegreesToRadians(-170.0);
    const qreal maxAngle = qDegreesToRadians(170.0);
    // Drag from start to end.
    qreal startAngle = minAngle;
    qreal endAngle = maxAngle;
    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, posAlongArc(
        arcCenter, startAngle, endAngle, distanceFromCenter, 0, QEasingCurve::InOutQuad), 30);

    moveSmoothlyAlongArc(window, arcCenter, distanceFromCenter, startAngle, endAngle, QEasingCurve::InOutQuad);

    // Come back from the end a bit.
    startAngle = endAngle;
    endAngle -= qDegreesToRadians(50.0);
    moveSmoothlyAlongArc(window, arcCenter, distanceFromCenter, startAngle, endAngle, QEasingCurve::InOutQuad);

    // Try to drag over max to show what happens with different wrap settings.
    startAngle = endAngle;
    endAngle = qDegreesToRadians(270.0);
    moveSmoothlyAlongArc(window, arcCenter, distanceFromCenter, startAngle, endAngle, QEasingCurve::InOutQuad);

    // Go back to the start so that it loops nicely.
    startAngle = endAngle;
    endAngle = minAngle;
    moveSmoothlyAlongArc(window, arcCenter, distanceFromCenter, startAngle, endAngle, QEasingCurve::InOutQuad);

    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, posAlongArc(
        arcCenter, startAngle, endAngle, distanceFromCenter, 1, QEasingCurve::InOutQuad), 30);

    gifRecorder.waitForFinish();
}

void tst_Gifs::checkables_data()
{
    QTest::addColumn<QString>("name");
    QTest::addColumn<QVector<int> >("pressIndices");

    QTest::newRow("checkbox") << "checkbox" << (QVector<int>() << 1 << 2 << 2 << 1);
    QTest::newRow("radiobutton") << "radiobutton" << (QVector<int>() << 1 << 2 << 1 << 0);
}

void tst_Gifs::checkables()
{
    QFETCH(QString, name);
    QFETCH(QVector<int>, pressIndices);

    GifRecorder gifRecorder;
    gifRecorder.setDataDirPath(dataDirPath);
    gifRecorder.setOutputDir(outputDir);
    gifRecorder.setRecordingDuration(6);
    gifRecorder.setQmlFileName(QString::fromLatin1("qtquickcontrols2-%1.qml").arg(name));

    gifRecorder.start();

    QQuickWindow *window = gifRecorder.window();

    for (int i = 0; i < pressIndices.size(); ++i) {
        const int pressIndex = pressIndices.at(i);
        const char *controlId = qPrintable(QString::fromLatin1("control%1").arg(pressIndex + 1));
        QQuickItem *control = window->property(controlId).value<QQuickItem*>();
        QVERIFY(control);

        const QPoint pos = control->mapToScene(QPointF(control->width() / 2, control->height() / 2)).toPoint();
        QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, pos, 800);
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, pos, 300);
    }

    gifRecorder.waitForFinish();
}

void tst_Gifs::comboBox()
{
    GifRecorder gifRecorder;
    gifRecorder.setDataDirPath(dataDirPath);
    gifRecorder.setOutputDir(outputDir);
    gifRecorder.setRecordingDuration(6);
    gifRecorder.setQmlFileName(QStringLiteral("qtquickcontrols2-combobox.qml"));

    gifRecorder.start();

    QQuickWindow *window = gifRecorder.window();
    QQuickItem *comboBox = window->property("comboBox").value<QQuickItem*>();
    QVERIFY(comboBox);

    // Open the popup.
    const QPoint center = comboBox->mapToScene(
        QPoint(comboBox->width() / 2, comboBox->height() / 2)).toPoint();
    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, center, 800);
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, center, 80);

    // Select the third item.
    QObject *popup = comboBox->property("popup").value<QObject*>();
    QVERIFY(popup);
    QQuickItem *popupContent = popup->property("contentItem").value<QQuickItem*>();
    QVERIFY(popupContent);
    const QPoint lastItemPos = popupContent->mapToScene(
        QPoint(popupContent->width() / 2, popupContent->height() * 0.8)).toPoint();
    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, lastItemPos, 600);
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, lastItemPos, 200);

    // Open the popup.
    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, center, 1500);
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, center, 80);

    // Select the first item.
    const QPoint firstItemPos = popupContent->mapToScene(
        QPoint(popupContent->width() / 2, popupContent->height() * 0.2)).toPoint();
    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, firstItemPos, 600);
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, firstItemPos, 200);

    gifRecorder.waitForFinish();
}

void tst_Gifs::triState_data()
{
    QTest::addColumn<QString>("name");

    QTest::newRow("checkbox-tristate") << "checkbox-tristate";
    QTest::newRow("checkdelegate-tristate") << "checkdelegate-tristate";
}

void tst_Gifs::triState()
{
    QFETCH(QString, name);

    GifRecorder gifRecorder;
    gifRecorder.setDataDirPath(dataDirPath);
    gifRecorder.setOutputDir(outputDir);
    gifRecorder.setRecordingDuration(6);
    gifRecorder.setQmlFileName(QString::fromLatin1("qtquickcontrols2-%1.qml").arg(name));

    gifRecorder.start();

    QQuickWindow *window = gifRecorder.window();
    QQuickItem *english = window->property("english").value<QQuickItem*>();
    QVERIFY(english);
    QQuickItem *norwegian = window->property("norwegian").value<QQuickItem*>();
    QVERIFY(norwegian);

    const QPoint englishCenter = english->mapToScene(
        QPointF(english->width() / 2, english->height() / 2)).toPoint();
    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, englishCenter, 1000);
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, englishCenter, 300);

    const QPoint norwegianCenter = norwegian->mapToScene(
        QPointF(norwegian->width() / 2, norwegian->height() / 2)).toPoint();
    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, norwegianCenter, 1000);
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, norwegianCenter, 300);

    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, norwegianCenter, 1000);
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, norwegianCenter, 300);

    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, englishCenter, 1000);
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, englishCenter, 300);

    gifRecorder.waitForFinish();
}

void tst_Gifs::scrollBar()
{
    GifRecorder gifRecorder;
    gifRecorder.setDataDirPath(dataDirPath);
    gifRecorder.setOutputDir(outputDir);
    gifRecorder.setRecordingDuration(6);
    gifRecorder.setQmlFileName("qtquickcontrols2-scrollbar.qml");

    gifRecorder.start();

    QQuickWindow *window = gifRecorder.window();
    QQuickItem *scrollBar = window->property("scrollBar").value<QQuickItem*>();
    QVERIFY(scrollBar);

    // Flick in the center of the screen to show that there's a scroll bar.
    const QPoint lhsWindowBottom = QPoint(0, window->height() - 1);
    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, lhsWindowBottom, 100);
    QTest::mouseMove(window, lhsWindowBottom - QPoint(0, 10), 30);
    QTest::mouseMove(window, lhsWindowBottom - QPoint(0, 30), 30);
    QTest::mouseMove(window, lhsWindowBottom - QPoint(0, 60), 30);
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, lhsWindowBottom - QPoint(0, 100), 30);

    // Scroll with the scroll bar.
    const QPoint rhsWindowBottom = QPoint(window->width() - 1, window->height() - 1);
    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, rhsWindowBottom, 2000);
    const QPoint rhsWindowTop = QPoint(window->width() - 1, 1);
    moveSmoothly(window, rhsWindowBottom, rhsWindowTop,
        qAbs(rhsWindowTop.y() - rhsWindowBottom.y()), QEasingCurve::InCubic, 10);
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, rhsWindowTop, 20);

    gifRecorder.waitForFinish();
}

void tst_Gifs::scrollIndicator()
{
    GifRecorder gifRecorder;
    gifRecorder.setDataDirPath(dataDirPath);
    gifRecorder.setOutputDir(outputDir);
    gifRecorder.setRecordingDuration(6);
    gifRecorder.setQmlFileName("qtquickcontrols2-scrollindicator.qml");

    gifRecorder.start();

    // Flick in the center of the screen to show that there's a scroll indicator.
    QQuickWindow *window = gifRecorder.window();
    const QPoint windowBottom = QPoint(0, window->height() - 1);
    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, windowBottom, 100);
    QTest::mouseMove(window, windowBottom - QPoint(0, 10), 30);
    QTest::mouseMove(window, windowBottom - QPoint(0, 30), 30);
    QTest::mouseMove(window, windowBottom - QPoint(0, 60), 30);
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, windowBottom - QPoint(0, 100), 30);

    // Scroll back down.
    const QPoint windowTop = QPoint(0, 0);
    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, windowTop, 2000);
    QTest::mouseMove(window, windowTop + QPoint(0, 10), 30);
    QTest::mouseMove(window, windowTop + QPoint(0, 30), 30);
    QTest::mouseMove(window, windowTop + QPoint(0, 60), 30);
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, windowTop + QPoint(0, 100), 30);

    gifRecorder.waitForFinish();
}

void tst_Gifs::progressBar_data()
{
    QTest::addColumn<bool>("indeterminate");

    QTest::newRow("indeterminate:false") << false;
    QTest::newRow("indeterminate:true") << true;
}

void tst_Gifs::progressBar()
{
    QFETCH(bool, indeterminate);

    GifRecorder gifRecorder;
    gifRecorder.setDataDirPath(dataDirPath);
    gifRecorder.setOutputDir(outputDir);
    gifRecorder.setRecordingDuration(4);
    gifRecorder.setQmlFileName(QString::fromLatin1("qtquickcontrols2-progressbar%1").arg(
        indeterminate ? QLatin1String("-indeterminate.qml") : QLatin1String(".qml")));

    gifRecorder.start();
    gifRecorder.waitForFinish();
}

void tst_Gifs::stackView_data()
{
    QTest::addColumn<QString>("name");
    QTest::addColumn<int>("duration");

    QTest::newRow("push") << "push" << 8;
    QTest::newRow("pop") << "pop" << 6;
    QTest::newRow("unwind") << "unwind" << 6;
    QTest::newRow("replace") << "replace" << 6;
}

void tst_Gifs::stackView()
{
    QFETCH(QString, name);
    QFETCH(int, duration);

    GifRecorder gifRecorder;
    gifRecorder.setDataDirPath(dataDirPath);
    gifRecorder.setOutputDir(outputDir);
    gifRecorder.setRecordingDuration(duration);
    gifRecorder.setHighQuality(true);
    gifRecorder.setQmlFileName(QString::fromLatin1("qtquickcontrols2-stackview-%1.qml").arg(name));

    gifRecorder.start();
    gifRecorder.waitForFinish();
}

void tst_Gifs::drawer()
{
    GifRecorder gifRecorder;
    gifRecorder.setDataDirPath(dataDirPath);
    gifRecorder.setOutputDir(outputDir);
    gifRecorder.setRecordingDuration(4);
    gifRecorder.setHighQuality(true);
    gifRecorder.setQmlFileName("qtquickcontrols2-drawer.qml");

    gifRecorder.start();

    QQuickWindow *window = gifRecorder.window();
    QObject *drawer = window->property("drawer").value<QObject*>();
    qreal width = drawer->property("width").toReal();

    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, QPoint(1, 1), 100);
    moveSmoothly(window, QPoint(1, 1), QPoint(width, 1), width, QEasingCurve::InOutBack, 1);
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, QPoint(width, 1), 30);

    QTest::qWait(1000);
    QMetaObject::invokeMethod(drawer, "close");

    gifRecorder.waitForFinish();
}

QTEST_MAIN(tst_Gifs)

#include "tst_gifs.moc"
