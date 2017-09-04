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

#include <QApplication>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QDebug>
#include <QDesktopWidget>
#include <QGroupBox>
#include <QQmlError>
#include <QQuickView>
#include <QQuickWidget>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QWidget widget;
    widget.setWindowTitle(QT_VERSION_STR);

    const QUrl source(QUrl::fromLocalFile(QLatin1String(SRCDIR) + QStringLiteral("/main.qml")));

    QHBoxLayout *hLayout = new QHBoxLayout(&widget);
    QGroupBox *groupBox = new QGroupBox("QuickWidget", &widget);
    QVBoxLayout *vLayout = new QVBoxLayout(groupBox);
    QQuickWidget *quickWidget = new QQuickWidget(groupBox);
    quickWidget->setMinimumSize(200, 200);
    vLayout->addWidget(quickWidget);
    quickWidget->setResizeMode(QQuickWidget::SizeRootObjectToView);
    quickWidget->setSource(source);
    if (quickWidget->status() == QQuickWidget::Error) {
        qWarning() << quickWidget->errors();
        return 1;
    }
    hLayout->addWidget(groupBox);

    groupBox = new QGroupBox("QQuickView/createWindowContainer", &widget);
    vLayout = new QVBoxLayout(groupBox);
    QQuickView *view = new QQuickView;
    view->setSource(source);
    if (view->status() == QQuickView::Error) {
        qWarning() << view->errors();
        return 1;
    }

    view->setResizeMode(QQuickView::SizeRootObjectToView);
    QWidget *container = QWidget::createWindowContainer(view, groupBox);
    container->setMinimumSize(200, 200);
    vLayout->addWidget(container);
    hLayout->addWidget(groupBox);

    const QRect availableGeometry = QApplication::desktop()->availableGeometry(&widget);
    widget.move(availableGeometry.center() - QPoint(widget.sizeHint().width() / 2, widget.sizeHint().height() / 2));

    widget.show();

    return app.exec();
}
