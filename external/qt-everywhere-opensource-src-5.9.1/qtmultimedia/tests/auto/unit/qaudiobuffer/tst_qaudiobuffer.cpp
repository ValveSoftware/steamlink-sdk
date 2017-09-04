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

#include <QtCore/QString>
#include <QtTest/QtTest>

#include <qaudiobuffer.h>

class tst_QAudioBuffer : public QObject
{
    Q_OBJECT

public:
    tst_QAudioBuffer();
    ~tst_QAudioBuffer();

private Q_SLOTS:
    void ctors();
    void assign();
    void constData() const;
    void data_const() const;
    void data();
    void durations();
    void durations_data();
    void stereoSample();

private:
    QAudioFormat mFormat;
    QAudioBuffer *mNull;
    QAudioBuffer *mEmpty;
    QAudioBuffer *mFromArray;
};

tst_QAudioBuffer::tst_QAudioBuffer()
{
    // Initialize some common buffers
    mFormat.setChannelCount(2);
    mFormat.setSampleSize(16);
    mFormat.setSampleType(QAudioFormat::UnSignedInt);
    mFormat.setSampleRate(10000);
    mFormat.setCodec("audio/pcm");

    QByteArray b(4000, char(0x80));
    mNull = new QAudioBuffer;
    mEmpty = new QAudioBuffer(500, mFormat); // 500 stereo frames of 16 bits -> 2KB
    mFromArray = new QAudioBuffer(b, mFormat);
}


tst_QAudioBuffer::~tst_QAudioBuffer()
{
    delete mNull;
    delete mEmpty;
    delete mFromArray;
}

void tst_QAudioBuffer::ctors()
{
    // Null buffer
    QVERIFY(!mNull->isValid());
    QVERIFY(mNull->constData() == 0);
    QVERIFY(mNull->data() == 0);
    QVERIFY(((const QAudioBuffer*)mNull)->data() == 0);
    QCOMPARE(mNull->duration(), 0LL);
    QCOMPARE(mNull->byteCount(), 0);
    QCOMPARE(mNull->sampleCount(), 0);
    QCOMPARE(mNull->frameCount(), 0);
    QCOMPARE(mNull->startTime(), -1LL);

    // Empty buffer
    QVERIFY(mEmpty->isValid());
    QVERIFY(mEmpty->constData() != 0);
    QVERIFY(mEmpty->data() != 0);
    QVERIFY(((const QAudioBuffer*)mEmpty)->data() != 0);
    QCOMPARE(mEmpty->sampleCount(), 1000);
    QCOMPARE(mEmpty->frameCount(), 500);
    QCOMPARE(mEmpty->duration(), 50000LL);
    QCOMPARE(mEmpty->byteCount(), 2000);
    QCOMPARE(mEmpty->startTime(), -1LL);

    // bytearray buffer
    QVERIFY(mFromArray->isValid());
    QVERIFY(mFromArray->constData() != 0);
    QVERIFY(mFromArray->data() != 0);
    QVERIFY(((const QAudioBuffer*)mFromArray)->data() != 0);
    /// 4000 bytes at 10KHz, 2ch, 16bit = 40kBps -> 0.1s
    QCOMPARE(mFromArray->duration(), 100000LL);
    QCOMPARE(mFromArray->byteCount(), 4000);
    QCOMPARE(mFromArray->sampleCount(), 2000);
    QCOMPARE(mFromArray->frameCount(), 1000);
    QCOMPARE(mFromArray->startTime(), -1LL);


    // Now some invalid buffers
    QAudioBuffer badFormat(1000, QAudioFormat());
    QVERIFY(!badFormat.isValid());
    QVERIFY(badFormat.constData() == 0);
    QVERIFY(badFormat.data() == 0);
    QVERIFY(((const QAudioBuffer*)&badFormat)->data() == 0);
    QCOMPARE(badFormat.duration(), 0LL);
    QCOMPARE(badFormat.byteCount(), 0);
    QCOMPARE(badFormat.sampleCount(), 0);
    QCOMPARE(badFormat.frameCount(), 0);
    QCOMPARE(badFormat.startTime(), -1LL);

    QAudioBuffer badArray(QByteArray(), mFormat);
    QVERIFY(!badArray.isValid());
    QVERIFY(badArray.constData() == 0);
    QVERIFY(badArray.data() == 0);
    QVERIFY(((const QAudioBuffer*)&badArray)->data() == 0);
    QCOMPARE(badArray.duration(), 0LL);
    QCOMPARE(badArray.byteCount(), 0);
    QCOMPARE(badArray.sampleCount(), 0);
    QCOMPARE(badArray.frameCount(), 0);
    QCOMPARE(badArray.startTime(), -1LL);

    QAudioBuffer badBoth = QAudioBuffer(QByteArray(), QAudioFormat());
    QVERIFY(!badBoth.isValid());
    QVERIFY(badBoth.constData() == 0);
    QVERIFY(badBoth.data() == 0);
    QVERIFY(((const QAudioBuffer*)&badBoth)->data() == 0);
    QCOMPARE(badBoth.duration(), 0LL);
    QCOMPARE(badBoth.byteCount(), 0);
    QCOMPARE(badBoth.sampleCount(), 0);
    QCOMPARE(badBoth.frameCount(), 0);
    QCOMPARE(badBoth.startTime(), -1LL);
}

