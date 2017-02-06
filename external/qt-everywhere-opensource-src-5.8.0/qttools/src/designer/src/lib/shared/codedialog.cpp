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

#include "codedialog_p.h"
#include "qdesigner_utils_p.h"
#include "iconloader_p.h"

#include <texteditfindwidget.h>

#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>
#ifndef QT_NO_CLIPBOARD
#include <QtGui/QClipboard>
#endif
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QFileDialog>
#include <QtGui/QIcon>
#include <QtGui/QKeyEvent>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QToolBar>
#include <QtWidgets/QVBoxLayout>

#include <QtCore/QDebug>
#include <QtCore/QDir>
#include <QtCore/QTemporaryFile>

QT_BEGIN_NAMESPACE

namespace qdesigner_internal {
// ----------------- CodeDialogPrivate
struct CodeDialog::CodeDialogPrivate {
    CodeDialogPrivate();

    QTextEdit *m_textEdit;
    TextEditFindWidget *m_findWidget;
    QString m_formFileName;
};

CodeDialog::CodeDialogPrivate::CodeDialogPrivate()
    : m_textEdit(new QTextEdit)
    , m_findWidget(new TextEditFindWidget)
{
}

// ----------------- CodeDialog
CodeDialog::CodeDialog(QWidget *parent) :
    QDialog(parent),
    m_impl(new CodeDialogPrivate)
{
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    QVBoxLayout *vBoxLayout = new QVBoxLayout;

    // Edit tool bar
    QToolBar *toolBar = new QToolBar;

    const QIcon saveIcon = createIconSet(QStringLiteral("filesave.png"));
    QAction *saveAction = toolBar->addAction(saveIcon, tr("Save..."));
    connect(saveAction, &QAction::triggered, this, &CodeDialog::slotSaveAs);

#ifndef QT_NO_CLIPBOARD
    const QIcon copyIcon = createIconSet(QStringLiteral("editcopy.png"));
    QAction *copyAction = toolBar->addAction(copyIcon, tr("Copy All"));
    connect(copyAction, &QAction::triggered, this, &CodeDialog::copyAll);
#endif

    QAction *findAction = toolBar->addAction(
            TextEditFindWidget::findIconSet(),
            tr("&Find in Text..."),
            m_impl->m_findWidget, &AbstractFindWidget::activate);
    findAction->setShortcut(QKeySequence::Find);

    vBoxLayout->addWidget(toolBar);

    // Edit
    m_impl->m_textEdit->setReadOnly(true);
    m_impl->m_textEdit->setMinimumSize(QSize(
                m_impl->m_findWidget->minimumSize().width(),
                500));
    vBoxLayout->addWidget(m_impl->m_textEdit);

    // Find
    m_impl->m_findWidget->setTextEdit(m_impl->m_textEdit);
    vBoxLayout->addWidget(m_impl->m_findWidget);

    // Button box
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // Disable auto default
    QPushButton *closeButton = buttonBox->button(QDialogButtonBox::Close);
    closeButton->setAutoDefault(false);
    vBoxLayout->addWidget(buttonBox);

    setLayout(vBoxLayout);
}

CodeDialog::~CodeDialog()
{
    delete m_impl;
}

void CodeDialog::setCode(const QString &code)
{
    m_impl->m_textEdit->setPlainText(code);
}

QString CodeDialog::code() const
{
   return m_impl->m_textEdit->toPlainText();
}

void CodeDialog::setFormFileName(const QString &f)
{
    m_impl->m_formFileName = f;
}

QString CodeDialog::formFileName() const
{
    return m_impl->m_formFileName;
}

bool CodeDialog::generateCode(const QDesignerFormWindowInterface *fw,
                              QString *code,
                              QString *errorMessage)
{
    // Generate temporary file name similar to form file name
    // (for header guards)
    QString tempPattern = QDir::tempPath();
    if (!tempPattern.endsWith(QDir::separator())) // platform-dependant
        tempPattern += QDir::separator();
    const QString fileName = fw->fileName();
    if (fileName.isEmpty()) {
        tempPattern += QStringLiteral("designer");
    } else {
        tempPattern += QFileInfo(fileName).baseName();
    }
    tempPattern += QStringLiteral("XXXXXX.ui");
    // Write to temp file
    QTemporaryFile tempFormFile(tempPattern);

    tempFormFile.setAutoRemove(true);
    if (!tempFormFile.open()) {
        *errorMessage = tr("A temporary form file could not be created in %1.").arg(QDir::tempPath());
        return false;
    }
    const QString tempFormFileName = tempFormFile.fileName();
    tempFormFile.write(fw->contents().toUtf8());
    if (!tempFormFile.flush())  {
        *errorMessage = tr("The temporary form file %1 could not be written.").arg(tempFormFileName);
        return false;
    }
    tempFormFile.close();
    // Run uic
    QByteArray rc;
    if (!runUIC(tempFormFileName, rc, *errorMessage))
        return false;
    *code = QString::fromUtf8(rc);
    return true;
}

bool CodeDialog::showCodeDialog(const QDesignerFormWindowInterface *fw,
                                QWidget *parent,
                                QString *errorMessage)
{
    QString code;
    if (!generateCode(fw, &code, errorMessage))
        return false;

    CodeDialog dialog(parent);
    dialog.setWindowTitle(tr("%1 - [Code]").arg(fw->mainContainer()->windowTitle()));
    dialog.setCode(code);
    dialog.setFormFileName(fw->fileName());
    dialog.exec();
    return true;
}

void CodeDialog::slotSaveAs()
{
    // build the default relative name 'ui_sth.h'
    const QString headerSuffix = QString(QLatin1Char('h'));
    QString filter;
    const QString uiFile = formFileName();

    if (!uiFile.isEmpty()) {
        filter = QStringLiteral("ui_");
        filter += QFileInfo(uiFile).baseName();
        filter += QLatin1Char('.');
        filter += headerSuffix;
    }
    // file dialog
    while (true) {
        const QString fileName =
            QFileDialog::getSaveFileName (this, tr("Save Code"), filter, tr("Header Files (*.%1)").arg(headerSuffix));
        if (fileName.isEmpty())
            break;

         QFile file(fileName);
         if (!file.open(QIODevice::WriteOnly|QIODevice::Text)) {
             warning(tr("The file %1 could not be opened: %2").arg(fileName).arg(file.errorString()));
             continue;
         }
         file.write(code().toUtf8());
         if (!file.flush()) {
             warning(tr("The file %1 could not be written: %2").arg(fileName).arg(file.errorString()));
             continue;
         }
         file.close();
         break;
    }
}

void CodeDialog::warning(const QString &msg)
{
     QMessageBox::warning(
             this, tr("%1 - Error").arg(windowTitle()),
             msg, QMessageBox::Close);
}

#ifndef QT_NO_CLIPBOARD
void CodeDialog::copyAll()
{
    QApplication::clipboard()->setText(code());
}
#endif

} // namespace qdesigner_internal

QT_END_NAMESPACE
