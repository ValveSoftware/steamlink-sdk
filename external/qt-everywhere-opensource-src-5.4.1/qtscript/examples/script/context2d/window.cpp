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

#include "window.h"
#include "environment.h"
#include "context2d.h"
#include "qcontext2dcanvas.h"
#include <QHBoxLayout>
#include <QListWidget>
#include <QDir>
#include <QMessageBox>

#ifndef QT_NO_SCRIPTTOOLS
#include <QAction>
#include <QApplication>
#include <QMainWindow>
#include <QPushButton>
#include <QVBoxLayout>
#include <QScriptEngineDebugger>
#endif

static QString scriptsDir()
{
    if (QFile::exists("./scripts"))
        return "./scripts";
    return ":/scripts";
}

//! [0]
Window::Window(QWidget *parent)
    : QWidget(parent)
#ifndef QT_NO_SCRIPTTOOLS
      , m_debugger(0), m_debugWindow(0)
#endif
{
    m_env = new Environment(this);
    QObject::connect(m_env, SIGNAL(scriptError(QScriptValue)),
                     this, SLOT(reportScriptError(QScriptValue)));

    Context2D *context = new Context2D(this);
    context->setSize(150, 150);
    m_canvas = new QContext2DCanvas(context, m_env, this);
    m_canvas->setFixedSize(context->size());
    m_canvas->setObjectName("tutorial");
    m_env->addCanvas(m_canvas);
//! [0]

#ifndef QT_NO_SCRIPTTOOLS
    QVBoxLayout *vbox = new QVBoxLayout();
    vbox->addWidget(m_canvas);
    m_debugButton = new QPushButton(tr("Run in Debugger"));
    connect(m_debugButton, SIGNAL(clicked()), this, SLOT(runInDebugger()));
    vbox->addWidget(m_debugButton);
#endif

    QHBoxLayout *hbox = new QHBoxLayout(this);
    m_view = new QListWidget(this);
    m_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    hbox->addWidget(m_view);
#ifndef QT_NO_SCRIPTTOOLS
    hbox->addLayout(vbox);
#else
    hbox->addWidget(m_canvas);
#endif

//! [1]
    QDir dir(scriptsDir());
    QFileInfoList entries = dir.entryInfoList(QStringList() << "*.js");
    for (int i = 0; i < entries.size(); ++i)
        m_view->addItem(entries.at(i).fileName());
    connect(m_view, SIGNAL(currentItemChanged(QListWidgetItem*,QListWidgetItem*)),
            this, SLOT(selectScript(QListWidgetItem*)));
//! [1]

    setWindowTitle(tr("Context 2D"));
}

//! [2]
void Window::selectScript(QListWidgetItem *item)
{
    QString fileName = item->text();
    runScript(fileName, /*debug=*/false);
}
//! [2]

void Window::reportScriptError(const QScriptValue &error)
{
    QMessageBox::warning(this, tr("Context 2D"), tr("Line %0: %1")
                         .arg(error.property("lineNumber").toInt32())
                         .arg(error.toString()));
}

#ifndef QT_NO_SCRIPTTOOLS
//! [3]
void Window::runInDebugger()
{
    QListWidgetItem *item = m_view->currentItem();
    if (item) {
        QString fileName = item->text();
        runScript(fileName, /*debug=*/true);
    }
}
//! [3]
#endif

//! [4]
void Window::runScript(const QString &fileName, bool debug)
{
    QFile file(scriptsDir() + "/" + fileName);
    file.open(QIODevice::ReadOnly);
    QString contents = file.readAll();
    file.close();
    m_env->reset();

#ifndef QT_NO_SCRIPTTOOLS
    if (debug) {
        if (!m_debugger) {
            m_debugger = new QScriptEngineDebugger(this);
            m_debugWindow = m_debugger->standardWindow();
            m_debugWindow->setWindowModality(Qt::ApplicationModal);
            m_debugWindow->resize(1280, 704);
        }
        m_debugger->attachTo(m_env->engine());
        m_debugger->action(QScriptEngineDebugger::InterruptAction)->trigger();
    } else {
        if (m_debugger)
            m_debugger->detach();
    }
#else
    Q_UNUSED(debug);
#endif

    QScriptValue ret = m_env->evaluate(contents, fileName);

#ifndef QT_NO_SCRIPTTOOLS
    if (m_debugWindow)
        m_debugWindow->hide();
#endif

    if (ret.isError())
        reportScriptError(ret);
}
//! [4]
