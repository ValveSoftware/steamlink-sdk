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

#include "testwindow.h"
#include "util.h"

#include <QGuiApplication>
#include <QtQml/QQmlEngine>
#include <QScopedPointer>
#include <QSurfaceFormat>
#include <QtTest/QtTest>
#include <QTimer>
#include <private/qquickwebengineview_p.h>

class tst_QQuickWebEngineDefaultSurfaceFormat : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void customDefaultSurfaceFormat();

private:
    inline void initEngineAndViewComponent();
    inline void initWindow();
    inline void deleteWindow();

    inline QQuickWebEngineView *newWebEngineView();
    inline QQuickWebEngineView *webEngineView() const;
    QUrl urlFromTestPath(const char *localFilePath);

    QQmlEngine *m_engine;
    TestWindow *m_window;
    QScopedPointer<QQmlComponent> m_component;
};

void tst_QQuickWebEngineDefaultSurfaceFormat::initEngineAndViewComponent() {
    m_engine = new QQmlEngine(this);
    m_component.reset(new QQmlComponent(m_engine, this));
    m_component->setData(QByteArrayLiteral("import QtQuick 2.0\n"
                                           "import QtWebEngine 1.2\n"
                                           "WebEngineView {}")
                         , QUrl());
}

void tst_QQuickWebEngineDefaultSurfaceFormat::initWindow()
{
    m_window = new TestWindow(newWebEngineView());
}

void tst_QQuickWebEngineDefaultSurfaceFormat::deleteWindow()
{
    delete m_window;
}

QQuickWebEngineView *tst_QQuickWebEngineDefaultSurfaceFormat::newWebEngineView()
{
    QObject *viewInstance = m_component->create();
    QQuickWebEngineView *webEngineView = qobject_cast<QQuickWebEngineView*>(viewInstance);
    return webEngineView;
}

inline QQuickWebEngineView *tst_QQuickWebEngineDefaultSurfaceFormat::webEngineView() const
{
    return static_cast<QQuickWebEngineView*>(m_window->webEngineView.data());
}

QUrl tst_QQuickWebEngineDefaultSurfaceFormat::urlFromTestPath(const char *localFilePath)
{
    QString testSourceDirPath = QString::fromLocal8Bit(TESTS_SOURCE_DIR);
    if (!testSourceDirPath.endsWith(QLatin1Char('/')))
        testSourceDirPath.append(QLatin1Char('/'));

    return QUrl::fromLocalFile(testSourceDirPath + QString::fromUtf8(localFilePath));
}

void tst_QQuickWebEngineDefaultSurfaceFormat::customDefaultSurfaceFormat()
{
    // Setting a new default QSurfaceFormat with a core OpenGL profile, before
    // app instantiation should succeed, without abort() being called.
    int argc = 1;
    char *argv[] = { const_cast<char*>("tst_QQuickWebEngineDefaultSurfaceFormat") };

    QSurfaceFormat format;
    format.setVersion( 3, 3 );
    format.setProfile( QSurfaceFormat::CoreProfile );
    QSurfaceFormat::setDefaultFormat( format );

    QGuiApplication app(argc, argv);
    QtWebEngine::initialize();

    initEngineAndViewComponent();
    initWindow();
    QQuickWebEngineView* view = webEngineView();
    view->setUrl(urlFromTestPath("html/basic_page.html"));
    m_window->show();

    QObject::connect(
        view,
        &QQuickWebEngineView::loadingChanged, [](QQuickWebEngineLoadRequest* request)
        {
            if (request->status() == QQuickWebEngineView::LoadSucceededStatus
               || request->status() == QQuickWebEngineView::LoadFailedStatus)
                QTimer::singleShot(100, qApp, &QCoreApplication::quit);
        }
    );

    QObject::connect(qApp, &QCoreApplication::aboutToQuit, [this]() {
        this->deleteWindow();
    });

    QCOMPARE(app.exec(), 0);
}

QTEST_APPLESS_MAIN(tst_QQuickWebEngineDefaultSurfaceFormat)
#include "tst_qquickwebenginedefaultsurfaceformat.moc"
