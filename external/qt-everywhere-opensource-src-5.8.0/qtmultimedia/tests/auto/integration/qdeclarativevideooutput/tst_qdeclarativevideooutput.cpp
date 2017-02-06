/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Toolkit.
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

//TESTED_COMPONENT=plugins/declarative/multimedia

#include <QtTest/QtTest>

#include <QtQml/qqmlengine.h>
#include <QtQml/qqmlcomponent.h>

#include "private/qdeclarativevideooutput_p.h"

#include <qabstractvideosurface.h>
#include <qvideorenderercontrol.h>
#include <qvideosurfaceformat.h>

#include <qmediaobject.h>

class SurfaceHolder : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QAbstractVideoSurface *videoSurface READ videoSurface WRITE setVideoSurface)
public:
    SurfaceHolder(QObject *parent)
        : QObject(parent)
        , m_surface(0)
    {
    }

    QAbstractVideoSurface *videoSurface() const
    {
        return m_surface;
    }
    void setVideoSurface(QAbstractVideoSurface *surface)
    {
        if (m_surface != surface && m_surface && m_surface->isActive()) {
            m_surface->stop();
        }
        m_surface = surface;
    }

    void presentDummyFrame(const QSize &size);

private:
    QAbstractVideoSurface *m_surface;

};

// Starts the surface and puts a frame
void SurfaceHolder::presentDummyFrame(const QSize &size)
{
    if (m_surface && m_surface->supportedPixelFormats().count() > 0) {
        QVideoFrame::PixelFormat pixelFormat = m_surface->supportedPixelFormats().value(0);
        QVideoSurfaceFormat format(size, pixelFormat);
        QVideoFrame frame(size.width() * size.height() * 4, size, size.width() * 4, pixelFormat);

        if (!m_surface->isActive())
            m_surface->start(format);
        m_surface->present(frame);

        // Have to spin an event loop or two for the surfaceFormatChanged() signal
        qApp->processEvents();
    }
}

class tst_QDeclarativeVideoOutput : public QObject
{
    Q_OBJECT
public:
    tst_QDeclarativeVideoOutput();

    ~tst_QDeclarativeVideoOutput()
    {
        delete m_mappingOutput;
        delete m_mappingSurface;
        delete m_mappingComponent;
    }

public slots:
    void initTestCase();

private slots:
    void fillMode();
    void orientation();
    void surfaceSource();
    void sourceRect();

    void contentRect();
    void contentRect_data();

    void mappingPoint();
    void mappingPoint_data();
    void mappingRect();
    void mappingRect_data();

    // XXX May be worth adding tests that the surface activeChanged signals are sent appropriately
    // to holder?

private:
    QQmlEngine m_engine;
    QByteArray m_plainQML;

    // Variables used for the mapping test
    QQmlComponent *m_mappingComponent;
    QObject *m_mappingOutput;
    SurfaceHolder *m_mappingSurface;

    void updateOutputGeometry(QObject *output);

    QRectF invokeR2R(QObject *object, const char *signature, const QRectF &rect);
    QPointF invokeP2P(QObject *object, const char *signature, const QPointF &point);
};

void tst_QDeclarativeVideoOutput::initTestCase()
{
    m_plainQML = \
            "import QtQuick 2.0\n" \
            "import QtMultimedia 5.0\n" \
            "VideoOutput {" \
            "    width: 150;" \
            "    height: 100;" \
            "}";

    // We initialize the mapping vars here
    m_mappingComponent = new QQmlComponent(&m_engine);
    m_mappingComponent->setData(m_plainQML, QUrl());
    m_mappingSurface = new SurfaceHolder(this);

    m_mappingOutput = m_mappingComponent->create();
    QVERIFY(m_mappingOutput != 0);

    m_mappingOutput->setProperty("source", QVariant::fromValue(static_cast<QObject*>(m_mappingSurface)));

    m_mappingSurface->presentDummyFrame(QSize(200,100)); // this should start m_surface
    updateOutputGeometry(m_mappingOutput);
}

Q_DECLARE_METATYPE(QDeclarativeVideoOutput::FillMode)

tst_QDeclarativeVideoOutput::tst_QDeclarativeVideoOutput()
    : m_mappingComponent(0)
    , m_mappingOutput(0)
    , m_mappingSurface(0)
{
    qRegisterMetaType<QDeclarativeVideoOutput::FillMode>();
}

