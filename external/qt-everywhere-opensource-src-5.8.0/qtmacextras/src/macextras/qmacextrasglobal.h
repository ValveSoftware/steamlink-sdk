/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtMacExtras module of the Qt Toolkit.
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
