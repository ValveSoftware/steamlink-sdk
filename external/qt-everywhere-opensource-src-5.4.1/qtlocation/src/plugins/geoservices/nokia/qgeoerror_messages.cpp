/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtLocation module of the Qt Toolkit.
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
** This file is part of the Ovi services plugin for the Maps and
** Navigation API.  The use of these services, whether by use of the
** plugin or by other means, is governed by the terms and conditions
** described by the file OVI_SERVICES_TERMS_AND_CONDITIONS.txt in
** this package, located in the directory containing the Ovi services
** plugin source code.
**
****************************************************************************/

#include "qgeoerror_messages.h"

QT_BEGIN_NAMESPACE

const char NOKIA_PLUGIN_CONTEXT_NAME[] = "QtLocationQML";
const char MISSED_CREDENTIALS[] = QT_TRANSLATE_NOOP("QtLocationQML", "Qt Location requires app_id and token parameters.\nPlease register at https://developer.here.com/ to get your personal application credentials.");
const char SAVING_PLACE_NOT_SUPPORTED[] = QT_TRANSLATE_NOOP("QtLocationQML", "Saving places is not supported.");
const char REMOVING_PLACE_NOT_SUPPORTED[] = QT_TRANSLATE_NOOP("QtLocationQML", "Removing places is not supported.");
const char SAVING_CATEGORY_NOT_SUPPORTED[] = QT_TRANSLATE_NOOP("QtLocationQML", "Saving categories is not supported.");
const char REMOVING_CATEGORY_NOT_SUPPORTED[] = QT_TRANSLATE_NOOP("QtLocationQML", "Removing categories is not supported.");
const char PARSE_ERROR[] = QT_TRANSLATE_NOOP("QtLocationQML", "Error parsing response.");
const char NETWORK_ERROR[] = QT_TRANSLATE_NOOP("QtLocationQML", "Network error.");
const char CANCEL_ERROR[] = QT_TRANSLATE_NOOP("QtLocationQML", "Request was canceled.");
const char RESPONSE_NOT_RECOGNIZABLE[] = QT_TRANSLATE_NOOP("QtLocationQML", "The response from the service was not in a recognizable format.");

QT_END_NAMESPACE
