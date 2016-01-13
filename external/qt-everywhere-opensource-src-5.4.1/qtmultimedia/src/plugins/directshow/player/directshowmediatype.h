/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Toolkit.
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

#ifndef DIRECTSHOWMEDIATYPE_H
#define DIRECTSHOWMEDIATYPE_H

#include <qvideosurfaceformat.h>

#include <dshow.h>
#include <dvdmedia.h>

class DirectShowMediaType : public AM_MEDIA_TYPE
{
public:
    DirectShowMediaType() { memset(this, 0, sizeof(DirectShowMediaType)); }
    DirectShowMediaType(const AM_MEDIA_TYPE &type) { copy(this, type); }
    DirectShowMediaType(const DirectShowMediaType &other) { copy(this, other); }
    DirectShowMediaType &operator =(const AM_MEDIA_TYPE &type) {
        freeData(this); copy(this, type); return *this; }
    DirectShowMediaType &operator =(const DirectShowMediaType &other) {
        freeData(this); copy(this, other); return *this; }
    ~DirectShowMediaType() { freeData(this); }

    void clear() { freeData(this); memset(this, 0, sizeof(DirectShowMediaType)); }

    static void copy(AM_MEDIA_TYPE *target, const AM_MEDIA_TYPE &source);
    static void freeData(AM_MEDIA_TYPE *type);
    static void deleteType(AM_MEDIA_TYPE *type);

    static GUID convertPixelFormat(QVideoFrame::PixelFormat format);
    static QVideoSurfaceFormat formatFromType(const AM_MEDIA_TYPE &type);

    static int bytesPerLine(const QVideoSurfaceFormat &format);

private:
    static QVideoSurfaceFormat::Direction scanLineDirection(QVideoFrame::PixelFormat pixelFormat, const BITMAPINFOHEADER &bmiHeader);
};

#endif
