/****************************************************************************
 **
 ** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
 ** Contact: http://www.qt-project.org/legal
 **
 ** This file is part of the test suite of the Qt Toolkit.
 **
 ** $QT_BEGIN_LICENSE:LGPL21$
 ** Commercial License Usage
 ** Licensees holding valid commercial Qt licenses may use this file in
 ** accordance with the commercial license agreement provided with the
 ** Software or, alternatively, in accordance with the terms contained in
 ** a written agreement between you and Digia. For licensing terms and
 ** conditions see http://qt.digia.com/licensing. For further information
 ** use the contact form at http://qt.digia.com/contact-us.
 **
 ** GNU Lesser General Public License Usage
 ** Alternatively, this file may be used under the terms of the GNU Lesser
 ** General Public License version 2.1 or version 3 as published by the Free
 ** Software Foundation and appearing in the file LICENSE.LGPLv21 and
 ** LICENSE.LGPLv3 included in the packaging of this file. Please review the
 ** following information to ensure the GNU Lesser General Public License
 ** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
 ** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
 **
 ** In addition, as a special exception, Digia gives you certain additional
 ** rights. These rights are described in the Digia Qt LGPL Exception
 ** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
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
};

MainWindow::MainWindow()
    : m_thumbnailToolBar(new QWinThumbnailToolBar(this))
    , m_logEdit(new QPlainTextEdit)
{
    setMinimumWidth(400);
    setWindowTitle(QStringLiteral("QWinThumbnailToolBar ") + QLatin1String(QT_VERSION_STR));
    QMenu *fileMenu = menuBar()->addMenu("&File");
    QAction *quitAction = fileMenu->addAction("&Quit");
    quitAction->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_Q));
    connect(quitAction, SIGNAL(triggered()), qApp, SLOT(quit()));
    setCentralWidget(m_logEdit);
}

void MainWindow::initThumbnailToolBar()
{
    m_thumbnailToolBar->setWindow(windowHandle());
    QWinThumbnailToolButton *testButton = new QWinThumbnailToolButton(m_thumbnailToolBar);
    testButton->setToolTip("Test");
    testButton->setIcon(style()->standardIcon(QStyle::SP_ComputerIcon));
    connect(testButton, SIGNAL(clicked()), this, SLOT(testButtonClicked()));
    m_thumbnailToolBar->addButton(testButton);
    m_thumbnailToolBar->setIconicPixmapNotificationsEnabled(true);
    connect(m_thumbnailToolBar, SIGNAL(iconicLivePreviewPixmapRequested()),
            this, SLOT(updateIconicLivePreviewPixmap()));
    connect(m_thumbnailToolBar, SIGNAL(iconicThumbnailPixmapRequested()),
            this, SLOT(updateIconicThumbnailPixmap()));
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
    const QString number = QString::number(n++);
    logText(QLatin1String(__FUNCTION__) + QLatin1Char(' ') + number);
    const QPixmap pixmap =
        drawColoredPixmap(QSize(200, 50), Qt::yellow, QStringLiteral("ITP ") + number);
    m_thumbnailToolBar->setIconicThumbnailPixmap(pixmap);
}

void MainWindow::updateIconicLivePreviewPixmap()
{
    static int n = 1;
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
