/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "mainwindow.h"
#include "fbitem.h"
#include <QCoreApplication>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QRadioButton>
#include <QCheckBox>
#include <QLabel>
#include <QQuickItem>

MainWindow::MainWindow(bool transparency)
    : m_currentView(0),
      m_currentRootObject(0),
      m_transparent(transparency)
{
    QVBoxLayout *layout = new QVBoxLayout;

    QGroupBox *groupBox = new QGroupBox(tr("Type"));
    QVBoxLayout *vbox = new QVBoxLayout;
    m_radioView = new QRadioButton(tr("QQuickView in a window container (direct)"));
    m_radioWidget = new QRadioButton(tr("QQuickWidget (indirect through framebuffer objects)"));
    vbox->addWidget(m_radioWidget);
    vbox->addWidget(m_radioView);
    m_radioWidget->setChecked(true);
    m_state = Unknown;
    connect(m_radioWidget, &QRadioButton::toggled, this, &MainWindow::updateView);
    connect(m_radioView, &QRadioButton::toggled, this, &MainWindow::updateView);
    groupBox->setLayout(vbox);

    layout->addWidget(groupBox);

    m_checkboxMultiSample = new QCheckBox(tr("Multisample (4x)"));
    connect(m_checkboxMultiSample, &QCheckBox::toggled, this, &MainWindow::updateView);
    layout->addWidget(m_checkboxMultiSample);

    m_labelStatus = new QLabel;
    layout->addWidget(m_labelStatus);

    qmlRegisterType<FbItem>("fbitem", 1, 0, "FbItem");

    QWidget *quickContainer = new QWidget;
    layout->addWidget(quickContainer);
    layout->setStretchFactor(quickContainer, 8);
    m_containerLayout = new QVBoxLayout;
    quickContainer->setLayout(m_containerLayout);

    // Add an overlay widget to demonstrate that it will _not_ work with
    // QQuickView, whereas it is perfectly fine with QQuickWidget.
    QPalette semiTransparent(QColor(255,0,0,128));
    semiTransparent.setBrush(QPalette::Text, Qt::white);
    semiTransparent.setBrush(QPalette::WindowText, Qt::white);

    m_overlayLabel = new QLabel("This is a\nsemi-transparent\n overlay widget\nwhich is placed\non top\n of the Quick\ncontent.", this);
    m_overlayLabel->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
    m_overlayLabel->setAutoFillBackground(true);
    m_overlayLabel->setPalette(semiTransparent);
    QFont f = font();
    f.setPixelSize(QFontInfo(f).pixelSize()*2);
    f.setWeight(QFont::Bold);
    m_overlayLabel->setFont(f);
    m_overlayLabel->hide();

    m_checkboxOverlayVisible = new QCheckBox(tr("Show widget overlay"));
    connect(m_checkboxOverlayVisible, &QCheckBox::toggled, m_overlayLabel, &QWidget::setVisible);
    layout->addWidget(m_checkboxOverlayVisible);

    setLayout(layout);

    updateView();
}

void MainWindow::resizeEvent(QResizeEvent*)
{
    int margin = width() / 10;
    int top = m_checkboxMultiSample->y();
    int bottom = m_checkboxOverlayVisible->geometry().bottom();
    m_overlayLabel->setGeometry(margin, top, width() - 2 * margin, bottom - top);
}

void MainWindow::switchTo(QWidget *view)
{
    if (m_containerLayout->count())
        m_containerLayout->takeAt(0);

    delete m_currentView;
    m_currentView = view;
    m_containerLayout->addWidget(m_currentView);
    m_currentView->setFocus();
}

void MainWindow::updateView()
{
    QSurfaceFormat format;
    format.setDepthBufferSize(16);
    format.setStencilBufferSize(8);
    if (m_transparent)
        format.setAlphaBufferSize(8);
    if (m_checkboxMultiSample->isChecked())
        format.setSamples(4);

    State state = m_radioView->isChecked() ? UseWindow : UseWidget;

    if (m_format == format && m_state == state)
        return;

    m_format = format;
    m_state = state;

    QString text = m_currentRootObject
            ? m_currentRootObject->property("currentText").toString()
            : QStringLiteral("Hello Qt");

    QUrl source("qrc:qquickviewcomparison/test.qml");

    if (m_state == UseWindow) {
        QQuickView *quickView = new QQuickView;
        // m_transparent is not supported here since many systems have problems with semi-transparent child windows
        quickView->setFormat(m_format);
        quickView->setResizeMode(QQuickView::SizeRootObjectToView);
        connect(quickView, &QQuickView::statusChanged, this, &MainWindow::onStatusChangedView);
        connect(quickView, &QQuickView::sceneGraphError, this, &MainWindow::onSceneGraphError);
        quickView->setSource(source);
        m_currentRootObject = quickView->rootObject();
        switchTo(QWidget::createWindowContainer(quickView));
    } else if (m_state == UseWidget) {
        QQuickWidget *quickWidget = new QQuickWidget;
        if (m_transparent)
            quickWidget->setClearColor(Qt::transparent);
        quickWidget->setFormat(m_format);
        quickWidget->setResizeMode(QQuickWidget::SizeRootObjectToView);
        connect(quickWidget, &QQuickWidget::statusChanged, this, &MainWindow::onStatusChangedWidget);
        connect(quickWidget, &QQuickWidget::sceneGraphError, this, &MainWindow::onSceneGraphError);
        quickWidget->setSource(source);
        m_currentRootObject = quickWidget->rootObject();
        switchTo(quickWidget);
    }

    if (m_currentRootObject) {
        m_currentRootObject->setProperty("currentText", text);
        m_currentRootObject->setProperty("multisample", m_checkboxMultiSample->isChecked());
        if (!QCoreApplication::arguments().contains(QStringLiteral("--no_render_alpha")))
            m_currentRootObject->setProperty("translucency", m_transparent);
    }

    m_overlayLabel->raise();
}

void MainWindow::onStatusChangedView(QQuickView::Status status)
{
    QString s;
    switch (status) {
    case QQuickView::Null:
        s = tr("Null");
        break;
    case QQuickView::Ready:
        s = tr("Ready");
        break;
    case QQuickView::Loading:
        s = tr("Loading");
        break;
    case QQuickView::Error:
        s = tr("Error");
        break;
    default:
        s = tr("Unknown");
        break;
    }
    m_labelStatus->setText(tr("QQuickView status: %1").arg(s));
}

void MainWindow::onStatusChangedWidget(QQuickWidget::Status status)
{
    QString s;
    switch (status) {
    case QQuickWidget::Null:
        s = tr("Null");
        break;
    case QQuickWidget::Ready:
        s = tr("Ready");
        break;
    case QQuickWidget::Loading:
        s = tr("Loading");
        break;
    case QQuickWidget::Error:
        s = tr("Error");
        break;
    default:
        s = tr("Unknown");
        break;
    }
    m_labelStatus->setText(tr("QQuickWidget status: %1").arg(s));
}

void MainWindow::onSceneGraphError(QQuickWindow::SceneGraphError error, const QString &message)
{
    m_labelStatus->setText(tr("Scenegraph error %1: %2").arg(error).arg(message));
}
