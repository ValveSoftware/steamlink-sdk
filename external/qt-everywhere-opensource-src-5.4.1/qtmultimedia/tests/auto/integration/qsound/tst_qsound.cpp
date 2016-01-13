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
#include <QtCore/QString>
#include <QSound>
#include <QSoundEffect>

class tst_QSound : public QObject
{
   Q_OBJECT

public:
   tst_QSound( QObject* parent=0) : QObject(parent) {}

private slots:
   void initTestCase();
   void cleanupTestCase();
   void testLooping();
   void testPlay();
   void testStop();

   void testStaticPlay();

private:
    QSound* sound;
};


void tst_QSound::initTestCase()
{
    sound = 0;
    // Only perform tests if audio device exists
    QStringList mimeTypes = QSoundEffect::supportedMimeTypes();
    if (mimeTypes.empty())
        QSKIP("No audio devices available");

    const QString testFileName = QStringLiteral("test.wav");
    const QString fullPath = QFINDTESTDATA(testFileName);
    QVERIFY2(!fullPath.isEmpty(), qPrintable(QStringLiteral("Unable to locate ") + testFileName));
    sound = new QSound(fullPath, this);

    QVERIFY(!sound->fileName().isEmpty());
    QCOMPARE(sound->loops(),1);
}

void tst_QSound::cleanupTestCase()
{
    if (sound)
    {
        delete sound;
        sound = NULL;
    }
}

void tst_QSound::testLooping()
{
    sound->setLoops(5);
    QCOMPARE(sound->loops(),5);

    sound->play();
    QVERIFY(!sound->isFinished());

    // test.wav is about 200ms, wait until it has finished playing 5 times
    QTest::qWait(3000);

    QVERIFY(sound->isFinished());
    QCOMPARE(sound->loopsRemaining(),0);
}

void tst_QSound::testPlay()
{
    sound->setLoops(1);
    sound->play();
    QVERIFY(!sound->isFinished());
    QTest::qWait(1000);
    QVERIFY(sound->isFinished());
}

void tst_QSound::testStop()
{
    sound->setLoops(10);
    sound->play();
    QVERIFY(!sound->isFinished());
    QTest::qWait(1000);
    sound->stop();
    QTest::qWait(1000);
    QVERIFY(sound->isFinished());
}

void tst_QSound::testStaticPlay()
{
    // Check that you hear sound with static play also.
    const QString testFileName = QStringLiteral("test.wav");
    const QString fullPath = QFINDTESTDATA(testFileName);
    QVERIFY2(!fullPath.isEmpty(), qPrintable(QStringLiteral("Unable to locate ") + testFileName));

    QSound::play(fullPath);

    QTest::qWait(1000);
}

QTEST_MAIN(tst_QSound);
#include "tst_qsound.moc"
