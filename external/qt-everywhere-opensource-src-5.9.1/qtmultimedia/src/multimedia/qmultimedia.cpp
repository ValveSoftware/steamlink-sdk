/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the documentation of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:FDL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Free Documentation License Usage
** Alternatively, this file may be used under the terms of the GNU Free
** Documentation License version 1.3 as published by the Free Software
** Foundation and appearing in the file included in the packaging of
** this file. Please review the following information to ensure
** the GNU Free Documentation License version 1.3 requirements
** will be met: http://www.gnu.org/copyleft/fdl.html.
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qmultimedia.h"

QT_BEGIN_NAMESPACE

/*!
    \namespace QMultimedia
    \ingroup multimedia-namespaces
    \ingroup multimedia
    \inmodule QtMultimedia

    \ingroup multimedia
    \ingroup multimedia_core

    \brief The QMultimedia namespace contains miscellaneous identifiers used throughout the Qt Multimedia library.

*/

static void qRegisterMultimediaMetaTypes()
{
    qRegisterMetaType<QMultimedia::AvailabilityStatus>();
    qRegisterMetaType<QMultimedia::SupportEstimate>();
    qRegisterMetaType<QMultimedia::EncodingMode>();
    qRegisterMetaType<QMultimedia::EncodingQuality>();
}

Q_CONSTRUCTOR_FUNCTION(qRegisterMultimediaMetaTypes)


/*!
    \enum QMultimedia::SupportEstimate

    Enumerates the levels of support a media service provider may have for a feature.

    \value NotSupported The feature is not supported.
    \value MaybeSupported The feature may be supported.
    \value ProbablySupported The feature is probably supported.
    \value PreferredService The service is the preferred provider of a service.
*/

/*!
    \enum QMultimedia::EncodingQuality

    Enumerates quality encoding levels.

    \value VeryLowQuality
    \value LowQuality
    \value NormalQuality
    \value HighQuality
    \value VeryHighQuality
*/

/*!
    \enum QMultimedia::EncodingMode

    Enumerates encoding modes.

    \value ConstantQualityEncoding Encoding will aim to have a constant quality, adjusting bitrate to fit.
    \value ConstantBitRateEncoding Encoding will use a constant bit rate, adjust quality to fit.
    \value AverageBitRateEncoding Encoding will try to keep an average bitrate setting, but will use
            more or less as needed.
    \value TwoPassEncoding The media will first be processed to determine the characteristics,
            and then processed a second time allocating more bits to the areas
            that need it.
*/

/*!
    \enum QMultimedia::AvailabilityStatus

    Enumerates Service status errors.

    \value Available The service is operating correctly.
    \value ServiceMissing There is no service available to provide the requested functionality.
    \value ResourceError The service could not allocate resources required to function correctly.
    \value Busy The service must wait for access to necessary resources.
*/

QT_END_NAMESPACE
