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

#include <QApplication>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QDebug>
#include <QDesktopWidget>
#include <QGroupBox>
#include <QQmlApplicationEngine>
#include <QQmlError>
#include <QQuickView>
#include <QQuickWidget>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QWidget widget;
    widget.setWindowTitle(QT_VERSION_STR);

    QHBoxLayout *hLayout = new QHBoxLayout(&widget);
    QGroupBox *groupBox = new QGroupBox("QuickWidget", &widget);
    QVBoxLayout *vLayout = new QVBoxLayout(groupBox);
    QQuickWidget *quickWidget = new QQuickWidget(groupBox);
    quickWidget->setMinimumSize(360, 520);
    vLayout->addWidget(quickWidget);
    quickWidget->setResizeMode(QQuickWidget::SizeRootObjectToView);
    quickWidget->setSource(QUrl(QLatin1String("qrc:/main.qml")));
    if (quickWidget->status() == QQuickWidget::Error) {
        qWarning() << quickWidget->errors();
        return 1;
    }
    hLayout->addWidget(groupBox);

    const QUrl gallerySource(QLatin1String("qrc:/gallery.qml"));
    QQmlApplicationEngine engine(gallerySource);
    QObject *root = engine.rootObjects().value(0, nullptr);
    if (!root || !root->isWindowType()) {
        qWarning() << "Load error" << gallerySource;
        return 1;
    }
    groupBox = new QGroupBox("QQuickView/createWindowContainer", &widget);
    vLayout = new QVBoxLayout(groupBox);
    QWidget *container = QWidget::createWindowContainer(qobject_cast<QWindow *>(root), groupBox);
    container->setMinimumSize(360, 520);
    vLayout->addWidget(container);
    hLayout->addWidget(groupBox);

    const QRect availableGeometry = QApplication::desktop()->availableGeometry(&widget);
    widget.move(availableGeometry.center() - QPoint(widget.sizeHint().width() / 2, widget.sizeHint().height() / 2));

    widget.show();

    return app.exec();
}
