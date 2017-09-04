/****************************************************************************
**
** Copyright (C) 2016 Research In Motion.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQml module of the Qt Toolkit.
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

#include "qqmlmodelsmodule_p.h"
#include <QtCore/qitemselectionmodel.h>
#include <private/qqmllistmodel_p.h>
#include <private/qqmldelegatemodel_p.h>
#include <private/qqmlobjectmodel_p.h>

QT_BEGIN_NAMESPACE

void QQmlModelsModule::defineModule()
{
    const char uri[] = "QtQml.Models";

    qmlRegisterType<QQmlListElement>(uri, 2, 1, "ListElement");
    qmlRegisterCustomType<QQmlListModel>(uri, 2, 1, "ListModel", new QQmlListModelParser);
    qmlRegisterType<QQmlDelegateModel>(uri, 2, 1, "DelegateModel");
    qmlRegisterType<QQmlDelegateModelGroup>(uri, 2, 1, "DelegateModelGroup");
    qmlRegisterType<QQmlObjectModel>(uri, 2, 1, "ObjectModel");
    qmlRegisterType<QQmlObjectModel,3>(uri, 2, 3, "ObjectModel");

    qmlRegisterType<QItemSelectionModel>(uri, 2, 2, "ItemSelectionModel");
}

QT_END_NAMESPACE
