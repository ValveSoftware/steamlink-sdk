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

//TESTED_COMPONENT=src/multimedia

#include <QtTest/QtTest>
#include <private/qsamplecache_p.h>

class tst_QSampleCache : public QObject
{
    Q_OBJECT
public:

public slots:

private slots:
    void testCachedSample();
    void testNotCachedSample();
    void testEnoughCapacity();
    void testNotEnoughCapacity();
    void testInvalidFile();

private:

};

void tst_QSampleCache::testCachedSample()
{
    QSampleCache cache;
    QSignalSpy loadingSpy(&cache, SIGNAL(isLoadingChanged()));

    QSample* sample = cache.requestSample(QUrl::fromLocalFile(QFINDTESTDATA("testdata/test.wav")));
    QVERIFY(sample);
    QTRY_COMPARE(loadingSpy.count(), 2);
    QTRY_VERIFY(!cache.isLoading());

    loadingSpy.clear();
    QSample* sampleCached = cache.requestSample(QUrl::fromLocalFile(QFINDTESTDATA("testdata/test.wav")));
    QCOMPARE(sample, sampleCached); // sample is cached
    QVERIFY(cache.isCached(QUrl::fromLocalFile(QFINDTESTDATA("testdata/test.wav"))));
    // loading thread still starts, but does nothing in this case
    QTRY_COMPARE(loadingSpy.count(), 2);
    QTRY_VERIFY(!cache.isLoading());

    sample->release();
    sampleCached->release();
}

void tst_QSampleCache::testNotCachedSample()
{
    QSampleCache cache;
    QSignalSpy loadingSpy(&cache, SIGNAL(isLoadingChanged()));

    QSample* sample = cache.requestSample(QUrl::fromLocalFile(QFINDTESTDATA("testdata/test.wav")));
    QVERIFY(sample);
    QTRY_COMPARE(loadingSpy.count(), 2);
    QTRY_VERIFY(!cache.isLoading());
    sample->release();

    QVERIFY(!cache.isCached(QUrl::fromLocalFile(QFINDTESTDATA("testdata/test.wav"))));
}

void tst_QSampleCache::testEnoughCapacity()
{
    QSampleCache cache;
    QSignalSpy loadingSpy(&cache, SIGNAL(isLoadingChanged()));

    QSample* sample = cache.requestSample(QUrl::fromLocalFile(QFINDTESTDATA("testdata/test.wav")));
    QVERIFY(sample);
    QTRY_COMPARE(loadingSpy.count(), 2); // make sure sample is loaded
    QTRY_VERIFY(!cache.isLoading());
    int sampleSize = sample->data().size();
    sample->release();
    cache.setCapacity(sampleSize * 2);

    QVERIFY(!cache.isCached(QUrl::fromLocalFile(QFINDTESTDATA("testdata/test.wav"))));

    loadingSpy.clear();
    sample = cache.requestSample(QUrl::fromLocalFile(QFINDTESTDATA("testdata/test.wav")));
    QVERIFY(sample);
    QTRY_COMPARE(loadingSpy.count(), 2);
    QTRY_VERIFY(!cache.isLoading());
    sample->release();

    QVERIFY(cache.isCached(QUrl::fromLocalFile(QFINDTESTDATA("testdata/test.wav"))));

    // load another sample and make sure first sample is not destroyed
    loadingSpy.clear();
    QSample* sampleOther = cache.requestSample(QUrl::fromLocalFile(QFINDTESTDATA("testdata/test2.wav")));
    QVERIFY(sampleOther);
    QTRY_COMPARE(loadingSpy.count(), 2);
    QTRY_VERIFY(!cache.isLoading());
    sampleOther->release();

    QVERIFY(cache.isCached(QUrl::fromLocalFile(QFINDTESTDATA("testdata/test.wav"))));
    QVERIFY(cache.isCached(QUrl::fromLocalFile(QFINDTESTDATA("testdata/test2.wav"))));

    loadingSpy.clear();
    QSample* sampleCached = cache.requestSample(QUrl::fromLocalFile(QFINDTESTDATA("testdata/test.wav")));
    QCOMPARE(sample, sampleCached); // sample is cached
    QVERIFY(cache.isCached(QUrl::fromLocalFile(QFINDTESTDATA("testdata/test.wav"))));
    QVERIFY(cache.isCached(QUrl::fromLocalFile(QFINDTESTDATA("testdata/test2.wav"))));
    QTRY_COMPARE(loadingSpy.count(), 2);
    QTRY_VERIFY(!cache.isLoading());

    sampleCached->release();
}

void tst_QSampleCache::testNotEnoughCapacity()
{
    QSampleCache cache;
    QSignalSpy loadingSpy(&cache, SIGNAL(isLoadingChanged()));

    QSample* sample = cache.requestSample(QUrl::fromLocalFile(QFINDTESTDATA("testdata/test.wav")));
    QVERIFY(sample);
    QTRY_COMPARE(loadingSpy.count(), 2); // make sure sample is loaded
    QTRY_VERIFY(!cache.isLoading());
    int sampleSize = sample->data().size();
    sample->release();
    cache.setCapacity(sampleSize / 2); // unloads all samples

    QVERIFY(!cache.isCached(QUrl::fromLocalFile(QFINDTESTDATA("testdata/test.wav"))));

    loadingSpy.clear();
    sample = cache.requestSample(QUrl::fromLocalFile(QFINDTESTDATA("testdata/test.wav")));
    QVERIFY(sample);
    QTRY_COMPARE(loadingSpy.count(), 2);
    QTRY_VERIFY(!cache.isLoading());
    sample->release();

    QVERIFY(cache.isCached(QUrl::fromLocalFile(QFINDTESTDATA("testdata/test.wav"))));

    // load another sample to force sample cache to destroy first sample
    loadingSpy.clear();
    QSample* sampleOther = cache.requestSample(QUrl::fromLocalFile(QFINDTESTDATA("testdata/test2.wav")));
    QVERIFY(sampleOther);
    QTRY_COMPARE(loadingSpy.count(), 2);
    QTRY_VERIFY(!cache.isLoading());
    sampleOther->release();

    QVERIFY(!cache.isCached(QUrl::fromLocalFile(QFINDTESTDATA("testdata/test.wav"))));
}

void tst_QSampleCache::testInvalidFile()
{
    QSampleCache cache;
    QSignalSpy loadingSpy(&cache, SIGNAL(isLoadingChanged()));

    QSample* sample = cache.requestSample(QUrl::fromLocalFile("invalid"));
    QVERIFY(sample);
    QTRY_COMPARE(sample->state(), QSample::Error);
    QTRY_COMPARE(loadingSpy.count(), 2);
    QTRY_VERIFY(!cache.isLoading());
    sample->release();

    QVERIFY(!cache.isCached(QUrl::fromLocalFile("invalid")));
}

QTEST_MAIN(tst_QSampleCache)

#include "tst_qsamplecache.moc"
