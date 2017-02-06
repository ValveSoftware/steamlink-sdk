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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "ui_mainwindow.h"

QT_BEGIN_NAMESPACE

class InvokeMethod;
class ChangeProperties;
class AmbientProperties;
class QAxScriptManager;
class QAxWidget;
class QMdiArea;

QT_END_NAMESPACE

QT_USE_NAMESPACE


class MainWindow : public QMainWindow, public Ui::MainWindow
{
    Q_OBJECT
public:
    MainWindow(QWidget *parent = 0);
    ~MainWindow();

    static MainWindow *instance() { return m_instance; }

    bool addControlFromClsid(const QString &clsid);
    bool addControlFromFile(const QString &fileName);
    bool loadScript(const QString &file);

protected:
    void closeEvent(QCloseEvent *) Q_DECL_OVERRIDE;

public slots:
    void appendLogText(const QString &);

protected slots:
    void on_actionFileNew_triggered();
    void on_actionFileLoad_triggered();
    void on_actionFileSave_triggered();

    void on_actionContainerSet_triggered();
    void on_actionContainerClear_triggered();
    void on_actionContainerProperties_triggered();

    void on_actionControlInfo_triggered();
    void on_actionControlDocumentation_triggered();
    void on_actionControlPixmap_triggered();
    void on_actionControlProperties_triggered();
    void on_actionControlMethods_triggered();
    void on_VerbMenu_aboutToShow();

    void on_actionScriptingLoad_triggered();
    void on_actionScriptingRun_triggered();

    void on_actionFreeUnusedDLLs_triggered();

private:
    QAxWidget *activeAxWidget() const;
    QList<QAxWidget *> axWidgets() const;

    static MainWindow *m_instance;

    InvokeMethod *m_dlgInvoke;
    ChangeProperties *m_dlgProperties;
    AmbientProperties *m_dlgAmbient;
    QAxScriptManager *m_scripts;
    QMdiArea *m_mdiArea;

    QtMessageHandler m_oldDebugHandler;

private slots:
    void updateGUI();
    void logPropertyChanged(const QString &prop);
    void logSignal(const QString &signal, int argc, void *argv);
    void logException(int code, const QString&source, const QString&desc, const QString&help);
    void logMacro(int code, const QString &description, int sourcePosition, const QString &sourceText);

    void on_VerbMenu_triggered(QAction *action);
};

#endif // MAINWINDOW_H