void tst_QDeclarativeVideoOutput::fillMode()
{
    QQmlComponent component(&m_engine);
    component.setData(m_plainQML, QUrl());

    QObject *videoOutput = component.create();
    QVERIFY(videoOutput != 0);

    QSignalSpy propSpy(videoOutput, SIGNAL(fillModeChanged(QDeclarativeVideoOutput::FillMode)));

    // Default is preserveaspectfit
    QCOMPARE(videoOutput->property("fillMode").value<QDeclarativeVideoOutput::FillMode>(), QDeclarativeVideoOutput::PreserveAspectFit);
    QCOMPARE(propSpy.count(), 0);

    videoOutput->setProperty("fillMode", QVariant(int(QDeclarativeVideoOutput::PreserveAspectCrop)));
    QCOMPARE(videoOutput->property("fillMode").value<QDeclarativeVideoOutput::FillMode>(), QDeclarativeVideoOutput::PreserveAspectCrop);
    QCOMPARE(propSpy.count(), 1);

    videoOutput->setProperty("fillMode", QVariant(int(QDeclarativeVideoOutput::Stretch)));
    QCOMPARE(videoOutput->property("fillMode").value<QDeclarativeVideoOutput::FillMode>(), QDeclarativeVideoOutput::Stretch);
    QCOMPARE(propSpy.count(), 2);

    videoOutput->setProperty("fillMode", QVariant(int(QDeclarativeVideoOutput::Stretch)));
    QCOMPARE(videoOutput->property("fillMode").value<QDeclarativeVideoOutput::FillMode>(), QDeclarativeVideoOutput::Stretch);
    QCOMPARE(propSpy.count(), 2);

    delete videoOutput;
}

void tst_QDeclarativeVideoOutput::orientation()
{
    QQmlComponent component(&m_engine);
    component.setData(m_plainQML, QUrl());

    QObject *videoOutput = component.create();
    QVERIFY(videoOutput != 0);

    QSignalSpy propSpy(videoOutput, SIGNAL(orientationChanged()));

    // Default orientation is 0
    QCOMPARE(videoOutput->property("orientation").toInt(), 0);
    QCOMPARE(propSpy.count(), 0);

    videoOutput->setProperty("orientation", QVariant(90));
    QCOMPARE(videoOutput->property("orientation").toInt(), 90);
    QCOMPARE(propSpy.count(), 1);

    videoOutput->setProperty("orientation", QVariant(180));
    QCOMPARE(videoOutput->property("orientation").toInt(), 180);
    QCOMPARE(propSpy.count(), 2);

    videoOutput->setProperty("orientation", QVariant(270));
    QCOMPARE(videoOutput->property("orientation").toInt(), 270);
    QCOMPARE(propSpy.count(), 3);

    videoOutput->setProperty("orientation", QVariant(360));
    QCOMPARE(videoOutput->property("orientation").toInt(), 360);
    QCOMPARE(propSpy.count(), 4);

    // More than 360 should be fine
    videoOutput->setProperty("orientation", QVariant(540));
    QCOMPARE(videoOutput->property("orientation").toInt(), 540);
    QCOMPARE(propSpy.count(), 5);

    // Negative should be fine
    videoOutput->setProperty("orientation", QVariant(-180));
    QCOMPARE(videoOutput->property("orientation").toInt(), -180);
    QCOMPARE(propSpy.count(), 6);

    // Same value should not reemit
    videoOutput->setProperty("orientation", QVariant(-180));
    QCOMPARE(videoOutput->property("orientation").toInt(), -180);
    QCOMPARE(propSpy.count(), 6);

    // Non multiples of 90 should not work
    videoOutput->setProperty("orientation", QVariant(-1));
    QCOMPARE(videoOutput->property("orientation").toInt(), -180);
    QCOMPARE(propSpy.count(), 6);

    delete videoOutput;
}