void tst_QAudioBuffer::assign()
{
    // TODO Needs strong behaviour definition
}

void tst_QAudioBuffer::constData() const
{
    const void *data = mEmpty->constData();
    QVERIFY(data != 0);

    const unsigned int *idata = reinterpret_cast<const unsigned int*>(data);
    QCOMPARE(*idata, 0U);

    const QAudioBuffer::S8U *sdata = mEmpty->constData<QAudioBuffer::S8U>();
    QVERIFY(sdata);
    QCOMPARE(sdata->left, (unsigned char)0);
    QCOMPARE(sdata->right, (unsigned char)0);

    // The bytearray one should be 0x80
    data = mFromArray->constData();
    QVERIFY(data != 0);

    idata = reinterpret_cast<const unsigned int *>(data);
    QEXPECT_FAIL("", "Unsigned 16bits are cleared to 0x8080 currently", Continue);
    QCOMPARE(*idata, 0x80008000);

    sdata = mFromArray->constData<QAudioBuffer::S8U>();
    QCOMPARE(sdata->left, (unsigned char)0x80);
    QCOMPARE(sdata->right, (unsigned char)0x80);
}

void tst_QAudioBuffer::data_const() const
{
    const void *data = ((const QAudioBuffer*)mEmpty)->data();
    QVERIFY(data != 0);

    const unsigned int *idata = reinterpret_cast<const unsigned int*>(data);
    QCOMPARE(*idata, 0U);

    const QAudioBuffer::S8U *sdata = ((const QAudioBuffer*)mEmpty)->constData<QAudioBuffer::S8U>();
    QVERIFY(sdata);
    QCOMPARE(sdata->left, (unsigned char)0);
    QCOMPARE(sdata->right, (unsigned char)0);

    // The bytearray one should be 0x80
    data = ((const QAudioBuffer*)mFromArray)->data();
    QVERIFY(data != 0);

    idata = reinterpret_cast<const unsigned int *>(data);
    QEXPECT_FAIL("", "Unsigned 16bits are cleared to 0x8080 currently", Continue);
    QCOMPARE(*idata, 0x80008000);

    sdata = ((const QAudioBuffer*)mFromArray)->constData<QAudioBuffer::S8U>();
    QCOMPARE(sdata->left, (unsigned char)0x80);
    QCOMPARE(sdata->right, (unsigned char)0x80);
}

