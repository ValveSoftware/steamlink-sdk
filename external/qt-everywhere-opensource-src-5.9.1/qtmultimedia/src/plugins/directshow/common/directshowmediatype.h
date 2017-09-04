/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Toolkit.
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

#ifndef DIRECTSHOWMEDIATYPE_H
#define DIRECTSHOWMEDIATYPE_H

#include <dshow.h>

#include <qvideosurfaceformat.h>

#include <dvdmedia.h>
#include <QtCore/qglobal.h>

QT_BEGIN_NAMESPACE

class DirectShowMediaType
{
public:
    DirectShowMediaType();
    DirectShowMediaType(const DirectShowMediaType &other);
    DirectShowMediaType(DirectShowMediaType &&other);
    explicit DirectShowMediaType(const AM_MEDIA_TYPE &type);
    explicit DirectShowMediaType(AM_MEDIA_TYPE &&type);
    ~DirectShowMediaType() { clear(mediaType); }

    DirectShowMediaType &operator =(const DirectShowMediaType &other);
    DirectShowMediaType &operator =(DirectShowMediaType &&other);

    void clear() { clear(mediaType); }

    inline AM_MEDIA_TYPE *operator &() Q_DECL_NOTHROW { return &mediaType; }
    inline AM_MEDIA_TYPE *operator ->() Q_DECL_NOTHROW { return &mediaType; }

    inline const AM_MEDIA_TYPE *operator &() const Q_DECL_NOTHROW { return &mediaType; }
    inline const AM_MEDIA_TYPE *operator ->() const Q_DECL_NOTHROW { return &mediaType; }

    static void init(AM_MEDIA_TYPE *type);
    static void copy(AM_MEDIA_TYPE *target, const AM_MEDIA_TYPE *source);
    static void copyToUninitialized(AM_MEDIA_TYPE *target, const AM_MEDIA_TYPE *source);
    static void move(AM_MEDIA_TYPE *target, AM_MEDIA_TYPE **source);
    static void move(AM_MEDIA_TYPE *target, AM_MEDIA_TYPE &source);
    static void clear(AM_MEDIA_TYPE &type);
    static void deleteType(AM_MEDIA_TYPE *type);
    static bool isPartiallySpecified(const AM_MEDIA_TYPE *mediaType);
    static bool isCompatible(const AM_MEDIA_TYPE *a, const AM_MEDIA_TYPE *b);
    static GUID convertPixelFormat(QVideoFrame::PixelFormat format);

    static QVideoSurfaceFormat videoFormatFromType(const AM_MEDIA_TYPE *type);
    static QVideoFrame::PixelFormat pixelFormatFromType(const AM_MEDIA_TYPE *type);
    static int bytesPerLine(const QVideoSurfaceFormat &format);
    static QVideoSurfaceFormat::Direction scanLineDirection(QVideoFrame::PixelFormat pixelFormat, const BITMAPINFOHEADER &bmiHeader);

private:
    AM_MEDIA_TYPE mediaType;
};

Q_DECLARE_TYPEINFO(DirectShowMediaType, Q_MOVABLE_TYPE);

QT_END_NAMESPACE

#endif
