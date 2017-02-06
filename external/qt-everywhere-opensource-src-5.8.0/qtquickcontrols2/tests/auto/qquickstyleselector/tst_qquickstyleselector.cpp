/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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
#include <QtQuickControls2/qquickstyle.h>
#include <QtQuickControls2/private/qquickstyle_p.h>
#include <QtQuickControls2/private/qquickstyleselector_p.h>
#include "../shared/util.h"

class tst_QQuickStyleSelector : public QQmlDataTest
{
    Q_OBJECT

private slots:
    void initTestCase();

    void select_data();
    void select();
};

void tst_QQuickStyleSelector::initTestCase()
{
    QQmlDataTest::initTestCase();
    QQuickStylePrivate::init(dataDirectoryUrl());
}

void tst_QQuickStyleSelector::select_data()
{
    QTest::addColumn<QString>("file");
    QTest::addColumn<QString>("style");
    QTest::addColumn<QString>("path");
    QTest::addColumn<QString>("fallback");
    QTest::addColumn<QString>("expected");

    // Control.qml exists only in the default style
    QTest::newRow("control") << "Control.qml" << "" << "data" << "" << testFileUrl("Control.qml").toString();
    QTest::newRow("/control") << "Control.qml" << "" << dataDirectory() << "" << testFileUrl("Control.qml").toString();
    QTest::newRow("fs/control") << "Control.qml" << "FileSystemStyle" << "data" << "" << testFileUrl("Control.qml").toString();
    QTest::newRow("/fs/control") << "Control.qml" << "FileSystemStyle" << dataDirectory() << "" << testFileUrl("Control.qml").toString();
    QTest::newRow(":/control") << "Control.qml" << "ResourceStyle" << ":/" << "" << testFileUrl("Control.qml").toString();
    QTest::newRow("qrc:/control") << "Control.qml" << "ResourceStyle" << "qrc:/" << "" << testFileUrl("Control.qml").toString();
    QTest::newRow("nosuch/control") << "Control.qml" << "NoSuchStyle" << "data" << "" << testFileUrl("Control.qml").toString();
    QTest::newRow("/nosuch/control") << "Control.qml" << "NoSuchStyle" << dataDirectory() << "" << testFileUrl("Control.qml").toString();

    QTest::newRow("control->base") << "Control.qml" << "" << "data" << "FallbackStyle" << testFileUrl("Control.qml").toString();
    QTest::newRow("/control->base") << "Control.qml" << "" << dataDirectory() << "FallbackStyle" << testFileUrl("Control.qml").toString();
    QTest::newRow("fs/control->base") << "Control.qml" << "FileSystemStyle" << "data" << "FallbackStyle" << testFileUrl("Control.qml").toString();
    QTest::newRow("/fs/control->base") << "Control.qml" << "FileSystemStyle" << dataDirectory() << "FallbackStyle" << testFileUrl("Control.qml").toString();
    QTest::newRow(":/control->base") << "Control.qml" << "ResourceStyle" << ":/" << "FallbackStyle" << testFileUrl("Control.qml").toString();
    QTest::newRow("qrc:/control->base") << "Control.qml" << "ResourceStyle" << "qrc:/" << "FallbackStyle" << testFileUrl("Control.qml").toString();
    QTest::newRow("nosuch/control->base") << "Control.qml" << "NoSuchStyle" << "data" << "FallbackStyle" << testFileUrl("Control.qml").toString();
    QTest::newRow("/nosuch/control->base") << "Control.qml" << "NoSuchStyle" << dataDirectory() << "FallbackStyle" << testFileUrl("Control.qml").toString();

    // Label.qml exists in the default and fallback styles
    QTest::newRow("label") << "Label.qml" << "" << "data" << "" << testFileUrl("Label.qml").toString();
    QTest::newRow("/label") << "Label.qml" << "" << dataDirectory() << "" << testFileUrl("Label.qml").toString();
    QTest::newRow("fs/label") << "Label.qml" << "FileSystemStyle" << "data" << "" << testFileUrl("Label.qml").toString();
    QTest::newRow("/fs/label") << "Label.qml" << "FileSystemStyle" << dataDirectory() << "" << testFileUrl("Label.qml").toString();
    QTest::newRow(":/label") << "Label.qml" << "ResourceStyle" << ":/" << "" << testFileUrl("Label.qml").toString();
    QTest::newRow("qrc:/label") << "Label.qml" << "ResourceStyle" << "qrc:/" << "" << testFileUrl("Label.qml").toString();
    QTest::newRow("nosuch/label") << "Label.qml" << "NoSuchStyle" << "data" << "" << testFileUrl("Label.qml").toString();
    QTest::newRow("/nosuch/label") << "Label.qml" << "NoSuchStyle" << dataDirectory() << "" << testFileUrl("Label.qml").toString();

    QTest::newRow("label->base") << "Label.qml" << "" << "data" << "FallbackStyle" << testFileUrl("FallbackStyle/Label.qml").toString();
    QTest::newRow("/label->base") << "Label.qml" << "" << dataDirectory() << "FallbackStyle" << testFileUrl("Label.qml").toString();
    QTest::newRow("fs/label->base") << "Label.qml" << "FileSystemStyle" << "data" << "FallbackStyle" << testFileUrl("FallbackStyle/Label.qml").toString();
    QTest::newRow("/fs/label->base") << "Label.qml" << "FileSystemStyle" << dataDirectory() << "FallbackStyle" << testFileUrl("FallbackStyle/Label.qml").toString();
    QTest::newRow(":/label->base") << "Label.qml" << "ResourceStyle" << ":/" << "FallbackStyle" << testFileUrl("FallbackStyle/Label.qml").toString();
    QTest::newRow("qrc:/label->base") << "Label.qml" << "ResourceStyle" << "qrc:/" << "FallbackStyle" << testFileUrl("FallbackStyle/Label.qml").toString();
    QTest::newRow("nosuch/label->base") << "Label.qml" << "NoSuchStyle" << "data" << "FallbackStyle" << testFileUrl("FallbackStyle/Label.qml").toString();
    QTest::newRow("/nosuch/label->base") << "Label.qml" << "NoSuchStyle" << dataDirectory() << "FallbackStyle" << testFileUrl("FallbackStyle/Label.qml").toString();

    // Button.qml exists in all styles including the fs and qrc styles
    QTest::newRow("button") << "Button.qml" << "" << "data" << "" << testFileUrl("Button.qml").toString();
    QTest::newRow("/button") << "Button.qml" << "" << dataDirectory() << "" << testFileUrl("Button.qml").toString();
    QTest::newRow("fs/button") << "Button.qml" << "FileSystemStyle" << "data" << "" << testFileUrl("FileSystemStyle/Button.qml").toString();
    QTest::newRow("/fs/button") << "Button.qml" << "FileSystemStyle" << dataDirectory() << "" << testFileUrl("FileSystemStyle/Button.qml").toString();
    QTest::newRow(":/button") << "Button.qml" << "ResourceStyle" << ":/" << "" << "qrc:/ResourceStyle/Button.qml";
    QTest::newRow("qrc:/button") << "Button.qml" << "ResourceStyle" << "qrc:/" << "" << "qrc:/ResourceStyle/Button.qml";
    QTest::newRow("nosuch/button") << "Button.qml" << "NoSuchStyle" << "data" << "" << testFileUrl("Button.qml").toString();
    QTest::newRow("/nosuch/button") << "Button.qml" << "NoSuchStyle" << dataDirectory() << "" << testFileUrl("Button.qml").toString();

    QTest::newRow("button->base") << "Button.qml" << "" << "data" << "FallbackStyle" << testFileUrl("FallbackStyle/Button.qml").toString();
    QTest::newRow("/button->base") << "Button.qml" << "" << dataDirectory() << "FallbackStyle" << testFileUrl("Button.qml").toString();
    QTest::newRow("fs/button->base") << "Button.qml" << "FileSystemStyle" << "data" << "FallbackStyle" << testFileUrl("FileSystemStyle/Button.qml").toString();
    QTest::newRow("/fs/button->base") << "Button.qml" << "FileSystemStyle" << dataDirectory() << "FallbackStyle" << testFileUrl("FileSystemStyle/Button.qml").toString();
    QTest::newRow(":/button->base") << "Button.qml" << "ResourceStyle" << ":/" << "FallbackStyle" << "qrc:/ResourceStyle/Button.qml";
    QTest::newRow("qrc:/button->base") << "Button.qml" << "ResourceStyle" << "qrc:/" << "FallbackStyle" << "qrc:/ResourceStyle/Button.qml";
    QTest::newRow("nosuch/button->base") << "Button.qml" << "NoSuchStyle" << "data" << "FallbackStyle" << testFileUrl("FallbackStyle/Button.qml").toString();
    QTest::newRow("/nosuch/button->base") << "Button.qml" << "NoSuchStyle" << dataDirectory() << "FallbackStyle" << testFileUrl("FallbackStyle/Button.qml").toString();
}

void tst_QQuickStyleSelector::select()
{
    QFETCH(QString, file);
    QFETCH(QString, style);
    QFETCH(QString, path);
    QFETCH(QString, fallback);
    QFETCH(QString, expected);

    QQuickStyle::setStyle(QDir(path).filePath(style));
    QQuickStyle::setFallbackStyle(fallback);

    QQuickStyleSelector selector;
    selector.setBaseUrl(dataDirectoryUrl());
    QCOMPARE(selector.select(file), expected);
}

QTEST_MAIN(tst_QQuickStyleSelector)

#include "tst_qquickstyleselector.moc"
