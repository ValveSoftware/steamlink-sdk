/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
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

#include "menus.h"
#include <QAction>
#include <QAxFactory>
#include <QMenuBar>
#include <QMessageBox>
#include <QTextEdit>
#include <QPixmap>

#include "fileopen.xpm"
#include "filesave.xpm"

QMenus::QMenus(QWidget *parent)
    : QMainWindow(parent, 0) // QMainWindow's default flag is WType_TopLevel
{
    QAction *action;

    QMenu *file = new QMenu(this);

    action = new QAction(QPixmap((const char**)fileopen), "&Open", this);
    action->setShortcut(tr("CTRL+O"));
    connect(action, SIGNAL(triggered()), this, SLOT(fileOpen()));
    file->addAction(action);

    action = new QAction(QPixmap((const char**)filesave),"&Save", this);
    action->setShortcut(tr("CTRL+S"));
    connect(action, SIGNAL(triggered()), this, SLOT(fileSave()));
    file->addAction(action);

    QMenu *edit = new QMenu(this);

    action = new QAction("&Normal", this);
    action->setShortcut(tr("CTRL+N"));
    action->setToolTip("Normal");
    action->setStatusTip("Toggles Normal");
    action->setCheckable(true);
    connect(action, SIGNAL(triggered()), this, SLOT(editNormal()));
    edit->addAction(action);

    action = new QAction("&Bold", this);
    action->setShortcut(tr("CTRL+B"));
    action->setCheckable(true);
    connect(action, SIGNAL(triggered()), this, SLOT(editBold()));
    edit->addAction(action);

    action = new QAction("&Underline", this);
    action->setShortcut(tr("CTRL+U"));
    action->setCheckable(true);
    connect(action, SIGNAL(triggered()), this, SLOT(editUnderline()));
    edit->addAction(action);

    QMenu *advanced = new QMenu(this);
    action = new QAction("&Font...", this);
    connect(action, SIGNAL(triggered()), this, SLOT(editAdvancedFont()));
    advanced->addAction(action);

    action = new QAction("&Style...", this);
    connect(action, SIGNAL(triggered()), this, SLOT(editAdvancedStyle()));
    advanced->addAction(action);

    edit->addMenu(advanced)->setText("&Advanced");

    edit->addSeparator();

    action = new QAction("Una&vailable", this);
    action->setShortcut(tr("CTRL+V"));
    action->setCheckable(true);
    action->setEnabled(false);
    connect(action, SIGNAL(triggered()), this, SLOT(editUnderline()));
    edit->addAction(action);

    QMenu *help = new QMenu(this);

    action = new QAction("&About...", this);
    action->setShortcut(tr("F1"));
    connect(action, SIGNAL(triggered()), this, SLOT(helpAbout()));
    help->addAction(action);

    action = new QAction("&About Qt...", this);
    connect(action, SIGNAL(triggered()), this, SLOT(helpAboutQt()));
    help->addAction(action);

    if (!QAxFactory::isServer())
        menuBar()->addMenu(file)->setText("&File");
    menuBar()->addMenu(edit)->setText("&Edit");
    menuBar()->addMenu(help)->setText("&Help");

    editor = new QTextEdit(this);
    setCentralWidget(editor);

    statusBar();
}

void QMenus::fileOpen()
{
    editor->append("File Open selected.");
}

void QMenus::fileSave()
{
    editor->append("File Save selected.");
}

void QMenus::editNormal()
{
    editor->append("Edit Normal selected.");
}

void QMenus::editBold()
{
    editor->append("Edit Bold selected.");
}

void QMenus::editUnderline()
{
    editor->append("Edit Underline selected.");
}

void QMenus::editAdvancedFont()
{
    editor->append("Edit Advanced Font selected.");
}

void QMenus::editAdvancedStyle()
{
    editor->append("Edit Advanced Style selected.");
}

void QMenus::helpAbout()
{
    QMessageBox::about(this, "About QMenus",
                       "This example implements an in-place ActiveX control with menus and status messages.");
}

void QMenus::helpAboutQt()
{
    QMessageBox::aboutQt(this);
}
