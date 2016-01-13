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

#include "qgstreamerv4l2input.h"

#include <QtCore/qdebug.h>
#include <QtCore/qfile.h>

#include <private/qcore_unix_p.h>
#include <linux/videodev2.h>

QT_BEGIN_NAMESPACE
static inline uint qHash(const QSize& key) { return uint(key.width()*256+key.height()); }

static bool operator<(const QSize &s1, const QSize s2)
{
    return s1.width()*s1.height() < s2.width()*s2.height();
}
QT_END_NAMESPACE

QGstreamerV4L2Input::QGstreamerV4L2Input(QObject *parent)
    :QObject(parent)
{
}

QGstreamerV4L2Input::~QGstreamerV4L2Input()
{
}

GstElement *QGstreamerV4L2Input::buildElement()
{
    GstElement *camera = gst_element_factory_make("v4l2src", "camera_source");
    if (camera && !m_device.isEmpty() )
        g_object_set(G_OBJECT(camera), "device", m_device.constData(), NULL);

    return camera;
}

void QGstreamerV4L2Input::setDevice(const QByteArray &newDevice)
{
    if (m_device != newDevice) {
        m_device = newDevice;
        updateSupportedResolutions(newDevice);
    }
}

void QGstreamerV4L2Input::setDevice(const QString &device)
{
    setDevice(QFile::encodeName(device));
}

void QGstreamerV4L2Input::updateSupportedResolutions(const QByteArray &device)
{
    m_frameRates.clear();
    m_resolutions.clear();
    m_ratesByResolution.clear();

    QSet<QSize> allResolutions;
    QSet<int> allFrameRates;

    QFile f(device);

    if (!f.open(QFile::ReadOnly))
        return;

    int fd = f.handle();

    //get the list of formats:
    QList<quint32> supportedFormats;

    {
        v4l2_fmtdesc fmt;
        memset(&fmt, 0, sizeof(v4l2_fmtdesc));

        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        int sanity = 0;

        for (fmt.index = 0;; fmt.index++) {
            if (sanity++ > 8)
                break;
            if(  ::ioctl(fd, VIDIOC_ENUM_FMT, &fmt) == -1) {
                if(errno == EINVAL)
                    break;
            }
            supportedFormats.append(fmt.pixelformat);
        }
    }

    QList<QSize> commonSizes;
    commonSizes << QSize(128, 96)
            <<QSize(160,120)
            <<QSize(176, 144)
            <<QSize(320, 240)
            <<QSize(352, 288)
            <<QSize(640, 480)
            <<QSize(1024, 768)
            <<QSize(1280, 1024)
            <<QSize(1600, 1200)
            <<QSize(1920, 1200)
            <<QSize(2048, 1536)
            <<QSize(2560, 1600)
            <<QSize(2580, 1936);

    QList<int> commonRates;
    commonRates << 05*1000 << 75*1000  << 10*1000 << 15*1000 << 20*1000
            << 24*1000 << 25*1000 << 30*1000 << 50*1000 << 60*1000;


    //get the list of resolutions:

    foreach (quint32 format, supportedFormats) {
        struct v4l2_frmsizeenum formatSize;
        memset(&formatSize, 0, sizeof(formatSize));
        formatSize.pixel_format = format;

        QList<QSize> sizeList;

        if (0) {
            char formatStr[5];
            memcpy(formatStr, &format, 4);
            formatStr[4] = 0;
            //qDebug() << "trying format" << formatStr;
        }

        for (int i=0;;i++) {
            formatSize.index = i;
            if (ioctl (fd, VIDIOC_ENUM_FRAMESIZES, &formatSize) < 0)
                break;

            if (formatSize.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
                sizeList.append(QSize(formatSize.discrete.width, formatSize.discrete.height));
            } else {

                foreach (const QSize& candidate, commonSizes) {
                    if (candidate.width() <= (int)formatSize.stepwise.max_width &&
                        candidate.height() >= (int)formatSize.stepwise.min_width &&
                        candidate.width() % formatSize.stepwise.step_width == 0 &&
                        candidate.height() <= (int)formatSize.stepwise.max_height &&
                        candidate.height() >= (int)formatSize.stepwise.min_height &&
                        candidate.height() % formatSize.stepwise.step_height == 0) {
                        sizeList.append(candidate);
                    }
                }

                if (!sizeList.contains(QSize(formatSize.stepwise.min_width, formatSize.stepwise.min_height)))
                    sizeList.prepend(QSize(formatSize.stepwise.min_width, formatSize.stepwise.min_height));

                if (!sizeList.contains(QSize(formatSize.stepwise.max_width, formatSize.stepwise.max_height)))
                    sizeList.append(QSize(formatSize.stepwise.max_width, formatSize.stepwise.max_height));

                break; //stepwise values are returned only for index 0
            }

        }

        //and frameRates for each resolution.

        foreach (const QSize &s, sizeList) {
            allResolutions.insert(s);

            struct v4l2_frmivalenum formatInterval;
            memset(&formatInterval, 0, sizeof(formatInterval));
            formatInterval.pixel_format = format;
            formatInterval.width = s.width();
            formatInterval.height = s.height();

            QList<int> frameRates; //in 1/1000 of fps

            for (int i=0; ; i++) {
                formatInterval.index = i;

                if (ioctl(fd, VIDIOC_ENUM_FRAMEINTERVALS, &formatInterval) < 0)
                    break;

                if (formatInterval.type == V4L2_FRMIVAL_TYPE_DISCRETE) {
                    //converts seconds to fps*1000
                    if (formatInterval.discrete.numerator)
                        frameRates.append(qRound(formatInterval.discrete.denominator*1000.0 / formatInterval.discrete.numerator));
                } else {
                    if (formatInterval.stepwise.min.numerator == 0 ||
                        formatInterval.stepwise.max.numerator == 0) {
                        qWarning() << "received invalid frame interval";
                        break;
                    }


                    int minRate = qRound(formatInterval.stepwise.min.denominator*1000.0 /
                                         formatInterval.stepwise.min.numerator);

                    int maxRate = qRound(formatInterval.stepwise.max.denominator*1000.0 /
                                         formatInterval.stepwise.max.numerator);


                    foreach (int candidate, commonRates) {
                        if (candidate >= minRate && candidate <= maxRate)
                            frameRates.append(candidate);
                    }

                    if (!frameRates.contains(minRate))
                        frameRates.prepend(minRate);

                    if (!frameRates.contains(maxRate))
                        frameRates.append(maxRate);

                    break; //stepwise values are returned only for index 0
                }
            }
            allFrameRates.unite(frameRates.toSet());
            m_ratesByResolution[s].unite(frameRates.toSet());
        }
    }

    f.close();

    foreach(int rate, allFrameRates) {
        m_frameRates.append(rate/1000.0);
    }

    qSort(m_frameRates);

    m_resolutions = allResolutions.toList();
    qSort(m_resolutions);

    //qDebug() << "frame rates:" << m_frameRates;
    //qDebug() << "resolutions:" << m_resolutions;
}


QList<qreal> QGstreamerV4L2Input::supportedFrameRates(const QSize &frameSize) const
{
    if (frameSize.isEmpty())
        return m_frameRates;
    else {
        QList<qreal> res;
        foreach(int rate, m_ratesByResolution[frameSize]) {
            res.append(rate/1000.0);
        }
        return res;
    }
}

QList<QSize> QGstreamerV4L2Input::supportedResolutions(qreal frameRate) const
{
    Q_UNUSED(frameRate);
    return m_resolutions;
}
