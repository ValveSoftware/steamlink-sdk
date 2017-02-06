/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
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

#ifndef TYPE_CONVERSION_H
#define TYPE_CONVERSION_H

#include <QColor>
#include <QDateTime>
#include <QDir>
#include <QIcon>
#include <QImage>
#include <QMatrix4x4>
#include <QNetworkCookie>
#include <QRect>
#include <QString>
#include <QUrl>
#include <base/strings/nullable_string16.h>
#include "base/files/file_path.h"
#include "base/time/time.h"
#include "content/public/common/file_chooser_file_info.h"
#include "content/public/common/favicon_url.h"
#include "favicon_manager.h"
#include "net/cookies/canonical_cookie.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPixelRef.h"
#include "third_party/skia/include/core/SkMatrix44.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "url/gurl.h"

namespace gfx {
class ImageSkiaRep;
}

namespace QtWebEngineCore {

inline QString toQt(const base::string16 &string)
{
#if defined(OS_WIN)
    return QString::fromStdWString(string.data());
#else
    return QString::fromUtf16(string.data());
#endif
}

inline QString toQt(const std::string &string)
{
    return QString::fromStdString(string);
}

inline QByteArray toQByteArray(const std::string &string)
{
    return QByteArray::fromStdString(string);
}

inline base::string16 toString16(const QString &qString)
{
#if defined(OS_WIN)
    return base::string16(qString.toStdWString());
#else
    return base::string16(qString.utf16());
#endif
}

inline base::NullableString16 toNullableString16(const QString &qString)
{
    return base::NullableString16(toString16(qString), qString.isNull());
}

inline QUrl toQt(const GURL &url)
{
    return QUrl(QString::fromStdString(url.spec()));
}

inline GURL toGurl(const QUrl& url)
{
    return GURL(url.toString().toStdString());
}

inline QPoint toQt(const gfx::Point &point)
{
    return QPoint(point.x(), point.y());
}

inline QPointF toQt(const gfx::Vector2dF &point)
{
    return QPointF(point.x(), point.y());
}

inline gfx::Point toGfx(const QPoint& point)
{
  return gfx::Point(point.x(), point.y());
}

inline QRect toQt(const gfx::Rect &rect)
{
    return QRect(rect.x(), rect.y(), rect.width(), rect.height());
}

inline QRectF toQt(const gfx::RectF &rect)
{
    return QRectF(rect.x(), rect.y(), rect.width(), rect.height());
}

inline QSize toQt(const gfx::Size &size)
{
    return QSize(size.width(), size.height());
}

inline gfx::SizeF toGfx(const QSizeF& size)
{
  return gfx::SizeF(size.width(), size.height());
}

inline QSizeF toQt(const gfx::SizeF &size)
{
    return QSizeF(size.width(), size.height());
}

inline QColor toQt(const SkColor &c)
{
    return QColor(SkColorGetR(c), SkColorGetG(c), SkColorGetB(c), SkColorGetA(c));
}

inline SkColor toSk(const QColor &c)
{
    return c.rgba();
}

inline QImage toQImage(const SkBitmap &bitmap, QImage::Format format)
{
    SkPixelRef *pixelRef = bitmap.pixelRef();
    return QImage((uchar *)pixelRef->pixels(), bitmap.width(), bitmap.height(), format);
}

QImage toQImage(const SkBitmap &bitmap);
QImage toQImage(const gfx::ImageSkiaRep &imageSkiaRep);
QIcon toQIcon(const std::vector<SkBitmap> &bitmaps);

inline QMatrix4x4 toQt(const SkMatrix44 &m)
{
    QMatrix4x4 qtMatrix(
        m.get(0, 0), m.get(0, 1), m.get(0, 2), m.get(0, 3),
        m.get(1, 0), m.get(1, 1), m.get(1, 2), m.get(1, 3),
        m.get(2, 0), m.get(2, 1), m.get(2, 2), m.get(2, 3),
        m.get(3, 0), m.get(3, 1), m.get(3, 2), m.get(3, 3));
    qtMatrix.optimize();
    return qtMatrix;
}

inline QDateTime toQt(base::Time time)
{
    return QDateTime::fromMSecsSinceEpoch(time.ToJavaTime());
}

inline base::Time toTime(const QDateTime &dateTime) {
    return base::Time::FromInternalValue(dateTime.toMSecsSinceEpoch());
}

inline QNetworkCookie toQt(const net::CanonicalCookie & cookie)
{
    QNetworkCookie qCookie = QNetworkCookie(QByteArray::fromStdString(cookie.Name()), QByteArray::fromStdString(cookie.Value()));
    qCookie.setDomain(toQt(cookie.Domain()));
    if (!cookie.ExpiryDate().is_null())
        qCookie.setExpirationDate(toQt(cookie.ExpiryDate()));
    qCookie.setHttpOnly(cookie.IsHttpOnly());
    qCookie.setPath(toQt(cookie.Path()));
    qCookie.setSecure(cookie.IsSecure());
    return qCookie;
}

inline base::FilePath::StringType toFilePathString(const QString &str)
{
#if defined(OS_WIN)
    return QDir::toNativeSeparators(str).toStdWString();
#else
    return str.toStdString();
#endif
}

inline base::FilePath toFilePath(const QString &str)
{
    return base::FilePath(toFilePathString(str));
}

template <typename T>
inline T fileListingHelper(const QString &)
// Clang is still picky about this though it should be supported eventually.
// See http://www.open-std.org/jtc1/sc22/wg21/docs/cwg_defects.html#941
#ifndef Q_CC_CLANG
= delete;
#else
{ return T(); }
#endif

template <>
inline content::FileChooserFileInfo fileListingHelper<content::FileChooserFileInfo>(const QString &file)
{
    content::FileChooserFileInfo choose_file;
    base::FilePath fp(toFilePath(file));
    choose_file.file_path = fp;
    choose_file.display_name = fp.BaseName().value();
    return choose_file;
}

template <>
inline base::FilePath fileListingHelper<base::FilePath>(const QString &file)
{
    return base::FilePath(toFilePathString(file));
}


template <typename T>
inline std::vector<T> toVector(const QStringList &fileList)
{
    std::vector<T> selectedFiles;
    selectedFiles.reserve(fileList.size());
    Q_FOREACH (const QString &file, fileList)
        selectedFiles.push_back(fileListingHelper<T>(file));
    return selectedFiles;
}

int flagsFromModifiers(Qt::KeyboardModifiers modifiers);

inline QStringList fromVector(const std::vector<base::string16> &vector)
{
    QStringList result;
    for (auto s: vector) {
      result.append(toQt(s));
    }
    return result;
}

FaviconInfo toFaviconInfo(const content::FaviconURL &);

} // namespace QtWebEngineCore

#endif // TYPE_CONVERSION_H
