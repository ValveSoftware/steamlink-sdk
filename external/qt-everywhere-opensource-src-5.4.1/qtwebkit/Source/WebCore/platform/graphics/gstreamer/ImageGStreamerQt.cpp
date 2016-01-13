/*
    Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"
#include "ImageGStreamer.h"

#if ENABLE(VIDEO) && USE(GSTREAMER)
#include <gst/gst.h>
#include <gst/video/video.h>

#ifdef GST_API_VERSION_1
#include <gst/video/gstvideometa.h>
#endif

#include <wtf/gobject/GOwnPtr.h>

using namespace WebCore;

ImageGStreamer::ImageGStreamer(GstBuffer* buffer, GstCaps* caps)
#ifdef GST_API_VERSION_1
    : m_buffer(buffer)
#endif
{
    GstVideoFormat format;
    IntSize size;
    int pixelAspectRatioNumerator, pixelAspectRatioDenominator, stride;
    getVideoSizeAndFormatFromCaps(caps, size, format, pixelAspectRatioNumerator, pixelAspectRatioDenominator, stride);

#ifdef GST_API_VERSION_1
    gst_buffer_map(buffer, &m_mapInfo, GST_MAP_READ);
    uchar* bufferData = reinterpret_cast<uchar*>(m_mapInfo.data);
#else
    uchar* bufferData = reinterpret_cast<uchar*>(GST_BUFFER_DATA(buffer));
#endif
    QImage::Format imageFormat;
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
    imageFormat = (format == GST_VIDEO_FORMAT_BGRA) ? QImage::Format_ARGB32_Premultiplied : QImage::Format_RGB32;
#else
    imageFormat = (format == GST_VIDEO_FORMAT_ARGB) ? QImage::Format_ARGB32_Premultiplied : QImage::Format_RGB32;
#endif

    QImage image(bufferData, size.width(), size.height(), imageFormat);

    QPixmap *surface = new QPixmap(QPixmap::fromImage(qMove(image), Qt::NoFormatConversion));
    m_image = BitmapImage::create(surface);

#ifdef GST_API_VERSION_1
    if (GstVideoCropMeta* cropMeta = gst_buffer_get_video_crop_meta(buffer))
        setCropRect(FloatRect(cropMeta->x, cropMeta->y, cropMeta->width, cropMeta->height));
#endif
}

ImageGStreamer::~ImageGStreamer()
{
    if (m_image)
        m_image.clear();

    m_image = 0;

#ifdef GST_API_VERSION_1
    // We keep the buffer memory mapped until the image is destroyed because the internal
    // QImage/QPixmap was created using the buffer data directly.
    gst_buffer_unmap(m_buffer.get(), &m_mapInfo);
#endif
}
#endif // USE(GSTREAMER)

