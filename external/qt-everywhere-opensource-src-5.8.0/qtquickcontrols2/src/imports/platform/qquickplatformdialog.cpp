/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Labs Platform module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qquickplatformdialog_p.h"

#include <QtCore/qloggingcategory.h>
#include <QtGui/private/qguiapplication_p.h>
#include <QtQuick/qquickitem.h>
#include <QtQuick/qquickwindow.h>

#include "widgets/qwidgetplatform_p.h"

QT_BEGIN_NAMESPACE

/*!
    \qmltype Dialog
    \inherits QtObject
    \instantiates QQuickPlatformDialog
    \inqmlmodule Qt.labs.platform
    \since 5.8
    \brief The base class of native dialogs.

    The Dialog type provides common QML API for native platform dialogs.

    To show a native dialog, construct an instance of one of the concrete
    Dialog implementations, set the desired properties, and call \l open().
    Dialog emits \l accepted() or \l rejected() when the user is done with
    the dialog.

    \labs
*/

/*!
    \qmlsignal void Qt.labs.platform::Dialog::accepted()

    This signal is emitted when the dialog has been accepted either
    interactively or by calling \l accept().

    \note This signal is \e not emitted when closing the dialog with \l close().

    \sa rejected()
*/

/*!
    \qmlsignal void Qt.labs.platform::Dialog::rejected()

    This signal is emitted when the dialog has been rejected either
    interactively or by calling \l reject().

    \note This signal is \e not emitted when closing the dialog with \l close().

    \sa accepted()
*/

Q_DECLARE_LOGGING_CATEGORY(qtLabsPlatformDialogs)

QQuickPlatformDialog::QQuickPlatformDialog(QPlatformTheme::DialogType type, QObject *parent)
    : QObject(parent),
      m_visible(false),
      m_complete(false),
      m_result(0),
      m_parentWindow(nullptr),
      m_flags(Qt::Dialog),
      m_modality(Qt::WindowModal),
      m_type(type),
      m_handle(nullptr)
{
}

QQuickPlatformDialog::~QQuickPlatformDialog()
{
    destroy();
}

QPlatformDialogHelper *QQuickPlatformDialog::handle() const
{
    return m_handle;
}

/*!
    \default
    \qmlproperty list<Object> Qt.labs.platform::Dialog::data

    This default property holds the list of all objects declared as children of
    the dialog.
*/
QQmlListProperty<QObject> QQuickPlatformDialog::data()
{
    return QQmlListProperty<QObject>(this, m_data);
}

/*!
    \qmlproperty Window Qt.labs.platform::Dialog::parentWindow

    This property holds the parent window of the dialog.

    Unless explicitly set, the window is automatically resolved by iterating
    the QML parent objects until a \l Window or an \l Item that has a window
    is found.
*/
QWindow *QQuickPlatformDialog::parentWindow() const
{
    return m_parentWindow;
}

void QQuickPlatformDialog::setParentWindow(QWindow *window)
{
    if (m_parentWindow == window)
        return;

    m_parentWindow = window;
    emit parentWindowChanged();
}

/*!
    \qmlproperty string Qt.labs.platform::Dialog::title

    This property holds the title of the dialog.
*/
QString QQuickPlatformDialog::title() const
{
    return m_title;
}

void QQuickPlatformDialog::setTitle(const QString &title)
{
    if (m_title == title)
        return;

    m_title = title;
    emit titleChanged();
}

/*!
    \qmlproperty Qt::WindowFlags Qt.labs.platform::Dialog::flags

    This property holds the window flags of the dialog. The default value is \c Qt.Dialog.
*/
Qt::WindowFlags QQuickPlatformDialog::flags() const
{
    return m_flags;
}

void QQuickPlatformDialog::setFlags(Qt::WindowFlags flags)
{
    if (m_flags == flags)
        return;

    m_flags = flags;
    emit flagsChanged();
}

/*!
    \qmlproperty Qt::WindowModality Qt.labs.platform::Dialog::modality

    This property holds the modality of the dialog. The default value is \c Qt.WindowModal.

    Available values:
    \value Qt.NonModal The dialog is not modal and does not block input to other windows.
    \value Qt.WindowModal The dialog is modal to a single window hierarchy and blocks input to its parent window, all grandparent windows, and all siblings of its parent and grandparent windows.
    \value Qt.ApplicationModal The dialog is modal to the application and blocks input to all windows.
*/
Qt::WindowModality QQuickPlatformDialog::modality() const
{
    return m_modality;
}

void QQuickPlatformDialog::setModality(Qt::WindowModality modality)
{
    if (m_modality == modality)
        return;

    m_modality = modality;
    emit modalityChanged();
}

