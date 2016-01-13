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

//TESTED_COMPONENT=src/multimedia

#include <QtTest/QtTest>

#include "qaudio.h"

// Adds an enum, and the stringized version
#define ADD_ENUM_TEST(x) \
    QTest::newRow(#x) \
        << QAudio::x \
    << QString(QLatin1String(#x));

class tst_QAudioNamespace : public QObject
{
    Q_OBJECT

private slots:
    void debugError();
    void debugError_data();
    void debugState();
    void debugState_data();
    void debugMode();
    void debugMode_data();
};

void tst_QAudioNamespace::debugError_data()
{
    QTest::addColumn<QAudio::Error>("error");
    QTest::addColumn<QString>("stringized");

    ADD_ENUM_TEST(NoError);
    ADD_ENUM_TEST(OpenError);
    ADD_ENUM_TEST(IOError);
    ADD_ENUM_TEST(UnderrunError);
    ADD_ENUM_TEST(FatalError);
}

void tst_QAudioNamespace::debugError()
{
    QFETCH(QAudio::Error, error);
    QFETCH(QString, stringized);

    QTest::ignoreMessage(QtDebugMsg, stringized.toLatin1().constData());
    qDebug() << error;
}

void tst_QAudioNamespace::debugState_data()
{
    QTest::addColumn<QAudio::State>("state");
    QTest::addColumn<QString>("stringized");

    ADD_ENUM_TEST(ActiveState);
    ADD_ENUM_TEST(SuspendedState);
    ADD_ENUM_TEST(StoppedState);
    ADD_ENUM_TEST(IdleState);
}

void tst_QAudioNamespace::debugState()
{
    QFETCH(QAudio::State, state);
    QFETCH(QString, stringized);

    QTest::ignoreMessage(QtDebugMsg, stringized.toLatin1().constData());
    qDebug() << state;
}

void tst_QAudioNamespace::debugMode_data()
{
    QTest::addColumn<QAudio::Mode>("mode");
    QTest::addColumn<QString>("stringized");

    ADD_ENUM_TEST(AudioInput);
    ADD_ENUM_TEST(AudioOutput);
}

void tst_QAudioNamespace::debugMode()
{
    QFETCH(QAudio::Mode, mode);
    QFETCH(QString, stringized);

    QTest::ignoreMessage(QtDebugMsg, stringized.toLatin1().constData());
    qDebug() << mode;
}
QTEST_MAIN(tst_QAudioNamespace)

#include "tst_qaudionamespace.moc"