void tst_QDeclarativeVideoOutput::surfaceSource()
{
    QQmlComponent component(&m_engine);
    component.setData(m_plainQML, QUrl());

    QObject *videoOutput = component.create();
    QVERIFY(videoOutput != 0);

    SurfaceHolder holder(this);

    QCOMPARE(holder.videoSurface(), static_cast<QAbstractVideoSurface*>(0));

    videoOutput->setProperty("source", QVariant::fromValue(static_cast<QObject*>(&holder)));

    QVERIFY(holder.videoSurface() != 0);

    // Now we could do things with the surface..
    QList<QVideoFrame::PixelFormat> formats = holder.videoSurface()->supportedPixelFormats();
    QVERIFY(formats.count() > 0);

    // See if we can start and stop each pixel format (..)
    foreach (QVideoFrame::PixelFormat format, formats) {
        QVideoSurfaceFormat surfaceFormat(QSize(200,100), format);
        QVERIFY(holder.videoSurface()->isFormatSupported(surfaceFormat)); // This does kind of depend on node factories

        QVERIFY(holder.videoSurface()->start(surfaceFormat));
        QVERIFY(holder.videoSurface()->surfaceFormat() == surfaceFormat);
        QVERIFY(holder.videoSurface()->isActive());

        holder.videoSurface()->stop();

        QVERIFY(!holder.videoSurface()->isActive());
    }

    delete videoOutput;

    // This should clear the surface
    QCOMPARE(holder.videoSurface(), static_cast<QAbstractVideoSurface*>(0));

    // Also, creating two sources, setting them in order, and destroying the first
    // should not zero holder.videoSurface()
    videoOutput = component.create();
    videoOutput->setProperty("source", QVariant::fromValue(static_cast<QObject*>(&holder)));

    QAbstractVideoSurface *surface = holder.videoSurface();
    QVERIFY(holder.videoSurface());

    QObject *videoOutput2 = component.create();
    QVERIFY(videoOutput2);
    videoOutput2->setProperty("source", QVariant::fromValue(static_cast<QObject*>(&holder)));
    QVERIFY(holder.videoSurface());
    QVERIFY(holder.videoSurface() != surface); // Surface should have changed
    surface = holder.videoSurface();

    // Now delete first one
    delete videoOutput;
    QVERIFY(holder.videoSurface());
    QVERIFY(holder.videoSurface() == surface); // Should not have changed surface

    // Now create a second surface and assign it as the source
    // The old surface holder should be zeroed
    SurfaceHolder holder2(this);
    videoOutput2->setProperty("source", QVariant::fromValue(static_cast<QObject*>(&holder2)));

    QCOMPARE(holder.videoSurface(), static_cast<QAbstractVideoSurface*>(0));
    QVERIFY(holder2.videoSurface() != 0);

    // Finally a combination - set the same source to two things, then assign a new source
    // to the first output - should not reset the first source
    videoOutput = component.create();
    videoOutput->setProperty("source", QVariant::fromValue(static_cast<QObject*>(&holder2)));

    // Both vo and vo2 were pointed to holder2 - setting vo2 should not clear holder2
    QVERIFY(holder2.videoSurface() != 0);
    QVERIFY(holder.videoSurface() == 0);
    videoOutput2->setProperty("source", QVariant::fromValue(static_cast<QObject*>(&holder)));
    QVERIFY(holder2.videoSurface() != 0);
    QVERIFY(holder.videoSurface() != 0);

    // They should also be independent
    QVERIFY(holder.videoSurface() != holder2.videoSurface());

    delete videoOutput;
    delete videoOutput2;
}

