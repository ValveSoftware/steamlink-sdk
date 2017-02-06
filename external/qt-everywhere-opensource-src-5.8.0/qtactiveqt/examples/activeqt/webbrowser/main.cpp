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

#include <QApplication>
#include <QMessageBox>
#include <QProgressBar>
#include <QLineEdit>
#include <QLabel>
#include <QStatusBar>
#include <QMainWindow>
#include <QDesktopWidget>
#include <QAbstractEventDispatcher>
#include <QSignalMapper>
#include <QVariant>
#include <QSettings>

#include "ui_mainwindow.h"

static const char qtUrl[] = "qt.io";
static const char iWebBrowser2DocumentationUrl[] = "http://msdn.microsoft.com/en-us/library/aa752127%28v=vs.85%29.aspx";
static const char versionKey[] = "Version";
static const char geometryKey[] = "Geometry";

struct Location {
    Location(const QString &t = QString(), const QString &a = QString()) : title(t), address(a) {}

    QString title;
    QString address;
};

Q_DECLARE_METATYPE(Location)

static QList<Location> defaultBookmarks()
{
    QList<Location> result;
    result.append(Location(QStringLiteral("Qt"), QLatin1String(qtUrl)));
    result.append(Location(QStringLiteral("Digia"), QStringLiteral("http://qt.digia.com/")));
    result.append(Location(QStringLiteral("IWebBrowser2 MSDN Documentation"), QLatin1String(iWebBrowser2DocumentationUrl)));
    return result;
}

static bool containsAddress(const QList<Location> &locations, const QString &address)
{
    foreach (const Location &location, locations) {
        if (location.address == address)
            return true;
    }
    return false;
}

static inline Location locationFromAction(const QAction *action)
{
    return action->data().value<Location>();
}

static QList<Location> readBookMarks(QSettings &settings)
{
    QList<Location> result;
    if (const int count = settings.beginReadArray(QStringLiteral("Bookmarks"))) {
        const QString titleKey = QStringLiteral("title");
        const QString addressKey = QStringLiteral("address");
        for (int i = 0; i < count; ++i) {
            settings.setArrayIndex(i);
            result.append(Location(settings.value(titleKey).toString(),
                                   settings.value(addressKey).toString()));
        }
    }
    settings.endArray();
    return result;
}

static void saveBookMarks(const QList<Location> &bookmarks, QSettings &settings)
{
    const int count = bookmarks.size();
    settings.beginWriteArray(QStringLiteral("Bookmarks"));
    const QString titleKey = QStringLiteral("title");
    const QString addressKey = QStringLiteral("address");
    for (int i = 0; i < count; ++i) {
        settings.setArrayIndex(i);
        settings.setValue(titleKey, bookmarks.at(i).title);
        settings.setValue(addressKey, bookmarks.at(i).address);
    }
    settings.endArray();
}

//! [0]
class MainWindow : public QMainWindow, public Ui::MainWindow
{
    Q_OBJECT
public:
    MainWindow();
    ~MainWindow();

public slots:
    void navigate(const QString &address);
    void on_WebBrowser_TitleChange(const QString &title);
    void on_WebBrowser_ProgressChange(int a, int b);
    void on_WebBrowser_CommandStateChange(int cmd, bool on);
    void on_WebBrowser_BeforeNavigate();
    void on_WebBrowser_NavigateComplete(const QString &address);

    void on_actionGo_triggered();
    void on_actionNewWindow_triggered();
    void on_actionAddBookmark_triggered();
    void on_actionAbout_triggered();
    void on_actionAboutQt_triggered();
    void on_actionFileClose_triggered();

private:
    inline const QString address() const
        { return addressEdit->text().trimmed(); }
    QList<Location> bookmarks() const;
    QAction *addLocation(const Location &location, QMenu *menu);
    inline void addBookmark(const Location &location)
        { bookmarkActions << addLocation(location, BookmarksMenu); }

    QProgressBar *pb;
    QLineEdit *addressEdit;
    QList<QAction *> bookmarkActions;
    QList<QAction *> historyActions;
    QSignalMapper locationActionMapper;
};
//! [0] //! [1]

MainWindow::MainWindow()
{
    setupUi(this);

    addressEdit = new QLineEdit;
    tbAddress->insertWidget(actionGo, new QLabel(tr("Address")));
    tbAddress->insertWidget(actionGo, addressEdit);

    connect(addressEdit, SIGNAL(returnPressed()), actionGo, SLOT(trigger()));
    connect(actionBack, SIGNAL(triggered()), WebBrowser, SLOT(GoBack()));
    connect(actionForward, SIGNAL(triggered()), WebBrowser, SLOT(GoForward()));
    connect(actionStop, SIGNAL(triggered()), WebBrowser, SLOT(Stop()));
    connect(actionRefresh, SIGNAL(triggered()), WebBrowser, SLOT(Refresh()));
    connect(actionHome, SIGNAL(triggered()), WebBrowser, SLOT(GoHome()));
    connect(actionSearch, SIGNAL(triggered()), WebBrowser, SLOT(GoSearch()));

    pb = new QProgressBar(statusBar());
    pb->setTextVisible(false);
    pb->hide();
    statusBar()->addPermanentWidget(pb);

    connect(&locationActionMapper, SIGNAL(mapped(QString)), this, SLOT(navigate(QString)));

    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       QCoreApplication::organizationName(), QCoreApplication::applicationName());
    const QByteArray restoredGeometry = settings.value(QLatin1String(geometryKey)).toByteArray();
    if (restoredGeometry.isEmpty() || !restoreGeometry(restoredGeometry)) {
        const QRect availableGeometry = QApplication::desktop()->availableGeometry(this);
        const QSize size = (availableGeometry.size() * 4) / 5;
        resize(size);
        move(availableGeometry.center() - QPoint(size.width(), size.height()) / 2);
    }
    const QString restoredVersion = settings.value(QLatin1String(versionKey)).toString();
    QList<Location> bookmarks = readBookMarks(settings);
    if (bookmarks.isEmpty() || restoredVersion.isEmpty())
        bookmarks = defaultBookmarks();
    foreach (const Location &bookmark, bookmarks)
        addBookmark(bookmark);
}

