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

#include "util.h"

#include <QtTest/QtTest>
#include <QQmlContext>
#include <QQuickView>
#include <QQuickItem>
#include <QPainter>
#include <qtwebengineglobal.h>
#include <private/qquickwebenginetestsupport_p.h>
#include <private/qquickwebengineview_p.h>

class TestView : public QQuickView {
    Q_OBJECT
public:
    TestView()
    {
        connect(this, &TestView::_q_exposeChanged, this, &TestView::exposeChanged,
                Qt::QueuedConnection);
    }

    virtual void exposeEvent(QExposeEvent *e) Q_DECL_OVERRIDE {
        QQuickView::exposeEvent(e);
        emit _q_exposeChanged();
    }

Q_SIGNALS:
    void _q_exposeChanged();
    void exposeChanged();
};

class tst_QQuickWebEngineViewGraphics : public QObject
{
    Q_OBJECT
public:
    tst_QQuickWebEngineViewGraphics();
    virtual ~tst_QQuickWebEngineViewGraphics();

public Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

private Q_SLOTS:
    void simpleGraphics();
    void renderMultipleTimes();
    void renderAfterNodeCleanup();
    void showHideShow();
    void simpleAcceleratedLayer();
    void reparentToOtherWindow();

private:
    void setHtml(const QString &html);
    QScopedPointer<TestView> m_view;
    QScopedPointer<QQuickWebEngineTestSupport> m_testSupport;
};

static const QString greenSquare("<div style=\"background-color: #00ff00; position:absolute; left:50px; top: 50px; width: 50px; height: 50px;\"></div>");
static const QString acLayerGreenSquare("<div style=\"background-color: #00ff00; position:absolute; left:50px; top: 50px; width: 50px; height: 50px; transform: translateZ(0); -webkit-transform: translateZ(0);\"></div>");

static QImage get150x150GreenReferenceImage()
{
    static QImage reference;
    if (reference.isNull()) {
        reference = QImage(150, 150, QImage::Format_RGB32);
        reference.fill(Qt::white);
        QPainter painter(&reference);
        painter.fillRect(50, 50, 50, 50, QColor("#00ff00"));
    }
    return reference;
}

tst_QQuickWebEngineViewGraphics::tst_QQuickWebEngineViewGraphics()
{
}

tst_QQuickWebEngineViewGraphics::~tst_QQuickWebEngineViewGraphics()
{
}

// This will be called before the first test function is executed.
// It is only called once.
void tst_QQuickWebEngineViewGraphics::initTestCase()
{
    QtWebEngine::initialize();
    m_testSupport.reset(new QQuickWebEngineTestSupport);
}

void tst_QQuickWebEngineViewGraphics::init()
{
    m_view.reset(new TestView);
}

void tst_QQuickWebEngineViewGraphics::cleanup()
{
}

void tst_QQuickWebEngineViewGraphics::simpleGraphics()
{
    setHtml(greenSquare);
    QCOMPARE(m_view->grabWindow(), get150x150GreenReferenceImage());
}

void tst_QQuickWebEngineViewGraphics::renderMultipleTimes()
{
    // This test is for loadVisuallyCommitted signal.
    // The setHtml() should not fail after multiple page load.
    setHtml(greenSquare);
    setHtml(greenSquare);
}

void tst_QQuickWebEngineViewGraphics::renderAfterNodeCleanup()
{
    setHtml(greenSquare);

    // Do it twice in a row, if the window isn't visible, the scene graph is going to be trashed by QQuickWindow::grabWindow after the first render.
    QVERIFY(!m_view->isVisible());
    QCOMPARE(m_view->grabWindow(), get150x150GreenReferenceImage());
    QCOMPARE(m_view->grabWindow(), get150x150GreenReferenceImage());
}

void tst_QQuickWebEngineViewGraphics::showHideShow()
{
    setHtml(greenSquare);
    QSignalSpy exposeSpy(m_view.data(), SIGNAL(exposeChanged()));
    m_view->show();
    QVERIFY(exposeSpy.wait());
    QCOMPARE(m_view->grabWindow(), get150x150GreenReferenceImage());

    m_view->hide();
    QVERIFY(exposeSpy.wait());
    m_view->show();
    QVERIFY(exposeSpy.wait());
    QCOMPARE(m_view->grabWindow(), get150x150GreenReferenceImage());
}

void tst_QQuickWebEngineViewGraphics::simpleAcceleratedLayer()
{
    setHtml(acLayerGreenSquare);
    QCOMPARE(m_view->grabWindow(), get150x150GreenReferenceImage());
}

void tst_QQuickWebEngineViewGraphics::reparentToOtherWindow()
{
    setHtml(greenSquare);
    QQuickWindow window;
    window.resize(m_view->size());
    window.create();

    m_view->rootObject()->setParentItem(window.contentItem());
    QCOMPARE(window.grabWindow(), get150x150GreenReferenceImage());
}

void tst_QQuickWebEngineViewGraphics::setHtml(const QString &html)
{
    QString htmlData = QUrl::toPercentEncoding(html);
    QString qmlData = QUrl::toPercentEncoding(QStringLiteral("import QtQuick 2.0; import QtWebEngine 1.2; WebEngineView { width: 150; height: 150 }"));
    m_view->setSource(QUrl(QStringLiteral("data:text/plain,%1").arg(qmlData)));
    m_view->create();

    QQuickWebEngineView *webEngineView = static_cast<QQuickWebEngineView *>(m_view->rootObject());
    webEngineView->setProperty("url", QUrl(QStringLiteral("data:text/html,%1").arg(htmlData)));
    webEngineView->setTestSupport(m_testSupport.data());
    QVERIFY(waitForViewportReady(webEngineView));
    QCOMPARE(m_view->rootObject()->property("loading"), QVariant(false));
}

QTEST_MAIN(tst_QQuickWebEngineViewGraphics)
#include "tst_qquickwebengineviewgraphics.moc"
