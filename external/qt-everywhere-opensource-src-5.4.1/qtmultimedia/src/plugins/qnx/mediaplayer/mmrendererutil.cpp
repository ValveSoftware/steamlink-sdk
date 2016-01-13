/****************************************************************************
**
** Copyright (C) 2012 Research In Motion
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
#include "mmrendererutil.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QString>
#include <QXmlStreamReader>

#include <mm/renderer.h>

QT_BEGIN_NAMESPACE

struct MmError {
    int errorCode;
    const char *name;
};

#define MM_ERROR_ENTRY(error) { error, #error }
static const MmError mmErrors[] = {
    MM_ERROR_ENTRY(MMR_ERROR_NONE),
    MM_ERROR_ENTRY(MMR_ERROR_UNKNOWN ),
    MM_ERROR_ENTRY(MMR_ERROR_INVALID_PARAMETER ),
    MM_ERROR_ENTRY(MMR_ERROR_INVALID_STATE),
    MM_ERROR_ENTRY(MMR_ERROR_UNSUPPORTED_VALUE),
    MM_ERROR_ENTRY(MMR_ERROR_UNSUPPORTED_MEDIA_TYPE),
    MM_ERROR_ENTRY(MMR_ERROR_MEDIA_PROTECTED),
    MM_ERROR_ENTRY(MMR_ERROR_UNSUPPORTED_OPERATION),
    MM_ERROR_ENTRY(MMR_ERROR_READ),
    MM_ERROR_ENTRY(MMR_ERROR_WRITE),
    MM_ERROR_ENTRY(MMR_ERROR_MEDIA_UNAVAILABLE),
    MM_ERROR_ENTRY(MMR_ERROR_MEDIA_CORRUPTED),
    MM_ERROR_ENTRY(MMR_ERROR_OUTPUT_UNAVAILABLE),
    MM_ERROR_ENTRY(MMR_ERROR_NO_MEMORY),
    MM_ERROR_ENTRY(MMR_ERROR_RESOURCE_UNAVAILABLE),
    MM_ERROR_ENTRY(MMR_ERROR_MEDIA_DRM_NO_RIGHTS),
    MM_ERROR_ENTRY(MMR_ERROR_DRM_CORRUPTED_DATA_STORE),
    MM_ERROR_ENTRY(MMR_ERROR_DRM_OUTPUT_PROTECTION),
    MM_ERROR_ENTRY(MMR_ERROR_DRM_OPL_HDMI),
    MM_ERROR_ENTRY(MMR_ERROR_DRM_OPL_DISPLAYPORT),
    MM_ERROR_ENTRY(MMR_ERROR_DRM_OPL_DVI),
    MM_ERROR_ENTRY(MMR_ERROR_DRM_OPL_ANALOG_VIDEO),
    MM_ERROR_ENTRY(MMR_ERROR_DRM_OPL_ANALOG_AUDIO),
    MM_ERROR_ENTRY(MMR_ERROR_DRM_OPL_TOSLINK),
    MM_ERROR_ENTRY(MMR_ERROR_DRM_OPL_SPDIF),
    MM_ERROR_ENTRY(MMR_ERROR_DRM_OPL_BLUETOOTH),
    MM_ERROR_ENTRY(MMR_ERROR_DRM_OPL_WIRELESSHD),
};
static const unsigned int numMmErrors = sizeof(mmErrors) / sizeof(MmError);

QString mmErrorMessage(const QString &msg, mmr_context_t *context, int *errorCode)
{
    const mmr_error_info_t * const mmError = mmr_error_info(context);

    if (errorCode)
        *errorCode = mmError->error_code;

    if (mmError->error_code < numMmErrors) {
        return QString("%1: %2 (code %3)").arg(msg).arg(mmErrors[mmError->error_code].name)
                                          .arg(mmError->error_code);
    } else {
        return QString("%1: Unknown error code %2").arg(msg).arg(mmError->error_code);
    }
}

bool checkForDrmPermission()
{
    QDir sandboxDir = QDir::home(); // always returns 'data' directory
    sandboxDir.cdUp(); // change to app sandbox directory

    QFile file(sandboxDir.filePath("app/native/bar-descriptor.xml"));
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "checkForDrmPermission: Unable to open bar-descriptor.xml";
        return false;
    }

    QXmlStreamReader reader(&file);
    while (!reader.atEnd()) {
        reader.readNextStartElement();
        if (reader.name() == QLatin1String("action")
            || reader.name() == QLatin1String("permission")) {
            if (reader.readElementText().trimmed() == QLatin1String("access_protected_media"))
                return true;
        }
    }

    return false;
}

QT_END_NAMESPACE
