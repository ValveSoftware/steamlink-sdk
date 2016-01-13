/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
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
**   * Neither the name of Digia Plc and its Subsidiary(-ies) nor the names
**     of its contributors may be used to endorse or promote products derived
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
#include <QQmlError>
#include <QtWidgets>

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow();

private slots:
    void quickWidgetStatusChanged(QQuickWidget::Status);
    void sceneGraphError(QQuickWindow::SceneGraphError error, const QString &message);
    void grabToFile();
    void renderToFile();

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
    centralWidget ->addSubWindow(lcd);

    QUrl source("qrc:quickwidget/rotatingsquare.qml");

    connect(m_quickWidget, SIGNAL(statusChanged(QQuickWidget::Status)),
            this, SLOT(quickWidgetStatusChanged(QQuickWidget::Status)));
    connect(m_quickWidget, SIGNAL(sceneGraphError(QQuickWindow::SceneGraphError,QString)),
            this, SLOT(sceneGraphError(QQuickWindow::SceneGraphError,QString)));
    m_quickWidget->resize(300,300);
    m_quickWidget->setResizeMode(QQuickWidget::SizeRootObjectToView );
    m_quickWidget->setSource(source);

    centralWidget ->addSubWindow(m_quickWidget);

    setCentralWidget(centralWidget);

    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    QAction *grabAction = fileMenu->addAction(tr("Grab to image"));
    connect(grabAction, SIGNAL(triggered()), this, SLOT(grabToFile()));
    QAction *renderAction = fileMenu->addAction(tr("Render to pixmap"));
    connect(renderAction, SIGNAL(triggered()), this, SLOT(renderToFile()));
    QAction *quitAction = fileMenu->addAction(tr("Quit"));
    connect(quitAction, SIGNAL(triggered()), qApp, SLOT(quit()));
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
    QString t;
    QFileDialog fd(parent, t, QString());
    fd.setAcceptMode(QFileDialog::AcceptSave);
    fd.setDefaultSuffix("png");
    fd.selectFile("test.png");
    if (fd.exec() == QDialog::Accepted)
        saveable->save(fd.selectedFiles().first());
}

void MainWindow::grabToFile()
{
    QImage image = m_quickWidget->grabFramebuffer();
    saveToFile(this, &image);
}

void MainWindow::renderToFile()
{
    QPixmap pixmap(m_quickWidget->size());
    m_quickWidget->render(&pixmap);
    saveToFile(this, &pixmap);
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    MainWindow mainWindow;
    mainWindow.show();

    return app.exec();
}

#include "main.moc"
