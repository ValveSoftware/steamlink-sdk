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

#include <QtWidgets/QApplication>
#include <QtQml>
#include <QtQuick/QQuickView>
#include <QtCore/QString>
#include <QStandardItemModel>
#include <QStringList>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QStandardItemModel model;
    model.appendRow(new QStandardItem("Norway"));
    model.appendRow(new QStandardItem("Netherlands"));
    model.appendRow(new QStandardItem("New Zealand"));
    model.appendRow(new QStandardItem("Namibia"));
    model.appendRow(new QStandardItem("Nicaragua"));
    model.appendRow(new QStandardItem("North Korea"));
    model.appendRow(new QStandardItem("Northern Cyprus "));
    model.appendRow(new QStandardItem("Sweden"));
    model.appendRow(new QStandardItem("Denmark"));

    QStringList strings;
    strings << "Norway" << "Sweden" << "Denmark";

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("standardmodel", &model);
    engine.rootContext()->setContextProperty("stringmodel", strings);
    engine.load(QUrl("main.qml"));
    QObject *topLevel = engine.rootObjects().value(0);

    QQuickWindow *window = qobject_cast<QQuickWindow *>(topLevel);
    if ( !window ) {
        qWarning("Error: Your root item has to be a Window.");
        return -1;
    }
    window->show();
    return app.exec();
}
