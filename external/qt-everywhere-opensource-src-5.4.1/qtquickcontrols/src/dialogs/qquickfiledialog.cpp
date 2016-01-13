/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtQuick.Dialogs module of the Qt Toolkit.
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

#include "qquickfiledialog_p.h"
#include <QQuickItem>
#include <QQmlEngine>
#include <private/qguiapplication_p.h>
#include <private/qv4object_p.h>

QT_BEGIN_NAMESPACE

using namespace QV4;

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


void QQuickFileDialog::addShortcut(int &i, const QString &name, const QString &path)
{
    QJSEngine *engine = qmlEngine(this);
    QJSValue o = engine->newObject();
    o.setProperty("name", name);
    o.setProperty("url", QUrl::fromLocalFile(path).toString());
    m_shortcuts.setProperty(i++, o);
}

void QQuickFileDialog::addIfReadable(int &i, const QString &name, QStandardPaths::StandardLocation loc)
{
    QStringList paths = QStandardPaths::standardLocations(loc);
    if (!paths.isEmpty() && QDir(paths.first()).isReadable())
        addShortcut(i, name, paths.first());
}

QJSValue QQuickFileDialog::shortcuts()
{
    if (m_shortcuts.isUndefined()) {
        QJSEngine *engine = qmlEngine(this);
        m_shortcuts = engine->newArray();
        int i = 0;

        addIfReadable(i, "Desktop", QStandardPaths::DesktopLocation);
        addIfReadable(i, "Documents", QStandardPaths::DocumentsLocation);
        addIfReadable(i, "Music", QStandardPaths::MusicLocation);
        addIfReadable(i, "Movies", QStandardPaths::MoviesLocation);
        addIfReadable(i, "Pictures", QStandardPaths::PicturesLocation);
        addIfReadable(i, "Home", QStandardPaths::HomeLocation);

        QFileInfoList drives = QDir::drives();
        foreach (QFileInfo fi, drives)
            addShortcut(i, fi.absoluteFilePath(), fi.absoluteFilePath());
    }
    return m_shortcuts;
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