void tst_QAudioBuffer::data()
{
    void *data = mEmpty->data();
    QVERIFY(data != 0);

    unsigned int *idata = reinterpret_cast<unsigned int*>(data);
    QCOMPARE(*idata, 0U);

    QAudioBuffer::S8U *sdata = mEmpty->data<QAudioBuffer::S8U>();
    QVERIFY(sdata);
    QCOMPARE(sdata->left, (unsigned char)0);
    QCOMPARE(sdata->right, (unsigned char)0);

    // The bytearray one should be 0x80
    data = mFromArray->data();
    QVERIFY(data != 0);

    idata = reinterpret_cast<unsigned int *>(data);
    QEXPECT_FAIL("", "Unsigned 16bits are cleared to 0x8080 currently", Continue);
    QCOMPARE(*idata, 0x80008000);

    sdata = mFromArray->data<QAudioBuffer::S8U>();
    QCOMPARE(sdata->left, (unsigned char)0x80);
    QCOMPARE(sdata->right, (unsigned char)0x80);
}

void tst_QAudioBuffer::durations()
{
    QFETCH(int, channelCount);
    QFETCH(int, sampleSize);
    QFETCH(int, frameCount);
    int sampleCount = frameCount * channelCount;
    QFETCH(QAudioFormat::SampleType, sampleType);
    QFETCH(int, sampleRate);
    QFETCH(qint64, duration);
    QFETCH(int, byteCount);

    QAudioFormat f;
    f.setChannelCount(channelCount);
    f.setSampleType(sampleType);
    f.setSampleSize(sampleSize);
    f.setSampleRate(sampleRate);
    f.setCodec("audio/pcm");

    QAudioBuffer b(frameCount, f);

    QCOMPARE(b.frameCount(), frameCount);
    QCOMPARE(b.sampleCount(), sampleCount);
    QCOMPARE(b.duration(), duration);
    QCOMPARE(b.byteCount(), byteCount);
}

void tst_QAudioBuffer::durations_data()
{
    QTest::addColumn<int>("channelCount");
    QTest::addColumn<int>("sampleSize");
    QTest::addColumn<int>("frameCount");
    QTest::addColumn<QAudioFormat::SampleType>("sampleType");
    QTest::addColumn<int>("sampleRate");
    QTest::addColumn<qint64>("duration");
    QTest::addColumn<int>("byteCount");
    QTest::newRow("M8_1000_8K") << 1 << 8 << 1000 << QAudioFormat::UnSignedInt << 8000 << 125000LL << 1000;
    QTest::newRow("M8_2000_8K") << 1 << 8 << 2000 << QAudioFormat::UnSignedInt << 8000 << 250000LL << 2000;
    QTest::newRow("M8_1000_4K") << 1 << 8 << 1000 << QAudioFormat::UnSignedInt << 4000 << 250000LL << 1000;

    QTest::newRow("S8_1000_8K") << 2 << 8 << 500 << QAudioFormat::UnSignedInt << 8000 << 62500LL << 1000;

    QTest::newRow("SF_1000_8K") << 2 << 32 << 500 << QAudioFormat::Float << 8000 << 62500LL << 4000;

    QTest::newRow("4x128_1000_16K") << 4 << 128 << 250 << QAudioFormat::SignedInt << 16000 << 15625LL << 16000;
}

