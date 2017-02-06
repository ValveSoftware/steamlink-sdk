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
#include "exportdialog.h"

#include <QtWidgets>
#include <QSvgRenderer>

#include "svgview.h"

static inline QString picturesLocation()
{
    return QStandardPaths::standardLocations(QStandardPaths::PicturesLocation).value(0, QDir::currentPath());
}

MainWindow::MainWindow()
    : QMainWindow()
    , m_view(new SvgView)
{
    QToolBar *toolBar = new QToolBar(this);
    addToolBar(Qt::TopToolBarArea, toolBar);

    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    const QIcon openIcon = QIcon::fromTheme("document-open", QIcon(":/qt-project.org/styles/commonstyle/images/standardbutton-open-32.png"));
    QAction *openAction = fileMenu->addAction(openIcon, tr("&Open..."), this, &MainWindow::openFile);
    openAction->setShortcut(QKeySequence::Open);
    toolBar->addAction(openAction);
    const QIcon exportIcon = QIcon::fromTheme("document-save", QIcon(":/qt-project.org/styles/commonstyle/images/standardbutton-save-32.png"));
    QAction *exportAction = fileMenu->addAction(exportIcon, tr("&Export..."), this, &MainWindow::exportImage);
    exportAction->setToolTip(tr("Export Image"));
    exportAction->setShortcut(Qt::CTRL + Qt::Key_E);
    toolBar->addAction(exportAction);
    QAction *quitAction = fileMenu->addAction(tr("E&xit"), qApp, QCoreApplication::quit);
    quitAction->setShortcuts(QKeySequence::Quit);

    QMenu *viewMenu = menuBar()->addMenu(tr("&View"));
    m_backgroundAction = viewMenu->addAction(tr("&Background"));
    m_backgroundAction->setEnabled(false);
    m_backgroundAction->setCheckable(true);
    m_backgroundAction->setChecked(false);
    connect(m_backgroundAction, &QAction::toggled, m_view, &SvgView::setViewBackground);

    m_outlineAction = viewMenu->addAction(tr("&Outline"));
    m_outlineAction->setEnabled(false);
    m_outlineAction->setCheckable(true);
    m_outlineAction->setChecked(true);
    connect(m_outlineAction, &QAction::toggled, m_view, &SvgView::setViewOutline);

    QMenu *rendererMenu = menuBar()->addMenu(tr("&Renderer"));
    m_nativeAction = rendererMenu->addAction(tr("&Native"));
    m_nativeAction->setCheckable(true);
    m_nativeAction->setChecked(true);
    m_nativeAction->setData(int(SvgView::Native));
#ifndef QT_NO_OPENGL
    m_glAction = rendererMenu->addAction(tr("&OpenGL"));
    m_glAction->setCheckable(true);
    m_glAction->setData(int(SvgView::OpenGL));
#endif
    m_imageAction = rendererMenu->addAction(tr("&Image"));
    m_imageAction->setCheckable(true);
    m_imageAction->setData(int(SvgView::Image));

    rendererMenu->addSeparator();
    m_highQualityAntialiasingAction = rendererMenu->addAction(tr("&High Quality Antialiasing"));
    m_highQualityAntialiasingAction->setEnabled(false);
    m_highQualityAntialiasingAction->setCheckable(true);
    m_highQualityAntialiasingAction->setChecked(false);
    connect(m_highQualityAntialiasingAction, &QAction::toggled, m_view, &SvgView::setHighQualityAntialiasing);
#ifdef QT_NO_OPENGL
    m_highQualityAntialiasingAction->setVisible(false);
#endif

    QActionGroup *rendererGroup = new QActionGroup(this);
    rendererGroup->addAction(m_nativeAction);
#ifndef QT_NO_OPENGL
    rendererGroup->addAction(m_glAction);
#endif
    rendererGroup->addAction(m_imageAction);

    menuBar()->addMenu(rendererMenu);

    connect(rendererGroup, &QActionGroup::triggered,
            [this] (QAction *a) { setRenderer(a->data().toInt()); });

    QMenu *help = menuBar()->addMenu(tr("&Help"));
    help->addAction(tr("About Qt"), qApp, &QApplication::aboutQt);

    setCentralWidget(m_view);
}

void MainWindow::openFile()
{
    QFileDialog fileDialog(this);
    fileDialog.setAcceptMode(QFileDialog::AcceptOpen);
    fileDialog.setMimeTypeFilters(QStringList() << "image/svg+xml" << "image/svg+xml-compressed");
    fileDialog.setWindowTitle(tr("Open SVG File"));
    if (m_currentPath.isEmpty())
        fileDialog.setDirectory(picturesLocation());

    while (fileDialog.exec() == QDialog::Accepted && !loadFile(fileDialog.selectedFiles().constFirst()))
        ;
}

bool MainWindow::loadFile(const QString &fileName)
{
    if (!QFileInfo::exists(fileName) || !m_view->openFile(fileName)) {
        QMessageBox::critical(this, tr("Open SVG File"),
                              tr("Could not open file '%1'.").arg(QDir::toNativeSeparators(fileName)));
        return false;
    }

    if (!fileName.startsWith(":/")) {
        m_currentPath = fileName;
        setWindowFilePath(fileName);
        const QSize size = m_view->svgSize();
        const QString message =
            tr("Opened %1, %2x%3").arg(QFileInfo(fileName).fileName()).arg(size.width()).arg(size.width());
        statusBar()->showMessage(message);
    }

    m_outlineAction->setEnabled(true);
    m_backgroundAction->setEnabled(true);

    const QSize availableSize = QApplication::desktop()->availableGeometry(this).size();
    resize(m_view->sizeHint().expandedTo(availableSize / 4) + QSize(80, 80 + menuBar()->height()));

    return true;
}

void MainWindow::setRenderer(int renderMode)
{

    m_highQualityAntialiasingAction->setEnabled(renderMode == SvgView::OpenGL);
    m_view->setRenderer(static_cast<SvgView::RendererType>(renderMode));
}

void MainWindow::exportImage()
{
    ExportDialog exportDialog(this);
    exportDialog.setExportSize(m_view->svgSize());
    QString fileName;
    if (m_currentPath.isEmpty()) {
        fileName = picturesLocation() + QLatin1String("/export.png");
    } else {
        const QFileInfo fi(m_currentPath);
        fileName = fi.absolutePath() + QLatin1Char('/') + fi.baseName() + QLatin1String(".png");
    }
    exportDialog.setExportFileName(fileName);

    while (true) {
        if (exportDialog.exec() != QDialog::Accepted)
            break;

        const QSize imageSize = exportDialog.exportSize();
        QImage image(imageSize, QImage::Format_ARGB32);
        image.fill(Qt::transparent);
        QPainter painter;
        painter.begin(&image);
        m_view->renderer()->render(&painter, QRectF(QPointF(), QSizeF(imageSize)));
        painter.end();

        const QString fileName = exportDialog.exportFileName();
        if (image.save(fileName)) {

            const QString message = tr("Exported %1, %2x%3, %4 bytes")
                .arg(QDir::toNativeSeparators(fileName)).arg(imageSize.width()).arg(imageSize.height())
                .arg(QFileInfo(fileName).size());
            statusBar()->showMessage(message);
            break;
        } else {
            QMessageBox::critical(this, tr("Export Image"),
                                  tr("Could not write file '%1'.").arg(QDir::toNativeSeparators(fileName)));
        }
    }
}
