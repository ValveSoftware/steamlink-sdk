/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Designer of the Qt Toolkit.
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

#ifndef QDESIGNER_H
#define QDESIGNER_H

#include <QtCore/QPointer>
#include <QtWidgets/QApplication>

QT_BEGIN_NAMESPACE

#define qDesigner \
    (static_cast<QDesigner*>(QCoreApplication::instance()))

class QDesignerWorkbench;
class QDesignerToolWindow;
class MainWindowBase;
class QDesignerServer;
class QDesignerClient;
class QErrorMessage;
class QCommandLineParser;
struct Options;

class QDesigner: public QApplication
{
    Q_OBJECT
public:
    enum ParseArgumentsResult {
        ParseArgumentsSuccess,
        ParseArgumentsError,
        ParseArgumentsHelpRequested
    };

    QDesigner(int &argc, char **argv);
    virtual ~QDesigner();

    ParseArgumentsResult parseCommandLineArguments();

    QDesignerWorkbench *workbench() const;
    QDesignerServer *server() const;
    MainWindowBase *mainWindow() const;
    void setMainWindow(MainWindowBase *tw);

protected:
    bool event(QEvent *ev);

signals:
    void initialized();

public slots:
    void showErrorMessage(const char *message);

private slots:
    void callCreateForm();

private:
    void showErrorMessageBox(const QString &);

    QDesignerServer *m_server;
    QDesignerClient *m_client;
    QDesignerWorkbench *m_workbench;
    QPointer<MainWindowBase> m_mainWindow;
    QPointer<QErrorMessage> m_errorMessageDialog;

    QString m_initializationErrors;
    QString m_lastErrorMessage;
    bool m_suppressNewFormShow;
};

QT_END_NAMESPACE

#endif // QDESIGNER_H
