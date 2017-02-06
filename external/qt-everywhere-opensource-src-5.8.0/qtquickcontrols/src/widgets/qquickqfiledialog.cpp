/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Quick Dialogs module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qquickqfiledialog_p.h"
#include "qquickitem.h"

#include <private/qguiapplication_p.h>
#include <private/qqmlcontext_p.h>
#include <QWindow>
#include <QQuickWindow>
#include <QFileDialog>

QT_BEGIN_NAMESPACE

/*!
    \qmltype QtFileDialog
    \instantiates QQuickQFileDialog
    \inqmlmodule QtQuick.PrivateWidgets
    \ingroup qtquick-visual
    \brief Dialog component for choosing files from a local filesystem.
    \since 5.1
    \internal

    QtFileDialog provides a means to instantiate and manage a QFileDialog.
    It is not recommended to be used directly; it is an implementation
    detail of \l FileDialog in the \l QtQuick.Dialogs module.

    To use this type, you will need to import the module with the following line:
    \code
    import QtQuick.PrivateWidgets 1.0
    \endcode
*/

/*!
    \qmlsignal QtQuick::Dialogs::FileDialog::accepted

    The \a accepted signal is emitted when the user has finished using the
    dialog. You can then inspect the \a fileUrl or \a fileUrls properties to
    get the selection.

    Example:

    \qml
    FileDialog {
        onAccepted: { console.log("Selected file: " + fileUrl) }
    }
    \endqml

    The corresponding handler is \c onAccepted.
*/

/*!
    \qmlsignal QtQuick::Dialogs::FileDialog::rejected

    The \a rejected signal is emitted when the user has dismissed the dialog,
    either by closing the dialog window or by pressing the Cancel button.

    The corresponding handler is \c onRejected.
*/

/*!
    \class QQuickQFileDialog
    \inmodule QtQuick.PrivateWidgets
    \internal

    \brief The QQuickQFileDialog class is a wrapper for a QFileDialog.

    \since 5.1
*/

/*!
    Constructs a file dialog with parent window \a parent.
*/
QQuickQFileDialog::QQuickQFileDialog(QObject *parent)
    : QQuickAbstractFileDialog(parent)
{
}

/*!
    Destroys the file dialog.
*/
QQuickQFileDialog::~QQuickQFileDialog()
{
    if (m_dlgHelper)
        m_dlgHelper->hide();
    delete m_dlgHelper;
}

QPlatformFileDialogHelper *QQuickQFileDialog::helper()
{
    QQuickItem *parentItem = qobject_cast<QQuickItem *>(parent());
    if (parentItem)
        m_parentWindow = parentItem->window();

    if (!m_dlgHelper) {
        m_dlgHelper = new QFileDialogHelper();
        connect(m_dlgHelper, SIGNAL(directoryEntered(QUrl)), this, SIGNAL(folderChanged()));
        connect(m_dlgHelper, SIGNAL(filterSelected(QString)), this, SIGNAL(filterSelected()));
        connect(m_dlgHelper, SIGNAL(accept()), this, SLOT(accept()));
        connect(m_dlgHelper, SIGNAL(reject()), this, SLOT(reject()));
    }

    return m_dlgHelper;
}

QList<QUrl> QQuickQFileDialog::fileUrls() const
{
    if (m_dialogHelperInUse)
        return m_dlgHelper->selectedFiles();
    return QList<QUrl>();
}

QFileDialogHelper::QFileDialogHelper() :
    QPlatformFileDialogHelper()
{
    connect(&m_dialog, SIGNAL(currentChanged(QString)), this, SLOT(currentChanged(QString)));
    connect(&m_dialog, SIGNAL(directoryEntered(QString)), this, SLOT(directoryEntered(QString)));
    connect(&m_dialog, SIGNAL(fileSelected(QString)), this, SLOT(fileSelected(QString)));
    connect(&m_dialog, SIGNAL(filesSelected(QStringList)), this, SLOT(filesSelected(QStringList)));
    connect(&m_dialog, SIGNAL(filterSelected(QString)), this, SIGNAL(filterSelected(QString)));
    connect(&m_dialog, SIGNAL(accepted()), this, SIGNAL(accept()));
    connect(&m_dialog, SIGNAL(rejected()), this, SIGNAL(reject()));
}