void tst_QDeclarativeVideoOutput::sourceRect()
{
    QQmlComponent component(&m_engine);
    component.setData(m_plainQML, QUrl());

    QObject *videoOutput = component.create();
    QVERIFY(videoOutput != 0);

    SurfaceHolder holder(this);

    QSignalSpy propSpy(videoOutput, SIGNAL(sourceRectChanged()));

    videoOutput->setProperty("source", QVariant::fromValue(static_cast<QObject*>(&holder)));

    QRectF invalid(0,0,-1,-1);

    QCOMPARE(videoOutput->property("sourceRect").toRectF(), invalid);

    holder.presentDummyFrame(QSize(200,100));

    QCOMPARE(videoOutput->property("sourceRect").toRectF(), QRectF(0, 0, 200, 100));
    QCOMPARE(propSpy.count(), 1);

    // Another frame shouldn't cause a source rect change
    holder.presentDummyFrame(QSize(200,100));
    QCOMPARE(propSpy.count(), 1);
    QCOMPARE(videoOutput->property("sourceRect").toRectF(), QRectF(0, 0, 200, 100));

    // Changing orientation and stretch modes should not affect this
    videoOutput->setProperty("orientation", QVariant(90));
    updateOutputGeometry(videoOutput);
    QCOMPARE(videoOutput->property("sourceRect").toRectF(), QRectF(0, 0, 200, 100));

    videoOutput->setProperty("orientation", QVariant(180));
    updateOutputGeometry(videoOutput);
    QCOMPARE(videoOutput->property("sourceRect").toRectF(), QRectF(0, 0, 200, 100));

    videoOutput->setProperty("orientation", QVariant(270));
    updateOutputGeometry(videoOutput);
    QCOMPARE(videoOutput->property("sourceRect").toRectF(), QRectF(0, 0, 200, 100));

    videoOutput->setProperty("orientation", QVariant(-90));
    updateOutputGeometry(videoOutput);
    QCOMPARE(videoOutput->property("sourceRect").toRectF(), QRectF(0, 0, 200, 100));

    videoOutput->setProperty("fillMode", QVariant(int(QDeclarativeVideoOutput::PreserveAspectCrop)));
    updateOutputGeometry(videoOutput);
    QCOMPARE(videoOutput->property("sourceRect").toRectF(), QRectF(0, 0, 200, 100));

    videoOutput->setProperty("fillMode", QVariant(int(QDeclarativeVideoOutput::Stretch)));
    updateOutputGeometry(videoOutput);
    QCOMPARE(videoOutput->property("sourceRect").toRectF(), QRectF(0, 0, 200, 100));

    videoOutput->setProperty("fillMode", QVariant(int(QDeclarativeVideoOutput::Stretch)));
    updateOutputGeometry(videoOutput);
    QCOMPARE(videoOutput->property("sourceRect").toRectF(), QRectF(0, 0, 200, 100));

    delete videoOutput;
}

void tst_QDeclarativeVideoOutput::mappingPoint()
{
    QFETCH(QPointF, point);
    QFETCH(int, orientation);
    QFETCH(QDeclarativeVideoOutput::FillMode, fillMode);
    QFETCH(QPointF, expected);

    QVERIFY(m_mappingOutput);
    m_mappingOutput->setProperty("orientation", QVariant(orientation));
    m_mappingOutput->setProperty("fillMode", QVariant::fromValue(fillMode));

    updateOutputGeometry(m_mappingOutput);

    QPointF output = invokeP2P(m_mappingOutput, "mapPointToItem", point);
    QPointF reverse = invokeP2P(m_mappingOutput, "mapPointToSource", output);

    QCOMPARE(output, expected);
    QCOMPARE(reverse, point);

    // Now the normalized versions
    // Source rectangle is 200x100
    QPointF normal(point.x() / 200, point.y() / 100);

    output = invokeP2P(m_mappingOutput, "mapNormalizedPointToItem", normal);
    reverse = invokeP2P(m_mappingOutput, "mapPointToSourceNormalized", output);

    QCOMPARE(output, expected);
    QCOMPARE(reverse, normal);
}