/*!
    \qmlproperty bool Qt.labs.platform::Dialog::visible

    This property holds the visibility of the dialog. The default value is \c false.

    \sa open(), close()
*/
bool QQuickPlatformDialog::isVisible() const
{
    return m_handle && m_visible;
}

void QQuickPlatformDialog::setVisible(bool visible)
{
    if (visible)
        open();
    else
        close();
}

/*!
    \qmlproperty int Qt.labs.platform::Dialog::result

    This property holds the result code.

    Standard result codes:
    \value Dialog.Accepted
    \value Dialog.Rejected

    \note MessageDialog sets the result to the value of the clicked standard
          button instead of using the standard result codes.
*/
int QQuickPlatformDialog::result() const
{
    return m_result;
}

void QQuickPlatformDialog::setResult(int result)
{
    if (m_result == result)
        return;

    m_result = result;
    emit resultChanged();
}

/*!
    \qmlmethod void Qt.labs.platform::Dialog::open()

    Opens the dialog.

    \sa visible, close()
*/
void QQuickPlatformDialog::open()
{
    if (m_visible || !create())
        return;

    onShow(m_handle);
    m_visible = m_handle->show(m_flags, m_modality, m_parentWindow);
    if (m_visible)
        emit visibleChanged();
}

/*!
    \qmlmethod void Qt.labs.platform::Dialog::close()

    Closes the dialog.

    \sa visible, open()
*/
void QQuickPlatformDialog::close()
{
    if (!m_handle || !m_visible)
        return;

    onHide(m_handle);
    m_handle->hide();
    m_visible = false;
    emit visibleChanged();
}

/*!
    \qmlmethod void Qt.labs.platform::Dialog::accept()

    Closes the dialog and emits the \l accepted() signal.

    \sa reject()
*/
void QQuickPlatformDialog::accept()
{
    done(Accepted);
}

/*!
    \qmlmethod void Qt.labs.platform::Dialog::reject()

    Closes the dialog and emits the \l rejected() signal.

    \sa accept()
*/
void QQuickPlatformDialog::reject()
{
    done(Rejected);
}

/*!
    \qmlmethod void Qt.labs.platform::Dialog::done(int result)

    Closes the dialog and sets the \a result.

    \sa accept(), reject(), result
*/
void QQuickPlatformDialog::done(int result)
{
    close();
    setResult(result);

    if (result == Accepted)
        emit accepted();
    else if (result == Rejected)
        emit rejected();
}

void QQuickPlatformDialog::classBegin()
{
}

void QQuickPlatformDialog::componentComplete()
{
    m_complete = true;
    if (!m_parentWindow)
        setParentWindow(findParentWindow());
}

static const char *qmlTypeName(const QObject *object)
{
    return object->metaObject()->className() + qstrlen("QQuickPlatform");
}

bool QQuickPlatformDialog::create()
{
    if (!m_handle) {
        if (useNativeDialog())
            m_handle = QGuiApplicationPrivate::platformTheme()->createPlatformDialogHelper(m_type);
        if (!m_handle)
            m_handle = QWidgetPlatform::createDialog(m_type, this);
        qCDebug(qtLabsPlatformDialogs) << qmlTypeName(this) << "->" << m_handle;
        if (m_handle) {
            onCreate(m_handle);
            connect(m_handle, &QPlatformDialogHelper::accept, this, &QQuickPlatformDialog::accept);
            connect(m_handle, &QPlatformDialogHelper::reject, this, &QQuickPlatformDialog::reject);
        }
    }
    return m_handle;
}

void QQuickPlatformDialog::destroy()
{
    delete m_handle;
    m_handle = nullptr;
}

bool QQuickPlatformDialog::useNativeDialog() const
{
    return !QCoreApplication::testAttribute(Qt::AA_DontUseNativeDialogs)
            && QGuiApplicationPrivate::platformTheme()->usePlatformNativeDialog(m_type);
}

void QQuickPlatformDialog::onCreate(QPlatformDialogHelper *dialog)
{
    Q_UNUSED(dialog);
}

void QQuickPlatformDialog::onShow(QPlatformDialogHelper *dialog)
{
    Q_UNUSED(dialog);
}

void QQuickPlatformDialog::onHide(QPlatformDialogHelper *dialog)
{
    Q_UNUSED(dialog);
}

QWindow *QQuickPlatformDialog::findParentWindow() const
{
    QObject *obj = parent();
    while (obj) {
        QWindow *window = qobject_cast<QWindow *>(obj);
        if (window)
            return window;
        QQuickItem *item = qobject_cast<QQuickItem *>(obj);
        if (item && item->window())
            return item->window();
        obj = obj->parent();
    }
    return nullptr;
}

QT_END_NAMESPACE
