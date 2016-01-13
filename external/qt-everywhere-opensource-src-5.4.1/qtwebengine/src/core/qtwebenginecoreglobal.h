/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QTWEBENGINECOREGLOBAL_H
#define QTWEBENGINECOREGLOBAL_H

#include <QtCore/qglobal.h>

#ifdef QT_WEBENGINE_LOGGING
#define QT_NOT_YET_IMPLEMENTED fprintf(stderr, "function %s not implemented! - %s:%d\n", __func__, __FILE__, __LINE__);
#define QT_NOT_USED fprintf(stderr, "# function %s should not be used! - %s:%d\n", __func__, __FILE__, __LINE__); Q_UNREACHABLE();
#else
#define QT_NOT_YET_IMPLEMENTED qt_noop();
#define QT_NOT_USED Q_UNREACHABLE(); // This will assert in debug.
#endif

#ifndef QT_STATIC
#  if defined(BUILDING_CHROMIUM)
#      define QWEBENGINE_EXPORT Q_DECL_EXPORT
#  else
#      define QWEBENGINE_EXPORT Q_DECL_IMPORT
#  endif
#else
#  define QWEBENGINE_EXPORT
#endif

#endif // QTWEBENGINECOREGLOBAL_H