void tst_QDeclarativeVideoOutput::mappingPoint_data()
{
    QTest::addColumn<QPointF>("point");
    QTest::addColumn<int>("orientation");
    QTest::addColumn<QDeclarativeVideoOutput::FillMode>("fillMode");
    QTest::addColumn<QPointF>("expected");

    QDeclarativeVideoOutput::FillMode stretch = QDeclarativeVideoOutput::Stretch;
    QDeclarativeVideoOutput::FillMode fit = QDeclarativeVideoOutput::PreserveAspectFit;
    QDeclarativeVideoOutput::FillMode crop = QDeclarativeVideoOutput::PreserveAspectCrop;

    // First make sure the component has processed the frame
    QCOMPARE(m_mappingOutput->property("sourceRect").toRectF(), QRectF(0,0,200,100));

    // 200x100 -> 150,100 stretch, 150x75 fit @ 12.5f, 200x100 @-25,0 crop

    // Corners, then the center, then a point in the middle somewhere
    QTest::newRow("s0-0") << QPointF(0,0) << 0 << stretch << QPointF(0,0);
    QTest::newRow("s1-0") << QPointF(200,0) << 0 << stretch << QPointF(150,0);
    QTest::newRow("s2-0") << QPointF(0,100) << 0 << stretch << QPointF(0,100);
    QTest::newRow("s3-0") << QPointF(200,100) << 0 << stretch << QPointF(150,100);
    QTest::newRow("s4-0") << QPointF(100,50) << 0 << stretch << QPointF(75,50);
    QTest::newRow("s5-0") << QPointF(40,80) << 0 << stretch << QPointF(30,80);

    QTest::newRow("f0-0") << QPointF(0,0) << 0 << fit << QPointF(0,12.5f);
    QTest::newRow("f1-0") << QPointF(200,0) << 0 << fit << QPointF(150,12.5f);
    QTest::newRow("f2-0") << QPointF(0,100) << 0 << fit << QPointF(0,87.5f);
    QTest::newRow("f3-0") << QPointF(200,100) << 0 << fit << QPointF(150,87.5f);
    QTest::newRow("f4-0") << QPointF(100,50) << 0 << stretch << QPointF(75,50);
    QTest::newRow("f5-0") << QPointF(40,80) << 0 << stretch << QPointF(30,80);

    QTest::newRow("c0-0") << QPointF(0,0) << 0 << crop << QPointF(-25,0);
    QTest::newRow("c1-0") << QPointF(200,0) << 0 << crop << QPointF(175,0);
    QTest::newRow("c2-0") << QPointF(0,100) << 0 << crop << QPointF(-25,100);
    QTest::newRow("c3-0") << QPointF(200,100) << 0 << crop << QPointF(175,100);
    QTest::newRow("c4-0") << QPointF(100,50) << 0 << stretch << QPointF(75,50);
    QTest::newRow("c5-0") << QPointF(40,80) << 0 << stretch << QPointF(30,80);

    // 90 degrees (anti clockwise)
    QTest::newRow("s0-90") << QPointF(0,0) << 90 << stretch << QPointF(0,100);
    QTest::newRow("s1-90") << QPointF(200,0) << 90 << stretch << QPointF(0,0);
    QTest::newRow("s2-90") << QPointF(0,100) << 90 << stretch << QPointF(150,100);
    QTest::newRow("s3-90") << QPointF(200,100) << 90 << stretch << QPointF(150,0);
    QTest::newRow("s4-90") << QPointF(100,50) << 90 << stretch << QPointF(75,50);
    QTest::newRow("s5-90") << QPointF(40,80) << 90 << stretch << QPointF(120,80);

    QTest::newRow("f0-90") << QPointF(0,0) << 90 << fit << QPointF(50,100);
    QTest::newRow("f1-90") << QPointF(200,0) << 90 << fit << QPointF(50,0);
    QTest::newRow("f2-90") << QPointF(0,100) << 90 << fit << QPointF(100,100);
    QTest::newRow("f3-90") << QPointF(200,100) << 90 << fit << QPointF(100,0);
    QTest::newRow("f4-90") << QPointF(100,50) << 90 << fit << QPointF(75,50);
    QTest::newRow("f5-90") << QPointF(40,80) << 90 << fit << QPointF(90,80);

    QTest::newRow("c0-90") << QPointF(0,0) << 90 << crop << QPointF(0,200);
    QTest::newRow("c1-90") << QPointF(200,0) << 90 << crop << QPointF(0,-100);
    QTest::newRow("c2-90") << QPointF(0,100) << 90 << crop << QPointF(150,200);
    QTest::newRow("c3-90") << QPointF(200,100) << 90 << crop << QPointF(150,-100);
    QTest::newRow("c4-90") << QPointF(100,50) << 90 << crop << QPointF(75,50);
    QTest::newRow("c5-90") << QPointF(40,80) << 90 << crop << QPointF(120,140);

    // 180
    QTest::newRow("s0-180") << QPointF(0,0) << 180 << stretch << QPointF(150,100);
    QTest::newRow("s1-180") << QPointF(200,0) << 180 << stretch << QPointF(0,100);
    QTest::newRow("s2-180") << QPointF(0,100) << 180 << stretch << QPointF(150,0);
    QTest::newRow("s3-180") << QPointF(200,100) << 180 << stretch << QPointF(0,0);
    QTest::newRow("s4-180") << QPointF(100,50) << 180 << stretch << QPointF(75,50);
    QTest::newRow("s5-180") << QPointF(40,80) << 180 << stretch << QPointF(120,20);

    QTest::newRow("f0-180") << QPointF(0,0) << 180 << fit << QPointF(150,87.5f);
    QTest::newRow("f1-180") << QPointF(200,0) << 180 << fit << QPointF(0,87.5f);
    QTest::newRow("f2-180") << QPointF(0,100) << 180 << fit << QPointF(150,12.5f);
    QTest::newRow("f3-180") << QPointF(200,100) << 180 << fit << QPointF(0,12.5f);
    QTest::newRow("f4-180") << QPointF(100,50) << 180 << fit << QPointF(75,50);
    QTest::newRow("f5-180") << QPointF(40,80) << 180 << fit << QPointF(120,27.5f);

    QTest::newRow("c0-180") << QPointF(0,0) << 180 << crop << QPointF(175,100);
    QTest::newRow("c1-180") << QPointF(200,0) << 180 << crop << QPointF(-25,100);
    QTest::newRow("c2-180") << QPointF(0,100) << 180 << crop << QPointF(175,0);
    QTest::newRow("c3-180") << QPointF(200,100) << 180 << crop << QPointF(-25,0);
    QTest::newRow("c4-180") << QPointF(100,50) << 180 << crop << QPointF(75,50);
    QTest::newRow("c5-180") << QPointF(40,80) << 180 << crop << QPointF(135,20);

    // 270
    QTest::newRow("s0-270") << QPointF(0,0) << 270 << stretch << QPointF(150,0);
    QTest::newRow("s1-270") << QPointF(200,0) << 270 << stretch << QPointF(150,100);
    QTest::newRow("s2-270") << QPointF(0,100) << 270 << stretch << QPointF(0,0);
    QTest::newRow("s3-270") << QPointF(200,100) << 270 << stretch << QPointF(0,100);
    QTest::newRow("s4-270") << QPointF(100,50) << 270 << stretch << QPointF(75,50);
    QTest::newRow("s5-270") << QPointF(40,80) << 270 << stretch << QPointF(30,20);

    QTest::newRow("f0-270") << QPointF(0,0) << 270 << fit << QPointF(100,0);
    QTest::newRow("f1-270") << QPointF(200,0) << 270 << fit << QPointF(100,100);
    QTest::newRow("f2-270") << QPointF(0,100) << 270 << fit << QPointF(50,0);
    QTest::newRow("f3-270") << QPointF(200,100) << 270 << fit << QPointF(50,100);
    QTest::newRow("f4-270") << QPointF(100,50) << 270 << fit << QPointF(75,50);
    QTest::newRow("f5-270") << QPointF(40,80) << 270 << fit << QPointF(60,20);

    QTest::newRow("c0-270") << QPointF(0,0) << 270 << crop << QPointF(150,-100);
    QTest::newRow("c1-270") << QPointF(200,0) << 270 << crop << QPointF(150,200);
    QTest::newRow("c2-270") << QPointF(0,100) << 270 << crop << QPointF(0,-100);
    QTest::newRow("c3-270") << QPointF(200,100) << 270 << crop << QPointF(0,200);
    QTest::newRow("c4-270") << QPointF(100,50) << 270 << crop << QPointF(75,50);
    QTest::newRow("c5-270") << QPointF(40,80) << 270 << crop << QPointF(30,-40);
}

