/****************************************************************************
**
** Copyright (C) 2016 Paul Lemire <paul.lemire350@gmail.com>
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
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


#include <QtTest/QTest>
#include <Qt3DRender/private/texturedatamanager_p.h>

namespace {

class FakeGenerator
{
public:
    explicit FakeGenerator(const QString &name)
        : m_name(name)
    {}

    bool operator==(const FakeGenerator &other) const
    {
        return other.m_name == m_name;
    }

    bool operator!=(const FakeGenerator &other) const
    {
        return !(other == *this);
    }
private:
    QString m_name;
};
typedef QSharedPointer<FakeGenerator> FakeGeneratorPtr;

class FakeData
{
public:
    explicit FakeData(int value)
        : m_value(value)
    {}

private:
    int m_value;
};
typedef QSharedPointer<FakeData> FakeDataPtr;


struct FakeAPITexture
{
    void requestUpload() {}
};

using Manager = Qt3DRender::Render::GeneratorDataManager<FakeGeneratorPtr, FakeDataPtr, FakeAPITexture>;

} // anonymous

class tst_TextureDataManager : public QObject
{
    Q_OBJECT

private Q_SLOTS:

    void checkAssumptions()
    {
        // GIVEN
        FakeGeneratorPtr zr1(FakeGeneratorPtr::create(QStringLiteral("ZR1")));
        FakeGeneratorPtr z06(FakeGeneratorPtr::create(QStringLiteral("Z06")));

        // THEN
        QVERIFY(*zr1 == *zr1);
        QVERIFY(*z06 == *z06);
        QVERIFY(*zr1 != *z06);
        QVERIFY(*z06 != *zr1);
    }

    void checkRequestDataShouldCreate()
    {
        // GIVEN
        Manager manager;
        FakeGeneratorPtr generator(FakeGeneratorPtr::create(QStringLiteral("ZR1")));
        FakeAPITexture texture;

        // THEN
        QCOMPARE(manager.pendingGenerators().size(), 0);

        // WHEN
        manager.requestData(generator, &texture);

        // THEN
        QCOMPARE(manager.pendingGenerators().size(), 1);
    }

    void checkRequestDataAlreadyExistingGenerator()
    {
        // GIVEN
        Manager manager;
        FakeGeneratorPtr generator(FakeGeneratorPtr::create(QStringLiteral("ZR1")));
        FakeGeneratorPtr generatorClone(FakeGeneratorPtr::create(QStringLiteral("ZR1")));
        FakeAPITexture texture;

        // THEN
        QCOMPARE(manager.pendingGenerators().size(), 0);

        // WHEN
        for (int i = 0; i < 5; ++i)
            manager.requestData(generator, &texture);

        // THEN
        QCOMPARE(manager.pendingGenerators().size(), 1);

        // WHEN
        for (int i = 0; i < 5; ++i)
            manager.requestData(generatorClone, &texture);

        // THEN
        QCOMPARE(manager.pendingGenerators().size(), 1);
    }


    void checkReleaseDataInvalidEntry()
    {
        // GIVEN
        Manager manager;
        FakeGeneratorPtr generator(FakeGeneratorPtr::create(QStringLiteral("ZR1")));
        FakeAPITexture texture;

        // THEN
        QCOMPARE(manager.pendingGenerators().size(), 0);

        // WHEN
        manager.releaseData(generator, &texture);

        // THEN
        QCOMPARE(manager.pendingGenerators().size(), 0);
        // and should not crash
    }

    void checkReleaseDataValidEntry()
    {
        // GIVEN
        Manager manager;
        FakeGeneratorPtr generator(FakeGeneratorPtr::create(QStringLiteral("ZR1")));
        FakeAPITexture texture;

        // THEN
        QCOMPARE(manager.pendingGenerators().size(), 0);

        // WHEN
        manager.requestData(generator, &texture);

        // THEN
        QCOMPARE(manager.pendingGenerators().size(), 1);

        // WHEN
        manager.releaseData(generator, &texture);

        // THEN
        QCOMPARE(manager.pendingGenerators().size(), 0);
    }

    void checkAssignGetData()
    {
        // GIVEN
        Manager manager;
        FakeGeneratorPtr generator(FakeGeneratorPtr::create(QStringLiteral("ZR1")));
        FakeDataPtr data(FakeDataPtr::create(883));
        FakeAPITexture texture;

        // WHEN
        manager.assignData(generator, data);

        // THEN
        QVERIFY(manager.getData(generator).isNull());

        // WHEN
        manager.requestData(generator, &texture);
        manager.assignData(generator, data);

        // THEN
        QCOMPARE(data, manager.getData(generator));
    }

    void checkPendingGenerators()
    {
        // GIVEN
        Manager manager;
        FakeGeneratorPtr generator(FakeGeneratorPtr::create(QStringLiteral("ZR1")));
        FakeDataPtr data(FakeDataPtr::create(883));
        FakeAPITexture texture;

        // WHEN
        manager.requestData(generator, &texture);

        // THEN
        QCOMPARE(manager.pendingGenerators().size(), 1);
        QCOMPARE(manager.pendingGenerators().first(), generator);

        // WHEN
        manager.requestData(generator, &texture);

        // THEN
        QCOMPARE(manager.pendingGenerators().size(), 1);
        QCOMPARE(manager.pendingGenerators().first(), generator);

        // WHEN
        FakeGeneratorPtr generator2(FakeGeneratorPtr::create(QStringLiteral("Z06")));
        FakeAPITexture texture2;
        manager.requestData(generator2, &texture2);

        // THEN
        QCOMPARE(manager.pendingGenerators().size(), 2);
        QCOMPARE(manager.pendingGenerators().first(), generator);
        QCOMPARE(manager.pendingGenerators().last(), generator2);

        // WHEN
        manager.releaseData(generator, &texture);

        // THEN
        QCOMPARE(manager.pendingGenerators().size(), 1);
        QCOMPARE(manager.pendingGenerators().first(), generator2);
    }
};

QTEST_MAIN(tst_TextureDataManager)

#include "tst_texturedatamanager.moc"
