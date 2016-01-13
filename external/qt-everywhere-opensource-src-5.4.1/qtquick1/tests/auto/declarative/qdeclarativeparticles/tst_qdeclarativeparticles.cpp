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
#include <QtTest/QtTest>
#include <QtTest/QSignalSpy>
#include <qdeclarativeview.h>
#include <QDeclarativeError>
#include <QGraphicsObject>

class tst_QDeclarativeParticles : public QObject
{
    Q_OBJECT
public:
    tst_QDeclarativeParticles();

private slots:
    void properties();
    void motionGravity();
    void motionWander();
    void runs();
private:
    QDeclarativeView *createView(const QString &filename);

};

tst_QDeclarativeParticles::tst_QDeclarativeParticles()
{
}

static inline QByteArray msgDeclarativeErrors(const QList<QDeclarativeError> &errors)
{
    QString result;
    QDebug debug(&result);
    foreach (const QDeclarativeError &error, errors)
        debug << error << '\n';
    return result.toLocal8Bit();
}

void tst_QDeclarativeParticles::properties()
{
    QDeclarativeView *canvas = createView(SRCDIR "/data/particlestest.qml");
    QVERIFY2(canvas->rootObject(), msgDeclarativeErrors(canvas->errors()).constData());

    QObject* particles = canvas->rootObject()->findChild<QObject*>("particles");
    QVERIFY(particles);

    particles->setProperty("source", QUrl::fromLocalFile(SRCDIR "/data/particle.png"));
    QCOMPARE(particles->property("source").toUrl(), QUrl::fromLocalFile(SRCDIR "/data/particle.png"));

    particles->setProperty("lifeSpanDeviation", (1000));
    QCOMPARE(particles->property("lifeSpanDeviation").toInt(), 1000);

    particles->setProperty("fadeInDuration", 1000);
    QCOMPARE(particles->property("fadeInDuration").toInt(), 1000);

    particles->setProperty("fadeOutDuration", 1000);
    QCOMPARE(particles->property("fadeOutDuration").toInt(), 1000);

    particles->setProperty("angle", 100.0);
    QCOMPARE(particles->property("angle").toDouble(), 100.0);

    particles->setProperty("angleDeviation", 100.0);
    QCOMPARE(particles->property("angleDeviation").toDouble(), 100.0);

    particles->setProperty("velocity", 100.0);
    QCOMPARE(particles->property("velocity").toDouble(), 100.0);

    particles->setProperty("velocityDeviation", 100.0);
    QCOMPARE(particles->property("velocityDeviation").toDouble(), 100.0);

    particles->setProperty("emissionVariance", 0.5);
    QCOMPARE(particles->property("emissionVariance").toDouble(),0.5);

    particles->setProperty("emissionRate", 12);
    QCOMPARE(particles->property("emissionRate").toInt(), 12);
}

void tst_QDeclarativeParticles::motionGravity()
{
    QDeclarativeView *canvas = createView(SRCDIR "/data/particlemotiontest.qml");
    QVERIFY2(canvas->rootObject(), msgDeclarativeErrors(canvas->errors()).constData());

    QObject* particles = canvas->rootObject()->findChild<QObject*>("particles");
    QVERIFY(particles);

    QObject* motionGravity = canvas->rootObject()->findChild<QObject*>("motionGravity");
    //QCOMPARE(qvariant_cast<QObject*>(particles->property("motion")), motionGravity);

    QSignalSpy xattractorSpy(motionGravity, SIGNAL(xattractorChanged()));
    QSignalSpy yattractorSpy(motionGravity, SIGNAL(yattractorChanged()));
    QSignalSpy accelerationSpy(motionGravity, SIGNAL(accelerationChanged()));

    QCOMPARE(motionGravity->property("xattractor").toDouble(), 0.0);
    QCOMPARE(motionGravity->property("yattractor").toDouble(), 1000.0);
    QCOMPARE(motionGravity->property("acceleration").toDouble(), 25.0);

    motionGravity->setProperty("xattractor", 20.0);
    motionGravity->setProperty("yattractor", 10.0);
    motionGravity->setProperty("acceleration", 10.0);

    QCOMPARE(motionGravity->property("xattractor").toDouble(), 20.0);
    QCOMPARE(motionGravity->property("yattractor").toDouble(), 10.0);
    QCOMPARE(motionGravity->property("acceleration").toDouble(), 10.0);

    QCOMPARE(xattractorSpy.count(), 1);
    QCOMPARE(yattractorSpy.count(), 1);
    QCOMPARE(accelerationSpy.count(), 1);

    motionGravity->setProperty("xattractor", 20.0);
    motionGravity->setProperty("yattractor", 10.0);
    motionGravity->setProperty("acceleration", 10.0);

    QCOMPARE(xattractorSpy.count(), 1);
    QCOMPARE(yattractorSpy.count(), 1);
    QCOMPARE(accelerationSpy.count(), 1);
}

