/****************************************************************************
**
** Copyright (C) 2012 Denis Shienkov <denis.shienkov@gmail.com>
** Copyright (C) 2012 Laszlo Papp <lpapp@kde.org>
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtSerialPort module of the Qt Toolkit.
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

#ifndef QSERIALPORTGLOBAL_H
#define QSERIALPORTGLOBAL_H

#include <QtCore/qstring.h>
#include <QtCore/qglobal.h>

QT_BEGIN_NAMESPACE

#ifndef QT_STATIC
#  if defined(QT_BUILD_SERIALPORT_LIB)
#    define Q_SERIALPORT_EXPORT Q_DECL_EXPORT
#  else
#    define Q_SERIALPORT_EXPORT Q_DECL_IMPORT
#  endif
#else
#  define Q_SERIALPORT_EXPORT
#endif

// These macros have been available only since Qt 5.0
#ifndef QT_DEPRECATED_SINCE
#define QT_DEPRECATED_SINCE(major, minor) 1
#endif

#ifndef Q_DECL_OVERRIDE
#define Q_DECL_OVERRIDE
#endif

#ifndef QStringLiteral
#define QStringLiteral(str) QString::fromUtf8(str)
#endif

#ifndef Q_NULLPTR
#define Q_NULLPTR NULL
#endif

QT_END_NAMESPACE

#endif // QSERIALPORTGLOBAL_H
