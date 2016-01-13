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