/* Test all rectangle mapping */
void tst_QDeclarativeVideoOutput::mappingRect()
{
    QFETCH(QRectF, rect);
    QFETCH(int, orientation);
    QFETCH(QDeclarativeVideoOutput::FillMode, fillMode);
    QFETCH(QRectF, expected);

    QVERIFY(m_mappingOutput);
    m_mappingOutput->setProperty("orientation", QVariant(orientation));
    m_mappingOutput->setProperty("fillMode", QVariant::fromValue(fillMode));

    updateOutputGeometry(m_mappingOutput);

    QRectF output = invokeR2R(m_mappingOutput, "mapRectToItem", rect);
    QRectF reverse = invokeR2R(m_mappingOutput, "mapRectToSource", output);

    QCOMPARE(output, expected);
    QCOMPARE(reverse, rect);

    // Now the normalized versions
    // Source rectangle is 200x100
    QRectF normal(rect.x() / 200, rect.y() / 100, rect.width() / 200, rect.height() / 100);

    output = invokeR2R(m_mappingOutput, "mapNormalizedRectToItem", normal);
    reverse = invokeR2R(m_mappingOutput, "mapRectToSourceNormalized", output);

    QCOMPARE(output, expected);
    QCOMPARE(reverse, normal);
}

void tst_QDeclarativeVideoOutput::mappingRect_data()
{
    QTest::addColumn<QRectF>("rect");
    QTest::addColumn<int>("orientation");
    QTest::addColumn<QDeclarativeVideoOutput::FillMode>("fillMode");
    QTest::addColumn<QRectF>("expected");

    // First make sure the component has processed the frame
    QCOMPARE(m_mappingOutput->property("sourceRect").toRectF(), QRectF(0,0,200,100));

    QDeclarativeVideoOutput::FillMode stretch = QDeclarativeVideoOutput::Stretch;
    QDeclarativeVideoOutput::FillMode fit = QDeclarativeVideoOutput::PreserveAspectFit;
    QDeclarativeVideoOutput::FillMode crop = QDeclarativeVideoOutput::PreserveAspectCrop;

    // Full rectangle mapping
    // Stretch
    QTest::newRow("s0")   << QRectF(0,0, 200, 100) << 0 << stretch << QRectF(0,0,150,100);
    QTest::newRow("s90")  << QRectF(0,0, 200, 100) << 90 << stretch << QRectF(0,0,150,100);
    QTest::newRow("s180") << QRectF(0,0, 200, 100) << 180 << stretch << QRectF(0,0,150,100);
    QTest::newRow("s270") << QRectF(0,0, 200, 100) << 270 << stretch << QRectF(0,0,150,100);

    // Fit
    QTest::newRow("f0")   << QRectF(0,0, 200, 100) << 0 << fit << QRectF(0,12.5f,150,75);
    QTest::newRow("f90") << QRectF(0,0, 200, 100) << 90 << fit << QRectF(50,0,50,100);
    QTest::newRow("f180") << QRectF(0,0, 200, 100) << 180 << fit << QRectF(0,12.5f,150,75);
    QTest::newRow("f270")  << QRectF(0,0, 200, 100) << 270 << fit << QRectF(50,0,50,100);

    // Crop
    QTest::newRow("c0")   << QRectF(0,0, 200, 100) << 0 << crop << QRectF(-25,0,200,100);
    QTest::newRow("c90") << QRectF(0,0, 200, 100) << 90 << crop << QRectF(0,-100,150,300);
    QTest::newRow("c180") << QRectF(0,0, 200, 100) << 180 << crop << QRectF(-25,0,200,100);
    QTest::newRow("c270")  << QRectF(0,0, 200, 100) << 270 << crop << QRectF(0,-100,150,300);

    // Partial rectangle mapping
    // Stretch
    // 50-130 in x (0.25 - 0.65), 25-50 (0.25 - 0.5) in y (out of 200, 100) -> 150x100
    QTest::newRow("p-s0")   << QRectF(50, 25, 80, 25) << 0 << stretch << QRectF(37.5f,25,60,25);
    QTest::newRow("p-s90") << QRectF(50, 25, 80, 25) << 90 << stretch << QRectF(37.5f,35,37.5f,40);
    QTest::newRow("p-s180") << QRectF(50, 25, 80, 25) << 180 << stretch << QRectF(52.5f,50,60,25);
    QTest::newRow("p-s270")  << QRectF(50, 25, 80, 25) << 270 << stretch << QRectF(75,25,37.5f,40);

    // Fit
    QTest::newRow("p-f0")   << QRectF(50, 25, 80, 25) << 0 << fit << QRectF(37.5f,31.25f,60,18.75f);
    QTest::newRow("p-f90")  << QRectF(50, 25, 80, 25) << 90 << fit << QRectF(62.5f,35,12.5f,40);
    QTest::newRow("p-f180") << QRectF(50, 25, 80, 25) << 180 << fit << QRectF(52.5f,50,60,18.75f);
    QTest::newRow("p-f270") << QRectF(50, 25, 80, 25) << 270 << fit << QRectF(75,25,12.5f,40);

    // Crop
    QTest::newRow("p-c0")   << QRectF(50, 25, 80, 25) << 0 << crop << QRectF(25,25,80,25);
    QTest::newRow("p-c90")  << QRectF(50, 25, 80, 25) << 90 << crop << QRectF(37.5f,5,37.5f,120);
    QTest::newRow("p-c180") << QRectF(50, 25, 80, 25) << 180 << crop << QRectF(45,50,80,25);
    QTest::newRow("p-c270") << QRectF(50, 25, 80, 25) << 270 << crop << QRectF(75,-25,37.5f,120);
}

