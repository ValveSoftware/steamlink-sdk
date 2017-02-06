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

#include "newform.h"
#include "qdesigner_workbench.h"
#include "qdesigner_actions.h"
#include "qdesigner_formwindow.h"
#include "qdesigner_settings.h"

#include <newformwidget_p.h>

#include <QtDesigner/QDesignerFormEditorInterface>

#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QDebug>
#include <QtCore/QDir>
#include <QtCore/QTemporaryFile>

#include <QtWidgets/QApplication>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QMenu>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QFrame>
#include <QtWidgets/QMessageBox>

QT_BEGIN_NAMESPACE

NewForm::NewForm(QDesignerWorkbench *workbench, QWidget *parentWidget, const QString &fileName)
    : QDialog(parentWidget, Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint),
      m_fileName(fileName),
      m_newFormWidget(QDesignerNewFormWidgetInterface::createNewFormWidget(workbench->core())),
      m_workbench(workbench),
      m_chkShowOnStartup(new QCheckBox(tr("Show this Dialog on Startup"))),
      m_createButton(new QPushButton(QApplication::translate("NewForm", "C&reate", 0))),
      m_recentButton(new QPushButton(QApplication::translate("NewForm", "Recent", 0))),
      m_buttonBox(0)
{
    setWindowTitle(tr("New Form"));
    QDesignerSettings settings(m_workbench->core());

    QVBoxLayout *vBoxLayout = new QVBoxLayout;

    connect(m_newFormWidget, &QDesignerNewFormWidgetInterface::templateActivated,
            this, &NewForm::slotTemplateActivated);
    connect(m_newFormWidget, &QDesignerNewFormWidgetInterface::currentTemplateChanged,
            this, &NewForm::slotCurrentTemplateChanged);
    vBoxLayout->addWidget(m_newFormWidget);

    QFrame *horizontalLine = new QFrame;
    horizontalLine->setFrameShape(QFrame::HLine);
    horizontalLine->setFrameShadow(QFrame::Sunken);
    vBoxLayout->addWidget(horizontalLine);

    m_chkShowOnStartup->setChecked(settings.showNewFormOnStartup());
    vBoxLayout->addWidget(m_chkShowOnStartup);

    m_buttonBox = createButtonBox();
    vBoxLayout->addWidget(m_buttonBox);
    setLayout(vBoxLayout);

    resize(500, 400);
    slotCurrentTemplateChanged(m_newFormWidget->hasCurrentTemplate());
}

QDialogButtonBox *NewForm::createButtonBox()
{
    // Dialog buttons with 'recent files'
    QDialogButtonBox *buttonBox = new QDialogButtonBox;
    buttonBox->addButton(QApplication::translate("NewForm", "&Close", 0),
                         QDialogButtonBox::RejectRole);
    buttonBox->addButton(m_createButton, QDialogButtonBox::AcceptRole);
    buttonBox->addButton(QApplication::translate("NewForm", "&Open...", 0),
                         QDialogButtonBox::ActionRole);
    buttonBox->addButton(m_recentButton, QDialogButtonBox::ActionRole);
    QDesignerActions *da = m_workbench->actionManager();
    QMenu *recentFilesMenu = new QMenu(tr("&Recent Forms"), m_recentButton);
    // Pop the "Recent Files" stuff in here.
    const QList<QAction *> recentActions = da->recentFilesActions()->actions();
    if (!recentActions.empty()) {
        const QList<QAction *>::const_iterator acend = recentActions.constEnd();
        for (QList<QAction *>::const_iterator it = recentActions.constBegin(); it != acend; ++it) {
            recentFilesMenu->addAction(*it);
            connect(*it, &QAction::triggered, this, &NewForm::recentFileChosen);
        }
    }
    m_recentButton->setMenu(recentFilesMenu);
    connect(buttonBox, &QDialogButtonBox::clicked, this, &NewForm::slotButtonBoxClicked);
    return buttonBox;
}

NewForm::~NewForm()
{
    QDesignerSettings settings (m_workbench->core());
    settings.setShowNewFormOnStartup(m_chkShowOnStartup->isChecked());
}

void NewForm::recentFileChosen()
{
    QAction *action = qobject_cast<QAction *>(sender());
    if (!action)
        return;
    if (action->objectName() == QStringLiteral("__qt_action_clear_menu_"))
        return;
    close();
}

void NewForm::slotCurrentTemplateChanged(bool templateSelected)
{
    if (templateSelected) {
        m_createButton->setEnabled(true);
        m_createButton->setDefault(true);
    } else {
        m_createButton->setEnabled(false);
    }
}

void NewForm::slotTemplateActivated()
{
    m_createButton->animateClick(0);
}

void NewForm::slotButtonBoxClicked(QAbstractButton *btn)
{
    switch (m_buttonBox->buttonRole(btn)) {
    case QDialogButtonBox::RejectRole:
        reject();
        break;
    case QDialogButtonBox::ActionRole:
        if (btn != m_recentButton) {
            m_fileName.clear();
            if (m_workbench->actionManager()->openForm(this))
                accept();
        }
        break;
    case QDialogButtonBox::AcceptRole: {
        QString errorMessage;
        if (openTemplate(&errorMessage)) {
            accept();
        }  else {
            QMessageBox::warning(this, tr("Read error"), errorMessage);
        }
    }
        break;
    default:
        break;
    }
}

bool NewForm::openTemplate(QString *ptrToErrorMessage)
{
    const QString contents = m_newFormWidget->currentTemplate(ptrToErrorMessage);
    if (contents.isEmpty())
        return false;
    // Write to temporary file and open
    QString tempPattern = QDir::tempPath();
    if (!tempPattern.endsWith(QDir::separator())) // platform-dependant
        tempPattern += QDir::separator();
    tempPattern += QStringLiteral("XXXXXX.ui");
    QTemporaryFile tempFormFile(tempPattern);

    tempFormFile.setAutoRemove(true);
    if (!tempFormFile.open()) {
        *ptrToErrorMessage = tr("A temporary form file could not be created in %1.").arg(QDir::tempPath());
        return false;
    }
    const QString tempFormFileName = tempFormFile.fileName();
    tempFormFile.write(contents.toUtf8());
    if (!tempFormFile.flush())  {
        *ptrToErrorMessage = tr("The temporary form file %1 could not be written.").arg(tempFormFileName);
        return false;
    }
    tempFormFile.close();
    return m_workbench->openTemplate(tempFormFileName, m_fileName, ptrToErrorMessage);
}

QImage NewForm::grabForm(QDesignerFormEditorInterface *core,
                         QIODevice &file,
                         const QString &workingDir,
                         const qdesigner_internal::DeviceProfile &dp)
{
    return qdesigner_internal::NewFormWidget::grabForm(core, file, workingDir, dp);
}

QT_END_NAMESPACE
