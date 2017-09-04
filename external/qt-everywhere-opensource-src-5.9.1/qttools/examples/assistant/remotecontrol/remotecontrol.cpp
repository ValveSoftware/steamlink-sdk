/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
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

#include <QtCore/QDir>
#include <QtCore/QProcess>
#include <QtCore/QTextStream>
#include <QtCore/QLibraryInfo>

#include <QtWidgets/QMessageBox>

#include "remotecontrol.h"

RemoteControl::RemoteControl(QWidget *parent, Qt::WindowFlags flags)
        : QMainWindow(parent, flags)
{
    ui.setupUi(this);
    connect(ui.indexLineEdit, SIGNAL(returnPressed()),
        this, SLOT(on_indexButton_clicked()));
    connect(ui.identifierLineEdit, SIGNAL(returnPressed()),
        this, SLOT(on_identifierButton_clicked()));
    connect(ui.urlLineEdit, SIGNAL(returnPressed()),
        this, SLOT(on_urlButton_clicked()));

    QString rc;
    QTextStream(&rc) << QLatin1String("qthelp://org.qt-project.qtdoc.")
                     << (QT_VERSION >> 16) << ((QT_VERSION >> 8) & 0xFF)
                     << (QT_VERSION & 0xFF)
                     << QLatin1String("/qtdoc/index.html");

    ui.startUrlLineEdit->setText(rc);

    process = new QProcess(this);
    connect(process, SIGNAL(finished(int,QProcess::ExitStatus)),
        this, SLOT(helpViewerClosed()));
}

RemoteControl::~RemoteControl()
{
    if (process->state() == QProcess::Running) {
        process->terminate();
        process->waitForFinished(3000);
    }
}

void RemoteControl::on_actionQuit_triggered()
{
    close();
}

void RemoteControl::on_launchButton_clicked()
{
    if (process->state() == QProcess::Running)
        return;

    QString app = QLibraryInfo::location(QLibraryInfo::BinariesPath) + QDir::separator();
#if !defined(Q_OS_MAC)
    app += QLatin1String("assistant");
#else
    app += QLatin1String("Assistant.app/Contents/MacOS/Assistant");
#endif

    ui.contentsCheckBox->setChecked(true);
    ui.indexCheckBox->setChecked(true);
    ui.bookmarksCheckBox->setChecked(true);

    QStringList args;
    args << QLatin1String("-enableRemoteControl");
    process->start(app, args);
    if (!process->waitForStarted()) {
        QMessageBox::critical(this, tr("Remote Control"),
            tr("Could not start Qt Assistant from %1.").arg(app));
        return;
    }

    if (!ui.startUrlLineEdit->text().isEmpty())
        sendCommand(QLatin1String("SetSource ")
            + ui.startUrlLineEdit->text());

    ui.launchButton->setEnabled(false);
    ui.startUrlLineEdit->setEnabled(false);
    ui.actionGroupBox->setEnabled(true);
}

void RemoteControl::sendCommand(const QString &cmd)
{
    if (process->state() != QProcess::Running)
        return;
    process->write(cmd.toLocal8Bit() + '\n');
}

void RemoteControl::on_indexButton_clicked()
{
    sendCommand(QLatin1String("ActivateKeyword ")
        + ui.indexLineEdit->text());
}

void RemoteControl::on_identifierButton_clicked()
{
    sendCommand(QLatin1String("ActivateIdentifier ")
        + ui.identifierLineEdit->text());
}

void RemoteControl::on_urlButton_clicked()
{
    sendCommand(QLatin1String("SetSource ")
        + ui.urlLineEdit->text());
}

void RemoteControl::on_syncContentsButton_clicked()
{
    sendCommand(QLatin1String("SyncContents"));
}

void RemoteControl::on_contentsCheckBox_toggled(bool checked)
{
    sendCommand(checked ?
        QLatin1String("Show Contents") : QLatin1String("Hide Contents"));
}

void RemoteControl::on_indexCheckBox_toggled(bool checked)
{
    sendCommand(checked ?
        QLatin1String("Show Index") : QLatin1String("Hide Index"));
}

void RemoteControl::on_bookmarksCheckBox_toggled(bool checked)
{
    sendCommand(checked ?
        QLatin1String("Show Bookmarks") : QLatin1String("Hide Bookmarks"));
}

void RemoteControl::helpViewerClosed()
{
    ui.launchButton->setEnabled(true);
    ui.startUrlLineEdit->setEnabled(true);
    ui.actionGroupBox->setEnabled(false);
}