void tst_QAudioBuffer::stereoSample()
{
    // Uninitialized (should default to zero level for type)
    QAudioBuffer::S8U s8u;
    QAudioBuffer::S8S s8s;
    QAudioBuffer::S16U s16u;
    QAudioBuffer::S16S s16s;
    QAudioBuffer::S32F s32f;

    QCOMPARE(s8u.left, (unsigned char) 0x80);
    QCOMPARE(s8u.right, (unsigned char) 0x80);
    QCOMPARE(s8u.average(), (unsigned char) 0x80);

    QCOMPARE(s8s.left, (signed char) 0x00);
    QCOMPARE(s8s.right, (signed char) 0x00);
    QCOMPARE(s8s.average(), (signed char) 0x0);

    QCOMPARE(s16u.left, (unsigned short) 0x8000);
    QCOMPARE(s16u.right, (unsigned short) 0x8000);
    QCOMPARE(s16u.average(), (unsigned short) 0x8000);

    QCOMPARE(s16s.left, (signed short) 0x0);
    QCOMPARE(s16s.right, (signed short) 0x0);
    QCOMPARE(s16s.average(), (signed short) 0x0);

    QCOMPARE(s32f.left, 0.0f);
    QCOMPARE(s32f.right, 0.0f);
    QCOMPARE(s32f.average(), 0.0f);

    // Initialized
    QAudioBuffer::S8U s8u2(34, 145);
    QAudioBuffer::S8S s8s2(23, -89);
    QAudioBuffer::S16U s16u2(500, 45000);
    QAudioBuffer::S16S s16s2(-10000, 346);
    QAudioBuffer::S32F s32f2(500.7f, -123.1f);

    QCOMPARE(s8u2.left, (unsigned char) 34);
    QCOMPARE(s8u2.right, (unsigned char) 145);
    QCOMPARE(s8u2.average(), (unsigned char) 89);

    QCOMPARE(s8s2.left, (signed char) 23);
    QCOMPARE(s8s2.right,(signed char) -89);
    QCOMPARE(s8s2.average(), (signed char) -33);

    QCOMPARE(s16u2.left, (unsigned short) 500);
    QCOMPARE(s16u2.right, (unsigned short) 45000);
    QCOMPARE(s16u2.average(), (unsigned short) 22750);

    QCOMPARE(s16s2.left, (signed short) -10000);
    QCOMPARE(s16s2.right, (signed short) 346);
    QCOMPARE(s16s2.average(), (signed short) (-5000 + 173));

    QCOMPARE(s32f2.left, 500.7f);
    QCOMPARE(s32f2.right, -123.1f);
    QCOMPARE(s32f2.average(), (500.7f - 123.1f)/2);

    // Assigned
    s8u = s8u2;
    s8s = s8s2;
    s16u = s16u2;
    s16s = s16s2;
    s32f = s32f2;

    QCOMPARE(s8u.left, (unsigned char) 34);
    QCOMPARE(s8u.right, (unsigned char) 145);
    QCOMPARE(s8u.average(), (unsigned char) 89);

    QCOMPARE(s8s.left, (signed char) 23);
    QCOMPARE(s8s.right, (signed char) -89);
    QCOMPARE(s8s.average(), (signed char) -33);

    QCOMPARE(s16u.left, (unsigned short) 500);
    QCOMPARE(s16u.right, (unsigned short) 45000);
    QCOMPARE(s16u.average(), (unsigned short) 22750);

    QCOMPARE(s16s.left, (signed short) -10000);
    QCOMPARE(s16s.right, (signed short) 346);
    QCOMPARE(s16s.average(), (signed short) (-5000 + 173));

    QCOMPARE(s32f.left, 500.7f);
    QCOMPARE(s32f.right, -123.1f);
    QCOMPARE(s32f.average(), (500.7f - 123.1f)/2);

    // Cleared
    s8u.clear();
    s8s.clear();
    s16u.clear();
    s16s.clear();
    s32f.clear();

    QCOMPARE(s8u.left, (unsigned char) 0x80);
    QCOMPARE(s8u.right, (unsigned char) 0x80);
    QCOMPARE(s8u.average(), (unsigned char) 0x80);

    QCOMPARE(s8s.left, (signed char) 0x00);
    QCOMPARE(s8s.right, (signed char) 0x00);
    QCOMPARE(s8s.average(), (signed char) 0x0);

    QCOMPARE(s16u.left, (unsigned short) 0x8000);
    QCOMPARE(s16u.right, (unsigned short) 0x8000);
    QCOMPARE(s16u.average(), (unsigned short) 0x8000);

    QCOMPARE(s16s.left, (signed short) 0x0);
    QCOMPARE(s16s.right, (signed short) 0x0);
    QCOMPARE(s16s.average(), (signed short) 0x0);

    QCOMPARE(s32f.left, 0.0f);
    QCOMPARE(s32f.right, 0.0f);
    QCOMPARE(s32f.average(), 0.0f);
}


QTEST_APPLESS_MAIN(tst_QAudioBuffer);

#include "tst_qaudiobuffer.moc"