void tst_QDeclarativeParticles::motionWander()
{
    QDeclarativeView *canvas = createView(SRCDIR "/data/particlemotiontest.qml");
    QVERIFY2(canvas->rootObject(), msgDeclarativeErrors(canvas->errors()).constData());

    QObject* particles = canvas->rootObject()->findChild<QObject*>("particles");
    QVERIFY(particles);

    QSignalSpy motionSpy(particles, SIGNAL(motionChanged()));
    QObject* motionWander = canvas->rootObject()->findChild<QObject*>("motionWander");

    QCOMPARE(motionSpy.count(), 0);
    particles->setProperty("motion", QVariant::fromValue(motionWander));
    //QCOMPARE(particles->property("motion"), QVariant::fromValue(motionWander));
    //QCOMPARE(motionSpy.count(), 1);

    particles->setProperty("motion", QVariant::fromValue(motionWander));
    //QCOMPARE(motionSpy.count(), 1);

    QSignalSpy xvarianceSpy(motionWander, SIGNAL(xvarianceChanged()));
    QSignalSpy yvarianceSpy(motionWander, SIGNAL(yvarianceChanged()));
    QSignalSpy paceSpy(motionWander, SIGNAL(paceChanged()));

    QCOMPARE(motionWander->property("xvariance").toDouble(), 30.0);
    QCOMPARE(motionWander->property("yvariance").toDouble(), 30.0);
    QCOMPARE(motionWander->property("pace").toDouble(), 100.0);

    motionWander->setProperty("xvariance", 20.0);
    motionWander->setProperty("yvariance", 10.0);
    motionWander->setProperty("pace", 10.0);

    QCOMPARE(motionWander->property("xvariance").toDouble(), 20.0);
    QCOMPARE(motionWander->property("yvariance").toDouble(), 10.0);
    QCOMPARE(motionWander->property("pace").toDouble(), 10.0);

    QCOMPARE(xvarianceSpy.count(), 1);
    QCOMPARE(yvarianceSpy.count(), 1);
    QCOMPARE(paceSpy.count(), 1);

    QCOMPARE(motionWander->property("xvariance").toDouble(), 20.0);
    QCOMPARE(motionWander->property("yvariance").toDouble(), 10.0);
    QCOMPARE(motionWander->property("pace").toDouble(), 10.0);

    QCOMPARE(xvarianceSpy.count(), 1);
    QCOMPARE(yvarianceSpy.count(), 1);
    QCOMPARE(paceSpy.count(), 1);
}

void tst_QDeclarativeParticles::runs()
{
    QDeclarativeView *canvas = createView(SRCDIR "/data/particlestest.qml");
    QVERIFY2(canvas->rootObject(), msgDeclarativeErrors(canvas->errors()).constData());

    QObject* particles = canvas->rootObject()->findChild<QObject*>("particles");
    QVERIFY(particles);
    QTest::qWait(1000);//Run for one second. Test passes if it doesn't crash.
}

QDeclarativeView *tst_QDeclarativeParticles::createView(const QString &filename)
{
    QDeclarativeView *canvas = new QDeclarativeView(0);
    canvas->setFixedSize(240,320);

    canvas->setSource(QUrl::fromLocalFile(filename));

    return canvas;
}
QTEST_MAIN(tst_QDeclarativeParticles)

#include "tst_qdeclarativeparticles.moc"
