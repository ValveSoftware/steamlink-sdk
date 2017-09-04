/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
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

#ifndef TESTWINDOW_H
#define TESTWINDOW_H

#if 0
#pragma qt_no_master_include
#endif

#include <QResizeEvent>
#include <QScopedPointer>
#include <QtQuick/qquickitem.h>
#include <QtQuick/qquickview.h>

// TestWindow: Utility class to ignore QQuickView details.
class TestWindow : public QQuickView {
public:
    inline TestWindow(QQuickItem *webEngineView);
    QScopedPointer<QQuickItem> webEngineView;

protected:
    inline void resizeEvent(QResizeEvent*);
};

inline TestWindow::TestWindow(QQuickItem *webEngineView)
    : webEngineView(webEngineView)
{
    Q_ASSERT(webEngineView);
    webEngineView->setParentItem(contentItem());
    resize(300, 400);
}

inline void TestWindow::resizeEvent(QResizeEvent *event)
{
    QQuickView::resizeEvent(event);
    webEngineView->setX(0);
    webEngineView->setY(0);
    webEngineView->setWidth(event->size().width());
    webEngineView->setHeight(event->size().height());
}

#endif /* TESTWINDOW_H */
