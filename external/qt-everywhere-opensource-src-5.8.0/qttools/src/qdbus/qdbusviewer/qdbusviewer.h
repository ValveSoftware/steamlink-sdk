/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the tools applications of the Qt Toolkit.
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

#ifndef QDBUSVIEWER_H
#define QDBUSVIEWER_H

#include <QtWidgets/QWidget>
#include <QtDBus/QDBusConnection>

class ServicesProxyModel;

QT_FORWARD_DECLARE_CLASS(QTableView)
QT_FORWARD_DECLARE_CLASS(QTreeView)
QT_FORWARD_DECLARE_CLASS(QTreeWidget)
QT_FORWARD_DECLARE_CLASS(QStringListModel)
QT_FORWARD_DECLARE_CLASS(QLineEdit)
QT_FORWARD_DECLARE_CLASS(QTextBrowser)
QT_FORWARD_DECLARE_CLASS(QDomDocument)
QT_FORWARD_DECLARE_CLASS(QDomElement)
QT_FORWARD_DECLARE_CLASS(QSplitter)
QT_FORWARD_DECLARE_CLASS(QSettings)

struct BusSignature
{
    QString mService, mPath, mInterface, mName;
    QString mTypeSig;
};

class QDBusViewer: public QWidget
{
    Q_OBJECT
public:
    QDBusViewer(const QDBusConnection &connection, QWidget *parent = 0);

    void saveState(QSettings *settings) const;
    void restoreState(const QSettings *settings);

public slots:
    void refresh();

private slots:
    void serviceChanged(const QModelIndex &index);
    void showContextMenu(const QPoint &);
    void connectionRequested(const BusSignature &sig);
    void callMethod(const BusSignature &sig);
    void getProperty(const BusSignature &sig);
    void setProperty(const BusSignature &sig);
    void dumpMessage(const QDBusMessage &msg);
    void refreshChildren();

    void serviceRegistered(const QString &service);
    void serviceUnregistered(const QString &service);
    void serviceOwnerChanged(const QString &name, const QString &oldOwner, const QString &newOwner);

    void activate(const QModelIndex &item);

    void logError(const QString &msg);
    void anchorClicked(const QUrl &url);

private:
    void logMessage(const QString &msg);

    QDBusConnection c;
    QString currentService;
    QTreeView *tree;
    QAction *refreshAction;
    QTreeWidget *services;
    QStringListModel *servicesModel;
    ServicesProxyModel *servicesProxyModel;
    QLineEdit *serviceFilterLine;
    QTableView *servicesView;
    QTextBrowser *log;
    QSplitter *topSplitter;
    QSplitter *splitter;
    QRegExp objectPathRegExp;
};

#endif
