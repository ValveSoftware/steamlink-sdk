/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the demonstration applications of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
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
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** As a special exception, The Qt Company gives you certain additional
** rights. These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file. Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "ui_mainwindow.h"
#include "ui_cookiewidget.h"
#include "ui_cookiedialog.h"
#include <QNetworkCookie>
#include <QMainWindow>

QT_BEGIN_NAMESPACE
class QWebEngineCookieStore;
QT_END_NAMESPACE

class CookieDialog : public QDialog, public Ui_CookieDialog
{
    Q_OBJECT
public:
    CookieDialog(const QNetworkCookie &cookie, QWidget *parent = nullptr);
    CookieDialog(QWidget *parent = 0);
    QNetworkCookie cookie();
};

class CookieWidget : public QWidget,  public Ui_CookieWidget
{
    Q_OBJECT
public:
    CookieWidget(const QNetworkCookie &cookie, QWidget *parent = nullptr);
    void setHighlighted(bool enabled);
signals:
    void deleteClicked();
    void viewClicked();
};

class MainWindow : public QMainWindow, public Ui_MainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(const QUrl &url);

private:
    bool containsCookie(const QNetworkCookie &cookie);

private slots:
    void handleCookieAdded(const QNetworkCookie &cookie);
    void handleDeleteAllClicked();
    void handleNewClicked();
    void handleUrlClicked();

private:
    QWebEngineCookieStore *m_store;
    QVector<QNetworkCookie> m_cookies;
    QVBoxLayout *m_layout;
};

#endif // MAINWINDOW_H
