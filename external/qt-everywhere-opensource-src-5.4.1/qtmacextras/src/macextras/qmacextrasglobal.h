/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtMacExtras module of the Qt Toolkit.
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

#ifndef QMACEXTRASGLOBAL_H
#define QMACEXTRASGLOBAL_H

#include <QtCore/qglobal.h>

QT_BEGIN_NAMESPACE

#if defined(QT_BUILD_MACEXTRAS_LIB)
#  define Q_MACEXTRAS_EXPORT Q_DECL_EXPORT
#else
#  define Q_MACEXTRAS_EXPORT Q_DECL_IMPORT
#endif


// ### remove when merged to QtCore
/*!
 * \macro Q_FORWARD_DECLARE_OBJC_CLASS(classname)
 *
 * Forward-declares an Objective-C class name in a manner such that it can be
 * compiled as either Objective-C or C++.
 *
 * This is primarily intended for use in header files that may be included by
 * both Objective-C and C++ source files.
 */
#ifdef __OBJC__
#define Q_FORWARD_DECLARE_OBJC_CLASS(classname) @class classname
#else
#define Q_FORWARD_DECLARE_OBJC_CLASS(classname) typedef struct objc_object classname
#endif

QT_END_NAMESPACE

#endif // QMACEXTRASGLOBAL_H
