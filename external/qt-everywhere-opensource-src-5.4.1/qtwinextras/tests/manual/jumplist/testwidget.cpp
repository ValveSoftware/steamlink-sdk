/****************************************************************************
 **
 ** Copyright (C) 2013 Ivan Vizir <define-true-false@yandex.com>
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

#include "testwidget.h"
#include "ui_testwidget.h"

#include <QApplication>
#include <QStyle>
#include <QFile>
#include <QFileDialog>
#include <QMessageBox>
#include <QWinJumpList>
#include <QWinJumpListItem>
#include <QWinJumpListCategory>
#include <QDebug>

TestWidget::TestWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::TestWidget)
{
    ui->setupUi(this);

    if (QCoreApplication::arguments().contains("-text"))
        ui->text->setPlainText("Hello, world!");
    if (!QCoreApplication::arguments().contains("-fullscreen"))
        ui->btnClose->hide();

    for (int i = 1; i < QCoreApplication::arguments().size(); i++) {
        const QString arg = QCoreApplication::arguments().at(i);
        if (!arg.isEmpty() && arg.at(0) != '-' && QFile(arg).exists()) {
            showFile(arg);
            break;
        }
    }

    connect(ui->btnUpdate,   SIGNAL(clicked()), SLOT(updateJumpList()));
    connect(ui->btnOpenFile, SIGNAL(clicked()), SLOT(openFile()));
    connect(ui->btnClose, SIGNAL(clicked()), qApp, SLOT(quit()));
}

TestWidget::~TestWidget()
{
    delete ui;
}

void TestWidget::changeEvent(QEvent *e)
{
    QWidget::changeEvent(e);
    switch (e->type()) {
    case QEvent::LanguageChange:
        ui->retranslateUi(this);
        break;
    default:
        break;
    }
}

void TestWidget::showFile(const QString &path)
{
    QFile file(path);
    if (file.open(QIODevice::ReadOnly|QIODevice::Text))
        ui->text->setPlainText(QString::fromUtf8(file.readAll()));
    else
        QMessageBox::warning(this, "Error", "Failed to open file");
}

void TestWidget::updateJumpList()
{
    QWinJumpList jumplist;
    jumplist.recent()->setVisible(ui->cbShowRecent->isChecked());
    jumplist.frequent()->setVisible(ui->cbShowFrequent->isChecked());
    if (ui->cbRunFullscreen->isChecked()) {
        QWinJumpListItem *item = new QWinJumpListItem(QWinJumpListItem::Link);
        item->setTitle(ui->cbRunFullscreen->text());
        item->setFilePath(QDir::toNativeSeparators(QCoreApplication::applicationFilePath()));
        item->setArguments(QStringList("-fullscreen"));
        item->setIcon(style()->standardIcon(QStyle::SP_TitleBarMaxButton));
        jumplist.tasks()->addItem(item);
    }
    if (ui->cbRunFusion->isChecked()) {
        jumplist.tasks()->addLink(style()->standardIcon(QStyle::SP_DesktopIcon), ui->cbRunFusion->text(), QDir::toNativeSeparators(QCoreApplication::applicationFilePath()), (QStringList() << "-style" << "fusion"));
    }
    if (ui->cbRunText->isChecked()) {
        jumplist.tasks()->addSeparator();
        jumplist.tasks()->addLink(ui->cbRunText->text(), QDir::toNativeSeparators(QCoreApplication::applicationFilePath()), QStringList("-text"));
    }
    jumplist.tasks()->setVisible(!jumplist.tasks()->isEmpty());
}

void TestWidget::openFile()
{
    QString filePath = QFileDialog::getOpenFileName(this, "Open a text file", QString(), "Text files (*.txt)");
    if (filePath.isEmpty())
        return;
    else
        showFile(filePath);
}
