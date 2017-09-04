/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Speech module of the Qt Toolkit.
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


#include <QTest>
#include <QTextToSpeech>
#include <QSignalSpy>

#if defined(HAVE_SPEECHD)
    #include <speech-dispatcher/libspeechd.h>
    #if LIBSPEECHD_MAJOR_VERSION == 0 && LIBSPEECHD_MINOR_VERSION < 9
        #define HAVE_SPEECHD_BEFORE_090
    #endif
#endif

class tst_QTextToSpeech : public QObject
{
    Q_OBJECT

private slots:
    void say_hello();
    void speech_rate();
    void pitch();
    void set_voice();
};


void tst_QTextToSpeech::say_hello()
{
    QString text = QStringLiteral("this is an example text");
    QTextToSpeech tts;
    QCOMPARE(tts.state(), QTextToSpeech::Ready);

    QElapsedTimer timer;
    timer.start();
    tts.say(text);
    QTRY_COMPARE(tts.state(), QTextToSpeech::Speaking);
    QSignalSpy spy(&tts, SIGNAL(stateChanged(QTextToSpeech::State)));
    spy.wait(10000);
    QCOMPARE(tts.state(), QTextToSpeech::Ready);
    QVERIFY(timer.elapsed() > 100);
}

void tst_QTextToSpeech::speech_rate()
{
    QString text = QStringLiteral("this is an example text");
    QTextToSpeech tts;
    tts.setRate(0.5);
    QCOMPARE(tts.state(), QTextToSpeech::Ready);
#ifndef HAVE_SPEECHD_BEFORE_090
    QCOMPARE(tts.rate(), 0.5);
#endif

    qint64 lastTime = 0;
    // check that speaking at slower rate takes more time (for 0.5, 0.0, -0.5)
    for (int i = 1; i >= -1; --i) {
        tts.setRate(i * 0.5);
        QElapsedTimer timer;
        timer.start();
        tts.say(text);
        QTRY_COMPARE(tts.state(), QTextToSpeech::Speaking);
        QSignalSpy spy(&tts, SIGNAL(stateChanged(QTextToSpeech::State)));
        spy.wait(10000);
        QCOMPARE(tts.state(), QTextToSpeech::Ready);
        qint64 time = timer.elapsed();
        QVERIFY(time > lastTime);
        lastTime = time;
    }
}

void tst_QTextToSpeech::pitch()
{
    QTextToSpeech tts;
    for (int i = -10; i <= 10; ++i) {
        tts.setPitch(i / 10.0);
#ifndef HAVE_SPEECHD_BEFORE_090
        QCOMPARE(tts.pitch(), i / 10.0);
#endif
    }
}

void tst_QTextToSpeech::set_voice()
{
    QString text = QStringLiteral("this is an example text");
    QTextToSpeech tts;
    QCOMPARE(tts.state(), QTextToSpeech::Ready);

    // Choose a voice
    QVector<QVoice> voices = tts.availableVoices();
    int vId = 0;
    QVERIFY(voices.length()); // have at least one voice
    if (voices.length() > 1) {
        vId = 1;
    }
    tts.setVoice(voices[vId]);
    QCOMPARE(tts.state(), QTextToSpeech::Ready);

    QElapsedTimer timer;
    timer.start();
    tts.say(text);
    QTRY_COMPARE(tts.state(), QTextToSpeech::Speaking);
    QSignalSpy spy(&tts, SIGNAL(stateChanged(QTextToSpeech::State)));
    spy.wait(10000);
    QCOMPARE(tts.state(), QTextToSpeech::Ready);
    QVERIFY(timer.elapsed() > 100);
}

QTEST_MAIN(tst_QTextToSpeech)
#include "tst_qtexttospeech.moc"
