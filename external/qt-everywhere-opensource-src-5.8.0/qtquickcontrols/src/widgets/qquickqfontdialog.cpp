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

#include "qquickqfontdialog_p.h"
#include "qquickitem.h"

#include <private/qguiapplication_p.h>
#include <private/qqmlcontext_p.h>
#include <QWindow>
#include <QQuickWindow>
#include <QFontDialog>

QT_BEGIN_NAMESPACE

class QFontDialogHelper : public QPlatformFontDialogHelper
{
public:
    QFontDialogHelper() :
        QPlatformFontDialogHelper()
    {
        connect(&m_dialog, SIGNAL(currentFontChanged(QFont)), this, SIGNAL(currentFontChanged(QFont)));
        connect(&m_dialog, SIGNAL(fontSelected(QFont)), this, SIGNAL(fontSelected(QFont)));
        connect(&m_dialog, SIGNAL(accepted()), this, SIGNAL(accept()));
        connect(&m_dialog, SIGNAL(rejected()), this, SIGNAL(reject()));
    }

    virtual void setCurrentFont(const QFont &font) { m_dialog.setCurrentFont(font); }
    virtual QFont currentFont() const { return m_dialog.currentFont(); }

    virtual void exec() { m_dialog.exec(); }

    virtual bool show(Qt::WindowFlags f, Qt::WindowModality m, QWindow *parent) {
        m_dialog.winId();
        QWindow *window = m_dialog.windowHandle();
        Q_ASSERT(window);
        window->setTransientParent(parent);
        window->setFlags(f);
        m_dialog.windowHandle()->setTransientParent(parent);
        m_dialog.windowHandle()->setFlags(f);
        m_dialog.setWindowModality(m);
        m_dialog.setWindowTitle(QPlatformFontDialogHelper::options()->windowTitle());
        m_dialog.setOptions((QFontDialog::FontDialogOptions)((int)(QPlatformFontDialogHelper::options()->options())));
        m_dialog.show();
        return m_dialog.isVisible();
    }

    virtual void hide() { m_dialog.hide(); }

private:
    QFontDialog m_dialog;
};

/*!
    \qmltype QtFontDialog
    \instantiates QQuickQFontDialog
    \inqmlmodule QtQuick.PrivateWidgets
    \ingroup qtquick-visual
    \brief Dialog component for choosing files from a local filesystem.
    \since 5.2
    \internal

    QtFontDialog provides a means to instantiate and manage a QFontDialog.
    It is not recommended to be used directly; it is an implementation
    detail of \l FontDialog in the \l QtQuick.Dialogs module.

    To use this type, you will need to import the module with the following line:
    \code
    import QtQuick.PrivateWidgets 1.1
    \endcode
*/

/*!
    \qmlsignal QtQuick::Dialogs::FontDialog::accepted

    The \a accepted signal is emitted when the user has finished using the
    dialog. You can then inspect the \a filePath or \a filePaths properties to
    get the selection.

    Example:

    \qml
    FontDialog {
        onAccepted: { console.log("Selected file: " + filePath) }
    }
    \endqml

    The corresponding handler is \c onAccepted.
*/

/*!
    \qmlsignal QtQuick::Dialogs::FontDialog::rejected

    The \a rejected signal is emitted when the user has dismissed the dialog,
    either by closing the dialog window or by pressing the Cancel button.

    The corresponding handler is \c onRejected.
*/

/*!
    \class QQuickQFontDialog
    \inmodule QtQuick.PrivateWidgets
    \internal

    \brief The QQuickQFontDialog class is a wrapper for a QFontDialog.

    \since 5.2
*/

/*!
    Constructs a file dialog with parent window \a parent.
*/
QQuickQFontDialog::QQuickQFontDialog(QObject *parent)
    : QQuickAbstractFontDialog(parent)
{
}

/*!
    Destroys the file dialog.
*/
QQuickQFontDialog::~QQuickQFontDialog()
{
    if (m_dlgHelper)
        m_dlgHelper->hide();
    delete m_dlgHelper;
}

QPlatformFontDialogHelper *QQuickQFontDialog::helper()
{
    QQuickItem *parentItem = qobject_cast<QQuickItem *>(parent());
    if (parentItem)
        m_parentWindow = parentItem->window();

    if (!m_dlgHelper) {
        m_dlgHelper = new QFontDialogHelper();
        connect(m_dlgHelper, SIGNAL(currentFontChanged(QFont)), this, SLOT(setFont(QFont)));
        connect(m_dlgHelper, SIGNAL(fontSelected(QFont)), this, SLOT(setFont(QFont)));
        connect(m_dlgHelper, SIGNAL(accept()), this, SLOT(accept()));
        connect(m_dlgHelper, SIGNAL(reject()), this, SLOT(reject()));
    }

    return m_dlgHelper;
}

QT_END_NAMESPACE