void tst_QDeclarativeVideoOutput::updateOutputGeometry(QObject *output)
{
    // Since the object isn't visible, update() doesn't do anything
    // so we manually force this
    QMetaObject::invokeMethod(output, "_q_updateGeometry");
}

void tst_QDeclarativeVideoOutput::contentRect()
{
    QFETCH(int, orientation);
    QFETCH(QDeclarativeVideoOutput::FillMode, fillMode);
    QFETCH(QRectF, expected);

    QVERIFY(m_mappingOutput);
    m_mappingOutput->setProperty("orientation", QVariant(orientation));
    m_mappingOutput->setProperty("fillMode", QVariant::fromValue(fillMode));

    updateOutputGeometry(m_mappingOutput);

    QRectF output = m_mappingOutput->property("contentRect").toRectF();
    QCOMPARE(output, expected);
}

void tst_QDeclarativeVideoOutput::contentRect_data()
{
    QTest::addColumn<int>("orientation");
    QTest::addColumn<QDeclarativeVideoOutput::FillMode>("fillMode");
    QTest::addColumn<QRectF>("expected");

    // First make sure the component has processed the frame
    QCOMPARE(m_mappingOutput->property("sourceRect").toRectF(), QRectF(0,0,200,100));

    QDeclarativeVideoOutput::FillMode stretch = QDeclarativeVideoOutput::Stretch;
    QDeclarativeVideoOutput::FillMode fit = QDeclarativeVideoOutput::PreserveAspectFit;
    QDeclarativeVideoOutput::FillMode crop = QDeclarativeVideoOutput::PreserveAspectCrop;

    // Stretch just keeps the full render rect regardless of orientation
    QTest::newRow("s0") << 0 << stretch << QRectF(0,0,150,100);
    QTest::newRow("s90") << 90 << stretch << QRectF(0,0,150,100);
    QTest::newRow("s180") << 180 << stretch << QRectF(0,0,150,100);
    QTest::newRow("s270") << 270 << stretch << QRectF(0,0,150,100);

    // Fit depends on orientation
    // Source is 200x100, fitting in 150x100 -> 150x75
    // or 100x200 -> 50x100
    QTest::newRow("f0") << 0 << fit << QRectF(0,12.5f,150,75);
    QTest::newRow("f90") << 90 << fit << QRectF(50,0,50,100);
    QTest::newRow("f180") << 180 << fit << QRectF(0,12.5,150,75);
    QTest::newRow("f270") << 270 << fit << QRectF(50,0,50,100);

    // Crop also depends on orientation, may go outside render rect
    // 200x100 -> -25,0 200x100
    // 100x200 -> 0,-100 150x300
    QTest::newRow("c0") << 0 << crop << QRectF(-25,0,200,100);
    QTest::newRow("c90") << 90 << crop << QRectF(0,-100,150,300);
    QTest::newRow("c180") << 180 << crop << QRectF(-25,0,200,100);
    QTest::newRow("c270") << 270 << crop << QRectF(0,-100,150,300);
}


QRectF tst_QDeclarativeVideoOutput::invokeR2R(QObject *object, const char *signature, const QRectF &rect)
{
    QRectF r;
    QMetaObject::invokeMethod(object, signature, Q_RETURN_ARG(QRectF, r), Q_ARG(QRectF, rect));
    return r;
}

QPointF tst_QDeclarativeVideoOutput::invokeP2P(QObject *object, const char *signature, const QPointF &point)
{
    QPointF p;
    QMetaObject::invokeMethod(object, signature, Q_RETURN_ARG(QPointF, p), Q_ARG(QPointF, point));
    return p;
}


QTEST_MAIN(tst_QDeclarativeVideoOutput)

#include "tst_qdeclarativevideooutput.moc"
