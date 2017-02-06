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

#include <QQuickWidget>
#include <QQuickItem>
#include <QQmlError>
#include <QtWidgets>
#include "fbitem.h"

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow();

private slots:
    void quickWidgetStatusChanged(QQuickWidget::Status);
    void sceneGraphError(QQuickWindow::SceneGraphError error, const QString &message);
    void grabFramebuffer();
    void renderToPixmap();
    void grabToImage();
    void createQuickWidgetsInTabs(QMdiArea *mdiArea);

private:
    QQuickWidget *m_quickWidget;
};

MainWindow::MainWindow()
   : m_quickWidget(new QQuickWidget)
{
    QSurfaceFormat format;
    if (QCoreApplication::arguments().contains(QStringLiteral("--coreprofile"))) {
        format.setVersion(4, 4);
        format.setProfile(QSurfaceFormat::CoreProfile);
    }
    if (QCoreApplication::arguments().contains(QStringLiteral("--multisample")))
        format.setSamples(4);
    m_quickWidget->setFormat(format);

    QMdiArea *centralWidget = new QMdiArea;

    QLCDNumber *lcd = new QLCDNumber;
    lcd->display(1337);
    lcd->setMinimumSize(250,100);
    centralWidget->addSubWindow(lcd);

    QUrl source("qrc:quickwidget/rotatingsquare.qml");

    connect(m_quickWidget, &QQuickWidget::statusChanged,
            this, &MainWindow::quickWidgetStatusChanged);
    connect(m_quickWidget, &QQuickWidget::sceneGraphError,
            this, &MainWindow::sceneGraphError);
    m_quickWidget->resize(300,300);
    m_quickWidget->setResizeMode(QQuickWidget::SizeRootObjectToView );
    m_quickWidget->setSource(source);

    centralWidget->addSubWindow(m_quickWidget);

    setCentralWidget(centralWidget);

    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(tr("Grab framebuffer"), this, &MainWindow::grabFramebuffer);
    fileMenu->addAction(tr("Render to pixmap"), this, &MainWindow::renderToPixmap);
    fileMenu->addAction(tr("Grab via grabToImage"), this, &MainWindow::grabToImage);
    fileMenu->addAction(tr("Quit"), qApp, &QCoreApplication::quit);

    QMenu *windowMenu = menuBar()->addMenu(tr("&Window"));
    windowMenu->addAction(tr("Add tab widget"), this,
                          [this, centralWidget] { createQuickWidgetsInTabs(centralWidget); });
}

void MainWindow::createQuickWidgetsInTabs(QMdiArea *mdiArea)
{
    QTabWidget *tabWidget = new QTabWidget;

    const QSize size(400, 400);

    QQuickWidget *w = new QQuickWidget;
    w->resize(size);
    w->setResizeMode(QQuickWidget::SizeRootObjectToView);
    w->setSource(QUrl("qrc:quickwidget/rotatingsquaretab.qml"));

    tabWidget->addTab(w, tr("Plain Quick content"));

    w = new QQuickWidget;
    w->resize(size);
    w->setResizeMode(QQuickWidget::SizeRootObjectToView);
    w->setSource(QUrl("qrc:quickwidget/customgl.qml"));

    tabWidget->addTab(w, tr("Custom OpenGL drawing"));

    mdiArea->addSubWindow(tabWidget);
    tabWidget->show();
}

void MainWindow::quickWidgetStatusChanged(QQuickWidget::Status status)
{
    if (status == QQuickWidget::Error) {
        QStringList errors;
        foreach (const QQmlError &error, m_quickWidget->errors())
            errors.append(error.toString());
        statusBar()->showMessage(errors.join(QStringLiteral(", ")));
    }
}

void MainWindow::sceneGraphError(QQuickWindow::SceneGraphError, const QString &message)
{
     statusBar()->showMessage(message);
}

template<class T> void saveToFile(QWidget *parent, T *saveable)
{
    QFileDialog fd(parent);
    fd.setAcceptMode(QFileDialog::AcceptSave);
    fd.setDefaultSuffix("png");
    fd.selectFile("test.png");
    if (fd.exec() == QDialog::Accepted)
        saveable->save(fd.selectedFiles().first());
}

void MainWindow::grabFramebuffer()
{
    QImage image = m_quickWidget->grabFramebuffer();
    saveToFile(this, &image);
}

void MainWindow::renderToPixmap()
{
    QPixmap pixmap(m_quickWidget->size());
    m_quickWidget->render(&pixmap);
    saveToFile(this, &pixmap);
}

void MainWindow::grabToImage()
{
    QFileDialog fd(this);
    fd.setAcceptMode(QFileDialog::AcceptSave);
    fd.setDefaultSuffix("png");
    fd.selectFile("test_grabToImage.png");
    if (fd.exec() == QDialog::Accepted) {
        QMetaObject::invokeMethod(m_quickWidget->rootObject(), "performLayerBasedGrab",
                                  Q_ARG(QVariant, fd.selectedFiles().first()));
    }
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    qmlRegisterType<FbItem>("QuickWidgetExample", 1, 0, "FbItem");

    MainWindow mainWindow;
    mainWindow.show();

    return app.exec();
}

#include "main.moc"