//! [1]

MainWindow::~MainWindow()
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       QCoreApplication::organizationName(), QCoreApplication::applicationName());
    saveBookMarks(bookmarks(), settings);
    settings.setValue(QLatin1String(versionKey), QLatin1String(QT_VERSION_STR));
    settings.setValue(QLatin1String(geometryKey), saveGeometry());
}

QAction *MainWindow::addLocation(const Location &location, QMenu *menu)
{
    QAction *action = menu->addAction(location.title);
    action->setData(QVariant::fromValue(location));
    locationActionMapper.setMapping(action, location.address);
    connect(action, SIGNAL(triggered()), &locationActionMapper, SLOT(map()));
    return action;
}

QList<Location> MainWindow::bookmarks() const
{
    QList<Location> result;
    foreach (const QAction *action, bookmarkActions)
        result.append(locationFromAction(action));
    return result;
}

void MainWindow::on_actionAddBookmark_triggered()
{
    if (!historyActions.isEmpty()) {
        const Location location = locationFromAction(historyActions.last());
        if (!containsAddress(bookmarks(), location.address))
            addBookmark(location);
    }
}

//! [2]
void MainWindow::on_WebBrowser_TitleChange(const QString &title)
{
    // This is called multiple times after NavigateComplete().
    // Add new URLs to history here.
    setWindowTitle("Qt WebBrowser - " + title);
    const QString currentAddress = address();
    const QString historyAddress = historyActions.isEmpty() ?
        QString() : locationFromAction(historyActions.last()).address;
    if (currentAddress.isEmpty() || currentAddress == "about:blank" || currentAddress == historyAddress)
        return;
    historyActions << addLocation(Location(title, currentAddress), HistoryMenu);
    if (historyActions.size() > 10)
        delete historyActions.takeFirst();
}

void MainWindow::on_WebBrowser_ProgressChange(int a, int b)
{
    if (a <= 0 || b <= 0) {
        pb->hide();
        return;
    }
    pb->show();
    pb->setRange(0, b);
    pb->setValue(a);
}

void MainWindow::on_WebBrowser_CommandStateChange(int cmd, bool on)
{
    switch (cmd) {
    case 1:
        actionForward->setEnabled(on);
        break;
    case 2:
        actionBack->setEnabled(on);
        break;
    }
}

void MainWindow::on_WebBrowser_BeforeNavigate()
{
    actionStop->setEnabled(true);
}

void MainWindow::on_WebBrowser_NavigateComplete(const QString &url)
{
    actionStop->setEnabled(false);
    const bool blocked = addressEdit->blockSignals(true);
    addressEdit->setText(url);
    addressEdit->blockSignals(blocked);
}

//! [3]
void MainWindow::on_actionGo_triggered()
{
    navigate(address());
}

//! [2]

void MainWindow::navigate(const QString &url)
{
    WebBrowser->dynamicCall("Navigate(const QString&)", url);
}

void MainWindow::on_actionNewWindow_triggered()
{
    MainWindow *window = new MainWindow;
    window->show();
    if (addressEdit->text().isEmpty())
        return;
    window->addressEdit->setText(addressEdit->text());
    window->actionStop->setEnabled(true);
    window->on_actionGo_triggered();
}

void MainWindow::on_actionAbout_triggered()
{
    QMessageBox::about(this, tr("About WebBrowser"),
                tr("This Example has been created using the ActiveQt integration into Qt Designer.\n"
                   "It demonstrates the use of QAxWidget to embed the Internet Explorer ActiveX\n"
                   "control into a Qt application."));
}

void MainWindow::on_actionAboutQt_triggered()
{
    QMessageBox::aboutQt(this, tr("About Qt"));
}

void MainWindow::on_actionFileClose_triggered()
{
    close();
}

#include "main.moc"

//! [3] //! [4]
int main(int argc, char ** argv)
{
    QApplication a(argc, argv);
    QCoreApplication::setApplicationVersion(QT_VERSION_STR);
    QCoreApplication::setApplicationName("Active Qt Web Browser");
    QCoreApplication::setOrganizationName("QtProject");
    MainWindow w;
    const QStringList arguments = QCoreApplication::arguments();
    const QString url = arguments.size() > 1 ?
        arguments.at(1) : QString::fromLatin1(qtUrl);
    w.navigate(url);
    w.show();
    return a.exec();
}
//! [4]
