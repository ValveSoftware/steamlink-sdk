/****************************************************************************
**
** Copyright (C) 2015 Klaralvdalens Datakonsult AB (KDAB).
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
#include <Qt3DCore/private/qaspectfactory_p.h>
#include <Qt3DCore/QAbstractAspect>

using namespace QT_PREPEND_NAMESPACE(Qt3DCore);

#define FAKE_ASPECT(ClassName) \
class ClassName : public QAbstractAspect \
{ \
    Q_OBJECT \
public: \
    explicit ClassName(QObject *parent = 0) \
        : QAbstractAspect(parent) {} \
\
private: \
    void onRegistered() Q_DECL_OVERRIDE {} \
    void onEngineStartup() Q_DECL_OVERRIDE {} \
    void onEngineShutdown() Q_DECL_OVERRIDE {} \
\
    QVector<QAspectJobPtr> jobsToExecute(qint64) Q_DECL_OVERRIDE \
    { \
        return QVector<QAspectJobPtr>(); \
    } \
};

FAKE_ASPECT(DefaultFakeAspect)
FAKE_ASPECT(AnotherFakeAspect)

QT3D_REGISTER_ASPECT("default", DefaultFakeAspect)

class tst_QAspectFactory : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void shoulHaveDefaultState()
    {
        // GIVEN
        QAspectFactory factory;

        // THEN
        QCOMPARE(factory.availableFactories().size(), 1);
        QCOMPARE(factory.availableFactories().first(), QLatin1String("default"));

        // WHEN
        QAbstractAspect *aspect = factory.createAspect(QLatin1String("default"));

        // THEN
        QVERIFY(qobject_cast<DefaultFakeAspect*>(aspect) != nullptr);
        QVERIFY(aspect->parent() == nullptr);
    }

    void shouldKnowAspectNames()
    {
        // GIVEN
        QAspectFactory factory;

        // WHEN
        DefaultFakeAspect fake;
        AnotherFakeAspect missing;

        // THEN
        QCOMPARE(factory.aspectName(&fake), QLatin1String("default"));
        QCOMPARE(factory.aspectName(&missing), QLatin1String());
    }

    void shouldGracefulyHandleMissingFactories()
    {
        // GIVEN
        QAspectFactory factory;

        // WHEN
        QAbstractAspect *aspect = factory.createAspect(QLatin1String("missing"), this);

        // THEN
        QVERIFY(qobject_cast<AnotherFakeAspect*>(aspect) == nullptr);
    }
};

QTEST_MAIN(tst_QAspectFactory)

#include "tst_qaspectfactory.moc"
