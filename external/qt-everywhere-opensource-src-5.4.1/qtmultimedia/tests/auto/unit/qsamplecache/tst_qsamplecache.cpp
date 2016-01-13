/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Toolkit.
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
