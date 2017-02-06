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

#include "qquickplatformcolordialog_p.h"

QT_BEGIN_NAMESPACE

/*!
    \qmltype ColorDialog
    \inherits Dialog
    \instantiates QQuickPlatformColorDialog
    \inqmlmodule Qt.labs.platform
    \since 5.8
    \brief A native color dialog.

    The ColorDialog type provides a QML API for native platform color dialogs.

    \image qtlabsplatform-colordialog-gtk.png

    To show a color dialog, construct an instance of ColorDialog, set the
    desired properties, and call \l {Dialog::}{open()}. The \l currentColor
    property can be used to determine the currently selected color in the
    dialog. The \l color property is updated only after the final selection
    has been made by accepting the dialog.

    \code
    MenuItem {
        text: "Color"
        onTriggered: colorDialog.open()
    }

    ColorDialog {
        id: colorDialog
        currentColor: document.color
    }

    MyDocument {
        id: document
        color: colorDialog.color
    }
    \endcode

    \section2 Availability

    A native platform color dialog is currently available on the following platforms:

    \list
    \li macOS
    \li Linux (when running with the GTK+ platform theme)
    \endlist

    \input includes/widgets.qdocinc 1

    \labs
*/

QQuickPlatformColorDialog::QQuickPlatformColorDialog(QObject *parent)
    : QQuickPlatformDialog(QPlatformTheme::ColorDialog, parent),
      m_options(QColorDialogOptions::create())
{
}

/*!
    \qmlproperty color Qt.labs.platform::ColorDialog::color

    This property holds the final accepted color.

    Unlike the \l currentColor property, the \c color property is not updated
    while the user is selecting colors in the dialog, but only after the final
    selection has been made. That is, when the user has clicked \uicontrol OK
    to accept a color. Alternatively, the \l {Dialog::}{accepted()} signal
    can be handled to get the final selection.

    \sa currentColor, {Dialog::}{accepted()}
*/
QColor QQuickPlatformColorDialog::color() const
{
    return m_color;
}

void QQuickPlatformColorDialog::setColor(const QColor &color)
{
    if (m_color == color)
        return;

    m_color = color;
    setCurrentColor(color);
    emit colorChanged();
}

/*!
    \qmlproperty color Qt.labs.platform::ColorDialog::currentColor

    This property holds the currently selected color in the dialog.

    Unlike the \l color property, the \c currentColor property is updated
    while the user is selecting colors in the dialog, even before the final
    selection has been made.

    \sa color
*/
QColor QQuickPlatformColorDialog::currentColor() const
{
    if (QPlatformColorDialogHelper *colorDialog = qobject_cast<QPlatformColorDialogHelper *>(handle()))
        return colorDialog->currentColor();
    return m_currentColor;
}

void QQuickPlatformColorDialog::setCurrentColor(const QColor &color)
{
    if (QPlatformColorDialogHelper *colorDialog = qobject_cast<QPlatformColorDialogHelper *>(handle()))
        colorDialog->setCurrentColor(color);
    m_currentColor = color;
}

/*!
    \qmlproperty flags Qt.labs.platform::ColorDialog::options

    This property holds the various options that affect the look and feel of the dialog.

    By default, all options are disabled.

    Options should be set before showing the dialog. Setting them while the dialog is
    visible is not guaranteed to have an immediate effect on the dialog (depending on
    the option and on the platform).

    Available options:
    \value ColorDialog.ShowAlphaChannel Allow the user to select the alpha component of a color.
    \value ColorDialog.NoButtons Don't display \uicontrol OK and \uicontrol Cancel buttons (useful for "live dialogs").
*/
QColorDialogOptions::ColorDialogOptions QQuickPlatformColorDialog::options() const
{
    return m_options->options();
}

void QQuickPlatformColorDialog::setOptions(QColorDialogOptions::ColorDialogOptions options)
{
    if (options == m_options->options())
        return;

    m_options->setOptions(options);
    emit optionsChanged();
}

bool QQuickPlatformColorDialog::useNativeDialog() const
{
    return QQuickPlatformDialog::useNativeDialog()
            && !m_options->testOption(QColorDialogOptions::DontUseNativeDialog);
}

void QQuickPlatformColorDialog::onCreate(QPlatformDialogHelper *dialog)
{
    if (QPlatformColorDialogHelper *colorDialog = qobject_cast<QPlatformColorDialogHelper *>(dialog)) {
        connect(colorDialog, &QPlatformColorDialogHelper::currentColorChanged, this, &QQuickPlatformColorDialog::currentColorChanged);
        colorDialog->setOptions(m_options);
    }
}

void QQuickPlatformColorDialog::onShow(QPlatformDialogHelper *dialog)
{
    m_options->setWindowTitle(title());
    if (QPlatformColorDialogHelper *colorDialog = qobject_cast<QPlatformColorDialogHelper *>(dialog))
        colorDialog->setOptions(m_options);
}

void QQuickPlatformColorDialog::accept()
{
    setColor(currentColor());
    QQuickPlatformDialog::accept();
}

QT_END_NAMESPACE
