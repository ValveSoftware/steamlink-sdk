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

#include "qquickfiledialog_p.h"
#include <QQuickItem>
#include <QQmlEngine>
#include <QJSValueIterator>
#include <private/qguiapplication_p.h>

QT_BEGIN_NAMESPACE

using namespace QV4;

// Note: documentation comments here are not currently used to generate
// user documentation, because AbstractFileDialog is not a user-facing type.
// FileDialog docs go into qquickplatformfiledialog.cpp

/*!
    \qmltype AbstractFileDialog
    \instantiates QQuickFileDialog
    \inqmlmodule QtQuick.Dialogs
    \ingroup qtquick-visual
    \brief API wrapper for QML file dialog implementations
    \since 5.1
    \internal

    AbstractFileDialog provides only the API for implementing a file dialog.
    The implementation (e.g. a Window or preferably an Item, in case it is
    shown on a device that doesn't support multiple windows) can be provided as
    \l implementation, which is the default property (the only allowed child
    element).
*/

/*!
    \qmlsignal QtQuick::Dialogs::AbstractFileDialog::accepted

    This signal is emitted by \l accept().

    The corresponding handler is \c onAccepted.
*/

/*!
    \qmlsignal QtQuick::Dialogs::AbstractFileDialog::rejected

    This signal is emitted by \l reject().

    The corresponding handler is \c onRejected.
*/

/*!
    \class QQuickFileDialog
    \inmodule QtQuick.Dialogs
    \internal

    The QQuickFileDialog class is a concrete subclass of
    \l QQuickAbstractFileDialog, but it is abstract from the QML perspective
    because it needs to enclose a graphical implementation. It exists in order
    to provide accessors and helper functions which the QML implementation will
    need.

    \since 5.1
*/

/*!
    Constructs a file dialog wrapper with parent window \a parent.
*/
QQuickFileDialog::QQuickFileDialog(QObject *parent)
    : QQuickAbstractFileDialog(parent)
{
}


/*!
    Destroys the file dialog wrapper.
*/
QQuickFileDialog::~QQuickFileDialog()
{
}

QList<QUrl> QQuickFileDialog::fileUrls() const
{
    return m_selections;
}

/*!
    \qmlproperty bool AbstractFileDialog::visible

    This property holds whether the dialog is visible. By default this is false.
*/

/*!
    \qmlproperty bool AbstractFileDialog::fileUrls

    A list of files to be populated as the user chooses.
*/

/*!
   \brief Clears \l fileUrls
*/
void QQuickFileDialog::clearSelection()
{
    m_selections.clear();
}

/*!
   \brief Adds one file to \l fileUrls

   \l path should be given as an absolute file:// path URL.
   Returns true on success, false if the given path is
   not valid given the current property settings.
*/
bool QQuickFileDialog::addSelection(const QUrl &path)
{
    QFileInfo info(path.toLocalFile());
    if (selectExisting() && !info.exists())
        return false;
    if (selectFolder() != info.isDir())
        return false;
    if (selectFolder())
        m_selections.append(pathFolder(path.toLocalFile()));
    else
        m_selections.append(path);
    return true;
}

/*!
    \brief get a file's directory as a URL

    If \a path points to a directory, just convert it to a URL.
    If \a path points to a file, convert the file's directory to a URL.
*/
QUrl QQuickFileDialog::pathFolder(const QString &path)
{
    QFileInfo info(path);
    if (info.exists() && info.isDir())
        return QUrl::fromLocalFile(path);
    return QUrl::fromLocalFile(QFileInfo(path).absolutePath());
}

/*!
    \qmlproperty QObject AbstractFileDialog::implementation

    The QML object which implements the actual file dialog. Should be either a
    \l Window or an \l Item.
*/

QT_END_NAMESPACE
