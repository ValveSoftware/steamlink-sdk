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
#include <QMainWindow>
#include <QPlainTextEdit>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QStyle>
#include <QScreen>
#include <QShortcut>
#include <QPixmap>
#include <QPainter>
#include <QFontMetrics>
#include <QtWinExtras>
#include <QDebug>

static QPixmap drawColoredPixmap(const QSize &size, const QColor &color,
                                 const QString &text = QString())
{
    QPixmap result(size);
    result.fill(color);
    QPainter painter(&result);
    painter.drawRect(QRect(QPoint(0, 0), size - QSize(1, 1)));
    if (!text.isEmpty()) {
        QFont font = painter.font();
        font.setPointSize(20);
        painter.setFont(font);
        const QFontMetrics fontMetrics(font);
        QRect boundingRect(fontMetrics.boundingRect(text));
        const int x = (size.width() - boundingRect.width()) / 2;
        const int y = size.height() - (size.height() - boundingRect.height()) / 2;
        painter.drawText(x, y, text);
    }
    return result;
}

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow();
    void initThumbnailToolBar();

public slots:
    void testButtonClicked();
    void updateIconicThumbnailPixmap();
    void updateIconicLivePreviewPixmap();
    void logText(const QString &text);

private:
    QWinThumbnailToolBar *m_thumbnailToolBar;
    QPlainTextEdit *m_logEdit;
    QAction *m_enableIconicPixmapAction;
    QAction *m_enableIconicLivePreviewAction;
};

MainWindow::MainWindow()
    : m_thumbnailToolBar(new QWinThumbnailToolBar(this))
    , m_logEdit(new QPlainTextEdit)
    , m_enableIconicPixmapAction(new QAction("Enable Iconic Pixmap", this))
    , m_enableIconicLivePreviewAction(new QAction("Enable LivePreview", this))
{
    setMinimumWidth(400);
    setWindowTitle(QStringLiteral("QWinThumbnailToolBar ") + QLatin1String(QT_VERSION_STR));
    QMenu *fileMenu = menuBar()->addMenu("&File");
    m_enableIconicPixmapAction->setCheckable(true);
    m_enableIconicPixmapAction->setChecked(true);
    m_enableIconicLivePreviewAction->setCheckable(true);
    m_enableIconicLivePreviewAction->setChecked(true);
    fileMenu->addAction(m_enableIconicPixmapAction);
    fileMenu->addAction(m_enableIconicLivePreviewAction);
    QAction *quitAction = fileMenu->addAction("&Quit");
    quitAction->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_Q));
    connect(quitAction, &QAction::triggered, QCoreApplication::quit);
    setCentralWidget(m_logEdit);
}

void MainWindow::initThumbnailToolBar()
{
    m_thumbnailToolBar->setWindow(windowHandle());
    QWinThumbnailToolButton *testButton = new QWinThumbnailToolButton(m_thumbnailToolBar);
    testButton->setToolTip("Test");
    testButton->setIcon(style()->standardIcon(QStyle::SP_ComputerIcon));
    connect(testButton, &QWinThumbnailToolButton::clicked, this, &MainWindow::testButtonClicked);
    m_thumbnailToolBar->addButton(testButton);
    m_thumbnailToolBar->setIconicPixmapNotificationsEnabled(true);
    connect(m_thumbnailToolBar, &QWinThumbnailToolBar::iconicLivePreviewPixmapRequested,
            this, &MainWindow::updateIconicLivePreviewPixmap);
    connect(m_thumbnailToolBar, &QWinThumbnailToolBar::iconicThumbnailPixmapRequested,
            this, &MainWindow::updateIconicThumbnailPixmap);
}

void MainWindow::logText(const QString &text)
{
    m_logEdit->appendPlainText(text);
    qDebug("%s", qPrintable(text));
}

void MainWindow::testButtonClicked()
{
    static int n = 1;
    logText(QStringLiteral("Clicked #") + QString::number(n++));
}

void MainWindow::updateIconicThumbnailPixmap()
{
    static int n = 1;
    if (!m_enableIconicPixmapAction->isChecked())
        return;
    const QString number = QString::number(n++);
    logText(QLatin1String(__FUNCTION__) + QLatin1Char(' ') + number);
    const QPixmap pixmap =
        drawColoredPixmap(QSize(200, 50), Qt::yellow, QStringLiteral("ITP ") + number);
    m_thumbnailToolBar->setIconicThumbnailPixmap(pixmap);
}

void MainWindow::updateIconicLivePreviewPixmap()
{
    static int n = 1;
    if (!m_enableIconicLivePreviewAction->isChecked())
        return;
    const QString number = QString::number(n++);
    logText(QLatin1String(__FUNCTION__) + QLatin1Char(' ') + number);
    const QPixmap pixmap =
        drawColoredPixmap(QSize(200, 50), Qt::red, QStringLiteral("ILP ") + number);
    m_thumbnailToolBar->setIconicLivePreviewPixmap(pixmap);
}

int main(int argc, char* argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    w.move(QGuiApplication::primaryScreen()->availableGeometry().center()
           - QPoint(w.width() / 2, w.height() / 2));
    w.show();
    w.initThumbnailToolBar();
    return a.exec();
}

#include "main.moc"
