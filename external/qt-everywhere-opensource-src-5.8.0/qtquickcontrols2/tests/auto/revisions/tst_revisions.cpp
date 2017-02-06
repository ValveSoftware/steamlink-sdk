/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
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

#include <QtTest/qtest.h>
#include <QtTest/qsignalspy.h>
#include <QtQml/qqmlengine.h>
#include <QtQml/qqmlcomponent.h>

class tst_revisions : public QObject
{
    Q_OBJECT

private slots:
    void revisions_data();
    void revisions();
};

void tst_revisions::revisions_data()
{
    QTest::addColumn<int>("revision");

    // In theory, this could be done in a loop from 5.7 to QT_VERSION, but
    // the test would immediately fail when the Qt version was bumped up.
    // Therefore it is better to just add these lines by hand when adding
    // new revisions.
    QTest::newRow("2.0") << 0; // Qt 5.7
    QTest::newRow("2.1") << 1; // Qt 5.8
}

void tst_revisions::revisions()
{
    QFETCH(int, revision);

    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData(QString("import QtQuick 2.0; \
                               import QtQuick.Controls 2.%1; \
                               import QtQuick.Controls.impl 2.%1; \
                               import QtQuick.Controls.Material 2.%1; \
                               import QtQuick.Controls.Material.impl 2.%1; \
                               import QtQuick.Controls.Universal 2.%1; \
                               import QtQuick.Controls.Universal.impl 2.%1; \
                               Control { }").arg(revision).toUtf8(), QUrl());

    QScopedPointer<QObject> object(component.create());
    QVERIFY2(!object.isNull(), qPrintable(component.errorString()));
}

QTEST_MAIN(tst_revisions)

#include "tst_revisions.moc"