QList<QUrl> QFileDialogHelper::selectedFiles() const
{
    return m_dialog.selectedUrls();
}

void QFileDialogHelper::setFilter() {
    m_dialog.setWindowTitle(QPlatformFileDialogHelper::options()->windowTitle());
    if (QPlatformFileDialogHelper::options()->isLabelExplicitlySet(QFileDialogOptions::LookIn))
        m_dialog.setLabelText(m_dialog.LookIn, QPlatformFileDialogHelper::options()->labelText(QFileDialogOptions::LookIn));
    if (QPlatformFileDialogHelper::options()->isLabelExplicitlySet(QFileDialogOptions::FileName))
        m_dialog.setLabelText(m_dialog.FileName, QPlatformFileDialogHelper::options()->labelText(QFileDialogOptions::FileName));
    if (QPlatformFileDialogHelper::options()->isLabelExplicitlySet(QFileDialogOptions::FileType))
        m_dialog.setLabelText(m_dialog.FileType, QPlatformFileDialogHelper::options()->labelText(QFileDialogOptions::FileType));
    if (QPlatformFileDialogHelper::options()->isLabelExplicitlySet(QFileDialogOptions::Accept))
        m_dialog.setLabelText(m_dialog.Accept, QPlatformFileDialogHelper::options()->labelText(QFileDialogOptions::Accept));
    if (QPlatformFileDialogHelper::options()->isLabelExplicitlySet(QFileDialogOptions::Reject))
        m_dialog.setLabelText(m_dialog.Reject, QPlatformFileDialogHelper::options()->labelText(QFileDialogOptions::Reject));
    m_dialog.setFilter(QPlatformFileDialogHelper::options()->filter());
    m_dialog.setNameFilters(QPlatformFileDialogHelper::options()->nameFilters());
    m_dialog.selectNameFilter(QPlatformFileDialogHelper::options()->initiallySelectedNameFilter());
    m_dialog.setFileMode(QFileDialog::FileMode(QPlatformFileDialogHelper::options()->fileMode()));
    m_dialog.setOptions((QFileDialog::Options)((int)(QPlatformFileDialogHelper::options()->options())));
    m_dialog.setAcceptMode(QFileDialog::AcceptMode(QPlatformFileDialogHelper::options()->acceptMode()));
}

bool QFileDialogHelper::show(Qt::WindowFlags f, Qt::WindowModality m, QWindow *parent) {
    m_dialog.winId();
    QWindow *window = m_dialog.windowHandle();
    Q_ASSERT(window);
    window->setTransientParent(parent);
    window->setFlags(f);
    m_dialog.setWindowModality(m);
    m_dialog.show();
    return m_dialog.isVisible();
}

void QFileDialogHelper::currentChanged(const QString& path)
{
    emit QPlatformFileDialogHelper::currentChanged(QUrl::fromLocalFile(path));
}

void QFileDialogHelper::directoryEntered(const QString& path)
{
    emit QPlatformFileDialogHelper::directoryEntered(QUrl::fromLocalFile(path));
}

void QFileDialogHelper::fileSelected(const QString& path)
{
    emit QPlatformFileDialogHelper::fileSelected(QUrl::fromLocalFile(path));
}

void QFileDialogHelper::filesSelected(const QStringList& paths)
{
    QList<QUrl> pathUrls;
    pathUrls.reserve(paths.count());
    for (const QString &path : paths)
        pathUrls << QUrl::fromLocalFile(path);
    emit QPlatformFileDialogHelper::filesSelected(pathUrls);
}

QT_END_NAMESPACE
