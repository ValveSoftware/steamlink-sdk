/***************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtLocation module of the Qt Toolkit.
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
#include "error_messages.h"

QT_BEGIN_NAMESPACE

const char CONTEXT_NAME[] = "QtLocationQML";

//to-be-translated error string

const char PLUGIN_PROPERTY_NOT_SET[] = QT_TRANSLATE_NOOP("QtLocationQML", "Plugin property is not set.");
const char PLUGIN_ERROR[] = QT_TRANSLATE_NOOP("QtLocationQML", "Plugin Error (%1): %2");
const char PLUGIN_PROVIDER_ERROR[] = QT_TRANSLATE_NOOP("QtLocationQML", "Plugin Error (%1): Could not instantiate provider");
const char PLUGIN_NOT_VALID[] = QT_TRANSLATE_NOOP("QtLocationQML", "Plugin is not valid");
const char CATEGORIES_NOT_INITIALIZED[] = QT_TRANSLATE_NOOP("QtLocationQML", "Unable to initialize categories");
const char UNABLE_TO_MAKE_REQUEST[] = QT_TRANSLATE_NOOP("QtLocationQML", "Unable to create request");
const char INDEX_OUT_OF_RANGE[] = QT_TRANSLATE_NOOP("QtLocationQML", "Index '%1' out of range");

QT_END_NAMESPACE
