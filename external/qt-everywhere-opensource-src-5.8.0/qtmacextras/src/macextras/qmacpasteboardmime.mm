/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtMacExtras of the Qt Toolkit.
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

#include <Cocoa/Cocoa.h>
#include "qmacpasteboardmime.h"
#include "qmacfunctions_p.h"

#include <QtCore/qlogging.h>
#include <qpa/qplatformnativeinterface.h>

QT_BEGIN_NAMESPACE

/*!
    \fn void qRegisterDraggedTypes(const QStringList &types)
    \relates QMacPasteboardMime

    Registers the given \a types as custom pasteboard types.

    This function should be called to enable the Drag and Drop events
    for custom pasteboard types on Cocoa implementations. This is required
    in addition to a QMacPasteboardMime subclass implementation. By default
    drag and drop is enabled for all standard pasteboard types.

    \sa QMacPasteboardMime
*/
void qRegisterDraggedTypes(const QStringList &types)
{
    QPlatformNativeInterface::NativeResourceForIntegrationFunction function = resolvePlatformFunction("registerdraggedtypes");
    if (function) {
        typedef void (*RegisterDraggedTypesFunction)(const QStringList &types);
        reinterpret_cast<RegisterDraggedTypesFunction>(function)(types);
    }
}

/*!
  \class QMacPasteboardMime
  \inmodule QtMacExtras
  \since 5.2
  \brief The QMacPasteboardMime class converts between a MIME type and a
  \l{http://developer.apple.com/macosx/uniformtypeidentifiers.html}{Uniform
  Type Identifier (UTI)} format.

  Qt's drag and drop and clipboard facilities use the MIME
  standard. On X11, this maps trivially to the Xdnd protocol. On
  Mac, although some applications use MIME to describe clipboard
  contents, it is more common to use Apple's UTI format.

  QMacPasteboardMime's role is to bridge the gap between MIME and UTI;
  By subclasses this class, one can extend Qt's drag and drop
  and clipboard handling to convert to and from unsupported, or proprietary, UTI formats.

  A subclass of QMacPasteboardMime will automatically be registered, and active, upon instantiation.

  Qt has predefined support for the following UTIs:
  \table
    \header \li UTI
            \li Converts to
    \row \li \c public.utf8-plain-text
         \li \c text/plain
    \row \li \c public.utf16-plain-text
         \li \c text/plain
    \row \li \c public.html
         \li \c text/html
    \row \li \c public.url
         \li \c text/uri-list
    \row \li \c public.file-url
         \li \c text/uri-list
    \row \li \c public.tiff
         \li \c application/x-qt-image
    \row \li \c public.vcard
         \li \c text/plain
    \row \li \c com.apple.traditional-mac-plain-text
         \li \c text/plain
    \row \li \c com.apple.pict
         \li \c application/x-qt-image
    \endtable

  When working with MIME data, Qt will interate through all instances of QMacPasteboardMime to
  find an instance that can convert to, or from, a specific MIME type. It will do this by calling
  canConvert() on each instance, starting with (and choosing) the last created instance first.
  The actual conversions will be done by using convertToMime() and convertFromMime().

  \note The API uses the term "flavor" in some cases. This is for backwards
  compatibility reasons, and should now be understood as UTIs.
*/

/*! \enum QMacPasteboardMime::QMacPasteboardMimeType
    \internal
*/

/*!
  Constructs a new conversion object of type \a t, adding it to the
  globally accessed list of available converters.
*/
QMacPasteboardMime::QMacPasteboardMime(char t) : type(t)
{
    Q_UNUSED(type);

    QPlatformNativeInterface::NativeResourceForIntegrationFunction function = resolvePlatformFunction("addToMimeList");
    if (function) {
        typedef void (*AddToGlobalMimeListFunction)(QMacPasteboardMime *);
        reinterpret_cast<AddToGlobalMimeListFunction>(function)(this);
    }
}

/*!
  Destroys a conversion object, removing it from the global
  list of available converters.
*/
QMacPasteboardMime::~QMacPasteboardMime()
{
    QPlatformNativeInterface::NativeResourceForIntegrationFunction function = resolvePlatformFunction("removeFromMimeList");
    if (function) {
        typedef void (*RemoveFromGlobalMimeListFunction)(QMacPasteboardMime *);
        reinterpret_cast<RemoveFromGlobalMimeListFunction>(function)(this);
    }
}

/*!
  Returns the item count for the given \a mimeData
*/
int QMacPasteboardMime::count(QMimeData *mimeData)
{
    Q_UNUSED(mimeData);
    return 1;
}

/*!
  \fn QString QMacPasteboardMime::convertorName()

  Returns a name for the converter.

  All subclasses must reimplement this pure virtual function.
*/

/*!
  \fn bool QMacPasteboardMime::canConvert(const QString &mime, QString flav)

  Returns true if the converter can convert (both ways) between
  \a mime and \a flav; otherwise returns false.

  All subclasses must reimplement this pure virtual function.
*/

/*!
  \fn QString QMacPasteboardMime::mimeFor(QString flav)

  Returns the MIME UTI used for Mac flavor \a flav, or 0 if this
  converter does not support \a flav.

  All subclasses must reimplement this pure virtual function.
*/

/*!
  \fn QString QMacPasteboardMime::flavorFor(const QString &mime)

  Returns the Mac UTI used for MIME type \a mime, or 0 if this
  converter does not support \a mime.

  All subclasses must reimplement this pure virtual function.
*/

/*!
    \fn QVariant QMacPasteboardMime::convertToMime(const QString &mime, QList<QByteArray> data, QString flav)

    Returns \a data converted from Mac UTI \a flav to MIME type \a
    mime.

    Note that Mac flavors must all be self-terminating. The input \a
    data may contain trailing data.

    All subclasses must reimplement this pure virtual function.
*/

/*!
  \fn QList<QByteArray> QMacPasteboardMime::convertFromMime(const QString &mime, QVariant data, QString flav)

  Returns \a data converted from MIME type \a mime
    to Mac UTI \a flav.

  Note that Mac flavors must all be self-terminating.  The return
  value may contain trailing data.

  All subclasses must reimplement this pure virtual function.
*/

QT_END_NAMESPACE
