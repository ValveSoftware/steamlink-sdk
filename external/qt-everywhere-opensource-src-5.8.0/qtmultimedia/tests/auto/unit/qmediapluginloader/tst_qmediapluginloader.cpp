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

#include <private/qmediapluginloader_p.h>
#include <qmediaserviceproviderplugin.h>

#include <QtTest/QtTest>
#include <QDebug>

QT_USE_NAMESPACE

class tst_QMediaPluginLoader : public QObject
{
    Q_OBJECT

public slots:
    void initTestCase();
    void cleanupTestCase();

private slots:
    void testInstance();
    void testInstances();
    void testInvalidKey();

private:
    QMediaPluginLoader *loader;
};

void tst_QMediaPluginLoader::initTestCase()
{
    loader = new QMediaPluginLoader(QMediaServiceProviderFactoryInterface_iid,
                                QLatin1String("mediaservice"),
                                Qt::CaseInsensitive);
}

void tst_QMediaPluginLoader::cleanupTestCase()
{
    delete loader;
}

void tst_QMediaPluginLoader::testInstance()
{
    const QStringList keys = loader->keys();

    if (keys.isEmpty()) // Test is invalidated, skip.
        QSKIP("No plug-ins available");

    foreach (const QString &key, keys)
        QVERIFY(loader->instance(key) != 0);
}

void tst_QMediaPluginLoader::testInstances()
{
    const QStringList keys = loader->keys();

    if (keys.isEmpty()) // Test is invalidated, skip.
        QSKIP("No plug-ins available");

    foreach (const QString &key, keys)
        QVERIFY(loader->instances(key).size() > 0);
}

// Last so as to not interfere with the other tests if there is a failure.
void tst_QMediaPluginLoader::testInvalidKey()
{
    const QString key(QLatin1String("invalid-key"));

    // This test assumes there is no 'invalid-key' in the key list, verify that.
    if (loader->keys().contains(key))
        QSKIP("a plug-in includes the invalid key");

    QVERIFY(loader->instance(key) == 0);

    // Test looking up the key hasn't inserted it into the list. See QMap::operator[].
    QVERIFY(!loader->keys().contains(key));

    QVERIFY(loader->instances(key).isEmpty());
    QVERIFY(!loader->keys().contains(key));
}

QTEST_MAIN(tst_QMediaPluginLoader)

#include "tst_qmediapluginloader.moc"
