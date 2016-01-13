/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtWinExtras module of the Qt Toolkit.
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

#include "qwinmime.h"

#include <QtGui/QGuiApplication>
#include <QtCore/QMetaObject>
#include <QtCore/QDebug>

#include <qpa/qplatformnativeinterface.h>

QT_BEGIN_NAMESPACE

/*!
    \class QWinMime
    \inmodule QtWinExtras
    \brief The QWinMime class maps open-standard MIME to Window Clipboard formats.

    Qt's drag-and-drop and clipboard facilities use the MIME standard.
    On X11, this maps trivially to the Xdnd protocol, but on Windows
    although some applications use MIME types to describe clipboard
    formats, others use arbitrary non-standardized naming conventions,
    or unnamed built-in formats of Windows.

    By instantiating subclasses of QWinMime that provide conversions
    between Windows Clipboard and MIME formats, you can convert
    proprietary clipboard formats to MIME formats.

    Qt has predefined support for the following Windows Clipboard formats:

    \table
    \header \li Windows Format \li Equivalent MIME type
    \row \li \c CF_UNICODETEXT \li \c text/plain
    \row \li \c CF_TEXT        \li \c text/plain
    \row \li \c CF_DIB         \li \c{image/xyz}, where \c xyz is
                                 a \l{QImageWriter::supportedImageFormats()}{Qt image format}
    \row \li \c CF_HDROP       \li \c text/uri-list
    \row \li \c CF_INETURL     \li \c text/uri-list
    \row \li \c CF_HTML        \li \c text/html
    \endtable

    An example use of this class would be to map the Windows Metafile
    clipboard format (\c CF_METAFILEPICT) to and from the MIME type
    \c{image/x-wmf}. This conversion might simply be adding or removing
    a header, or even just passing on the data. See \l{Drag and Drop}
    for more information on choosing and definition MIME types.

    You can check if a MIME type is convertible using canConvertFromMime() and
    can perform conversions with convertToMime() and convertFromMime().

    \since 5.4
*/

/*!
    Constructs a new conversion object, adding it to the globally accessed
    list of available converters.
*/
QWinMime::QWinMime()
{
    if (!QMetaObject::invokeMethod(QGuiApplication::platformNativeInterface(),
                                   "registerWindowsMime", Q_ARG(void *, this))) {
        qWarning() << Q_FUNC_INFO << "Unable to register mime type.";
    }
}

/*!
    Destroys a conversion object, removing it from the global
    list of available converters.
*/
QWinMime::~QWinMime()
{
    if (!QMetaObject::invokeMethod(QGuiApplication::platformNativeInterface(),
                                   "unregisterWindowsMime", Q_ARG(void *, this))) {
        qWarning() << Q_FUNC_INFO << "Unable to unregister mime type.";
    }
}

/*!
    Registers the MIME type \a mime, and returns an ID number
    identifying the format on Windows.
*/
int QWinMime::registerMimeType(const QString &mime)
{
    int result = 0;
    if (!QMetaObject::invokeMethod(QGuiApplication::platformNativeInterface(),
                                   "registerMimeType",
                                   Q_RETURN_ARG(int, result),
                                   Q_ARG(QString, mime))) {
        qWarning() << Q_FUNC_INFO << "Unable to register mime type " << mime;
    }
    return result;
}

/*!
    \fn bool QWinMime::canConvertFromMime(const FORMATETC &formatetc, const QMimeData *mimeData) const

    Returns true if the converter can convert from the \a mimeData to
    the format specified in \a formatetc.

    All subclasses must reimplement this pure virtual function.
*/

/*!
    \fn bool QWinMime::canConvertToMime(const QString &mimeType, IDataObject *pDataObj) const

    Returns true if the converter can convert to the \a mimeType from
    the available formats in \a pDataObj.

    All subclasses must reimplement this pure virtual function.
*/

/*!
    \fn QString QWinMime::mimeForFormat(const FORMATETC &formatetc) const

    Returns the mime type that will be created form the format specified
    in \a formatetc, or an empty string if this converter does not support
    \a formatetc.

    All subclasses must reimplement this pure virtual function.
*/

/*!
    \fn QVector<FORMATETC> QWinMime::formatsForMime(const QString &mimeType, const QMimeData *mimeData) const

    Returns a QVector of FORMATETC structures representing the different windows clipboard
    formats that can be provided for the \a mimeType from the \a mimeData.

    All subclasses must reimplement this pure virtual function.
*/

/*!
    \fn QVariant QWinMime::convertToMime(const QString &mimeType, IDataObject *pDataObj,
                                             QVariant::Type preferredType) const

    Returns a QVariant containing the converted data for \a mimeType from \a pDataObj.
    If possible the QVariant should be of the \a preferredType to avoid needless conversions.

    All subclasses must reimplement this pure virtual function.
*/

/*!
    \fn bool QWinMime::convertFromMime(const FORMATETC &formatetc, const QMimeData *mimeData, STGMEDIUM * pmedium) const

    Convert the \a mimeData to the format specified in \a formatetc.
    The converted data should then be placed in \a pmedium structure.

    Return true if the conversion was successful.

    All subclasses must reimplement this pure virtual function.
*/

QT_END_NAMESPACE
