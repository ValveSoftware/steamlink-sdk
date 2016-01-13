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

#ifndef QGSTUTILS_P_H
#define QGSTUTILS_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API. It exists purely as an
// implementation detail. This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <QtCore/qmap.h>
#include <QtCore/qset.h>
#include <QtCore/qvector.h>
#include <gst/gst.h>
#include <qaudioformat.h>
#include <qcamera.h>

QT_BEGIN_NAMESPACE

class QSize;
class QVariant;
class QByteArray;

namespace QGstUtils {
    struct CameraInfo
    {
        QString name;
        QString description;
        int orientation;
        QCamera::Position position;
        QByteArray driver;
    };

    QMap<QByteArray, QVariant> gstTagListToMap(const GstTagList *list);

    QSize capsResolution(const GstCaps *caps);
    QSize capsCorrectedResolution(const GstCaps *caps);
    QAudioFormat audioFormatForCaps(const GstCaps *caps);
    QAudioFormat audioFormatForBuffer(GstBuffer *buffer);
    GstCaps *capsForAudioFormat(QAudioFormat format);
    void initializeGst();
    QMultimedia::SupportEstimate hasSupport(const QString &mimeType,
                                             const QStringList &codecs,
                                             const QSet<QString> &supportedMimeTypeSet);

    QVector<CameraInfo> enumerateCameras(GstElementFactory *factory = 0);
    QList<QByteArray> cameraDevices(GstElementFactory * factory = 0);
    QString cameraDescription(const QString &device, GstElementFactory * factory = 0);
    QCamera::Position cameraPosition(const QString &device, GstElementFactory * factory = 0);
    int cameraOrientation(const QString &device, GstElementFactory * factory = 0);
    QByteArray cameraDriver(const QString &device, GstElementFactory * factory = 0);
}

void qt_gst_object_ref_sink(gpointer object);

QT_END_NAMESPACE

#endif
