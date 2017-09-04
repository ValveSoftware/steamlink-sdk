/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
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

#include <qtest.h>
#include "../util.h"

#include <QSurfaceFormat>
#include <QTimer>
#include <qwebengineview.h>

class tst_QWebEngineDefaultSurfaceFormat : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void customDefaultSurfaceFormat();
};

void tst_QWebEngineDefaultSurfaceFormat::customDefaultSurfaceFormat()
{
#if defined(Q_OS_WIN)
    QSKIP("Crashes on Windows");
#endif
    // Setting a new default QSurfaceFormat with a core OpenGL profile before
    // app instantiation should succeed, without abort() being called.
    int argc = 1;
    char *argv[] = { const_cast<char*>("tst_QWebEngineDefaultSurfaceFormat") };

    QSurfaceFormat format;
    format.setVersion( 3, 3 );
    format.setProfile( QSurfaceFormat::CoreProfile );
    QSurfaceFormat::setDefaultFormat( format );

    QApplication app(argc, argv);

    QWebEngineView view;
    view.load(QUrl("qrc:///resources/index.html"));
    view.show();
    QObject::connect(&view, &QWebEngineView::loadFinished, []() {
        QTimer::singleShot(100, qApp, &QCoreApplication::quit);
    });

    QCOMPARE(app.exec(), 0);
}

QTEST_APPLESS_MAIN(tst_QWebEngineDefaultSurfaceFormat)
#include "tst_qwebenginedefaultsurfaceformat.moc"
