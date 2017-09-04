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

#include <QtDesigner/abstractwidgetfactory.h>
#include "abstractformeditor.h"
#include "abstractwidgetdatabase.h"

QT_BEGIN_NAMESPACE

/*!
    \class QDesignerWidgetFactoryInterface
    \brief The QDesignerWidgetFactoryInterface class provides an interface that is used to control
    the widget factory used by Qt Designer.
    \inmodule QtDesigner
    \internal
*/

/*!
    \fn QDesignerWidgetFactoryInterface::QDesignerWidgetFactoryInterface(QObject *parent)

    Constructs an interface to a widget factory with the given \a parent.
*/
QDesignerWidgetFactoryInterface::QDesignerWidgetFactoryInterface(QObject *parent)
    : QObject(parent)
{
}

/*!
    \fn virtual QDesignerWidgetFactoryInterface::~QDesignerWidgetFactoryInterface()
*/
QDesignerWidgetFactoryInterface::~QDesignerWidgetFactoryInterface()
{
}

/*!
    \fn virtual QDesignerFormEditorInterface *QDesignerWidgetFactoryInterface::core() const = 0

    Returns the core form editor interface associated with this interface.
*/

/*!
    \fn virtual QWidget* QDesignerWidgetFactoryInterface::containerOfWidget(QWidget *child) const = 0

    Returns the widget that contains the specified \a child widget.
*/

/*!
    \fn virtual QWidget* QDesignerWidgetFactoryInterface::widgetOfContainer(QWidget *container) const = 0


*/

/*!
    \fn virtual QWidget *QDesignerWidgetFactoryInterface::createWidget(const QString &name, QWidget *parent) const = 0

    Returns a new widget with the given \a name and \a parent widget. If no parent is specified,
    the widget created will be a top-level widget.
*/

/*!
    \fn virtual QLayout *QDesignerWidgetFactoryInterface::createLayout(QWidget *widget, QLayout *layout, int type) const = 0

    Returns a new layout of the specified \a type for the given \a widget or \a layout.
*/

/*!
    \fn virtual bool QDesignerWidgetFactoryInterface::isPassiveInteractor(QWidget *widget) = 0
*/

/*!
    \fn virtual void QDesignerWidgetFactoryInterface::initialize(QObject *object) const = 0
*/

QT_END_NAMESPACE
