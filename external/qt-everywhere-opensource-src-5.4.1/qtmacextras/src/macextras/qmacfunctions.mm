/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtMacExtras module of the Qt Toolkit.
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
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qmacfunctions.h"

#include <QByteArray>
#include <QPixmap>
#include <QUrl>
#include <QString>

#include "qmacfunctions_p.h"

#import <Foundation/Foundation.h>

#ifdef Q_OS_IOS
 #import <CoreGraphics/CoreGraphics.h>
#endif

QT_BEGIN_NAMESPACE

/*!
    \namespace QtMac
    \inmodule QtMacExtras
    \since 5.2
    \brief The QtMac namespace contains miscellaneous functionality specific to the OS X operating system.
    \inheaderfile QtMac
 */
namespace QtMac
{

#if QT_DEPRECATED_SINCE(5,3)
/*!
    \obsolete

    Use QByteArray::toNSData() instead.
 */
NSData* toNSData(const QByteArray &data)
{
    return [NSData dataWithBytes:data.constData() length:data.size()];
}

/*!
    \obsolete

    Use QByteArray::fromNSData() instead.
 */
QByteArray fromNSData(const NSData *data)
{
    QByteArray ba;
    ba.resize([data length]);
    [data getBytes:ba.data() length:ba.size()];
    return ba;
}
#endif // QT_DEPRECATED_SINCE

/*!
    Creates a \c CGImageRef equivalent to the QPixmap. Returns the \c CGImageRef handle.

    It is the caller's responsibility to release the \c CGImageRef data
    after use.

    This function is not available in Qt 5.x until 5.0.2 and will return NULL in earlier versions.

    \sa fromCGImageRef()
*/
CGImageRef toCGImageRef(const QPixmap &pixmap)
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
    QPlatformNativeInterface::NativeResourceForIntegrationFunction function = resolvePlatformFunction("qimagetocgimage");
    if (function) {
        typedef CGImageRef (*QImageToCGImageFunction)(const QImage &image);
        return reinterpret_cast<QImageToCGImageFunction>(function)(pixmap.toImage());
    }

    return NULL;
#else
    return pixmap.toCGImageRef();
#endif
}

/*!
    Returns a QPixmap that is equivalent to the given \a image.

    This function is not available in Qt 5.x until 5.0.2 and will return a null pixmap in earlier versions.

    \sa toCGImageRef(), {QPixmap#Pixmap Conversion}{Pixmap Conversion}
*/
QPixmap fromCGImageRef(CGImageRef image)
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
    QPlatformNativeInterface::NativeResourceForIntegrationFunction function = resolvePlatformFunction("cgimagetoqimage");
    if (function) {
        typedef QImage (*CGImageToQImageFunction)(CGImageRef image);
        return QPixmap::fromImage(reinterpret_cast<CGImageToQImageFunction>(function)(image));
    }

    return QPixmap();
#else
    return QPixmap::fromCGImageRef(image);
#endif
}

} // namespace QtMac

QT_END_NAMESPACE
