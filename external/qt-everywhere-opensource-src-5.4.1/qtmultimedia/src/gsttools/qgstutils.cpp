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

#include "qgstutils_p.h"

#include <QtCore/qdatetime.h>
#include <QtCore/qdir.h>
#include <QtCore/qbytearray.h>
#include <QtCore/qvariant.h>
#include <QtCore/qsize.h>
#include <QtCore/qset.h>
#include <QtCore/qstringlist.h>
#include <qaudioformat.h>

#ifdef USE_V4L
#  include <private/qcore_unix_p.h>
#  include <linux/videodev2.h>
#endif

#include "qgstreamervideoinputdevicecontrol_p.h"

QT_BEGIN_NAMESPACE

//internal
static void addTagToMap(const GstTagList *list,
                        const gchar *tag,
                        gpointer user_data)
{
    QMap<QByteArray, QVariant> *map = reinterpret_cast<QMap<QByteArray, QVariant>* >(user_data);

    GValue val;
    val.g_type = 0;
    gst_tag_list_copy_value(&val,list,tag);

    switch( G_VALUE_TYPE(&val) ) {
        case G_TYPE_STRING:
        {
            const gchar *str_value = g_value_get_string(&val);
            map->insert(QByteArray(tag), QString::fromUtf8(str_value));
            break;
        }
        case G_TYPE_INT:
            map->insert(QByteArray(tag), g_value_get_int(&val));
            break;
        case G_TYPE_UINT:
            map->insert(QByteArray(tag), g_value_get_uint(&val));
            break;
        case G_TYPE_LONG:
            map->insert(QByteArray(tag), qint64(g_value_get_long(&val)));
            break;
        case G_TYPE_BOOLEAN:
            map->insert(QByteArray(tag), g_value_get_boolean(&val));
            break;
        case G_TYPE_CHAR:
            map->insert(QByteArray(tag), g_value_get_char(&val));
            break;
        case G_TYPE_DOUBLE:
            map->insert(QByteArray(tag), g_value_get_double(&val));
            break;
        default:
            // GST_TYPE_DATE is a function, not a constant, so pull it out of the switch
            if (G_VALUE_TYPE(&val) == GST_TYPE_DATE) {
                const GDate *date = gst_value_get_date(&val);
                if (g_date_valid(date)) {
                    int year = g_date_get_year(date);
                    int month = g_date_get_month(date);
                    int day = g_date_get_day(date);
                    map->insert(QByteArray(tag), QDate(year,month,day));
                    if (!map->contains("year"))
                        map->insert("year", year);
                }
            } else if (G_VALUE_TYPE(&val) == GST_TYPE_FRACTION) {
                int nom = gst_value_get_fraction_numerator(&val);
                int denom = gst_value_get_fraction_denominator(&val);

                if (denom > 0) {
                    map->insert(QByteArray(tag), double(nom)/denom);
                }
            }
            break;
    }

    g_value_unset(&val);
}

/*!
  Convert GstTagList structure to QMap<QByteArray, QVariant>.

  Mapping to int, bool, char, string, fractions and date are supported.
  Fraction values are converted to doubles.
*/
QMap<QByteArray, QVariant> QGstUtils::gstTagListToMap(const GstTagList *tags)
{
    QMap<QByteArray, QVariant> res;
    gst_tag_list_foreach(tags, addTagToMap, &res);

    return res;
}

/*!
  Returns resolution of \a caps.
  If caps doesn't have a valid size, and ampty QSize is returned.
*/
QSize QGstUtils::capsResolution(const GstCaps *caps)
{
    QSize size;

    if (caps) {
        const GstStructure *structure = gst_caps_get_structure(caps, 0);
        gst_structure_get_int(structure, "width", &size.rwidth());
        gst_structure_get_int(structure, "height", &size.rheight());
    }

    return size;
}

/*!
  Returns aspect ratio corrected resolution of \a caps.
  If caps doesn't have a valid size, an empty QSize is returned.
*/
QSize QGstUtils::capsCorrectedResolution(const GstCaps *caps)
{
    QSize size;

    if (caps) {
        const GstStructure *structure = gst_caps_get_structure(caps, 0);
        gst_structure_get_int(structure, "width", &size.rwidth());
        gst_structure_get_int(structure, "height", &size.rheight());

        gint aspectNum = 0;
        gint aspectDenum = 0;
        if (!size.isEmpty() && gst_structure_get_fraction(
                    structure, "pixel-aspect-ratio", &aspectNum, &aspectDenum)) {
            if (aspectDenum > 0)
                size.setWidth(size.width()*aspectNum/aspectDenum);
        }
    }

    return size;
}

/*!
  Returns audio format for caps.
  If caps doesn't have a valid audio format, an empty QAudioFormat is returned.
*/

QAudioFormat QGstUtils::audioFormatForCaps(const GstCaps *caps)
{
    const GstStructure *structure = gst_caps_get_structure(caps, 0);

    QAudioFormat format;

    if (qstrcmp(gst_structure_get_name(structure), "audio/x-raw-int") == 0) {

        format.setCodec("audio/pcm");

        int endianness = 0;
        gst_structure_get_int(structure, "endianness", &endianness);
        if (endianness == 1234)
            format.setByteOrder(QAudioFormat::LittleEndian);
        else if (endianness == 4321)
            format.setByteOrder(QAudioFormat::BigEndian);

        gboolean isSigned = FALSE;
        gst_structure_get_boolean(structure, "signed", &isSigned);
        if (isSigned)
            format.setSampleType(QAudioFormat::SignedInt);
        else
            format.setSampleType(QAudioFormat::UnSignedInt);

        // Number of bits allocated per sample.
        int width = 0;
        gst_structure_get_int(structure, "width", &width);

        // The number of bits used per sample. This must be less than or equal to the width.
        int depth = 0;
        gst_structure_get_int(structure, "depth", &depth);

        if (width != depth) {
            // Unsupported sample layout.
            return QAudioFormat();
        }
        format.setSampleSize(width);

        int rate = 0;
        gst_structure_get_int(structure, "rate", &rate);
        format.setSampleRate(rate);

        int channels = 0;
        gst_structure_get_int(structure, "channels", &channels);
        format.setChannelCount(channels);

    } else if (qstrcmp(gst_structure_get_name(structure), "audio/x-raw-float") == 0) {

        format.setCodec("audio/pcm");

        int endianness = 0;
        gst_structure_get_int(structure, "endianness", &endianness);
        if (endianness == 1234)
            format.setByteOrder(QAudioFormat::LittleEndian);
        else if (endianness == 4321)
            format.setByteOrder(QAudioFormat::BigEndian);

        format.setSampleType(QAudioFormat::Float);

        int width = 0;
        gst_structure_get_int(structure, "width", &width);

        format.setSampleSize(width);

        int rate = 0;
        gst_structure_get_int(structure, "rate", &rate);
        format.setSampleRate(rate);

        int channels = 0;
        gst_structure_get_int(structure, "channels", &channels);
        format.setChannelCount(channels);

    } else {
        return QAudioFormat();
    }

    return format;
}


/*!
  Returns audio format for a buffer.
  If the buffer doesn't have a valid audio format, an empty QAudioFormat is returned.
*/

QAudioFormat QGstUtils::audioFormatForBuffer(GstBuffer *buffer)
{
    GstCaps* caps = gst_buffer_get_caps(buffer);
    if (!caps)
        return QAudioFormat();

    QAudioFormat format = QGstUtils::audioFormatForCaps(caps);
    gst_caps_unref(caps);
    return format;
}


/*!
  Builds GstCaps for an audio format.
  Returns 0 if the audio format is not valid.
  Caller must unref GstCaps.
*/

GstCaps *QGstUtils::capsForAudioFormat(QAudioFormat format)
{
    GstStructure *structure = 0;

    if (format.isValid()) {
        if (format.sampleType() == QAudioFormat::SignedInt || format.sampleType() == QAudioFormat::UnSignedInt) {
            structure = gst_structure_new("audio/x-raw-int", NULL);
        } else if (format.sampleType() == QAudioFormat::Float) {
            structure = gst_structure_new("audio/x-raw-float", NULL);
        }
    }

    GstCaps *caps = 0;

    if (structure) {
        gst_structure_set(structure, "rate", G_TYPE_INT, format.sampleRate(), NULL);
        gst_structure_set(structure, "channels", G_TYPE_INT, format.channelCount(), NULL);
        gst_structure_set(structure, "width", G_TYPE_INT, format.sampleSize(), NULL);
        gst_structure_set(structure, "depth", G_TYPE_INT, format.sampleSize(), NULL);

        if (format.byteOrder() == QAudioFormat::LittleEndian)
            gst_structure_set(structure, "endianness", G_TYPE_INT, 1234, NULL);
        else if (format.byteOrder() == QAudioFormat::BigEndian)
            gst_structure_set(structure, "endianness", G_TYPE_INT, 4321, NULL);

        if (format.sampleType() == QAudioFormat::SignedInt)
            gst_structure_set(structure, "signed", G_TYPE_BOOLEAN, TRUE, NULL);
        else if (format.sampleType() == QAudioFormat::UnSignedInt)
            gst_structure_set(structure, "signed", G_TYPE_BOOLEAN, FALSE, NULL);

        caps = gst_caps_new_empty();
        Q_ASSERT(caps);
        gst_caps_append_structure(caps, structure);
    }

    return caps;
}

void QGstUtils::initializeGst()
{
    static bool initialized = false;
    if (!initialized) {
        initialized = true;
        gst_init(NULL, NULL);
    }
}

namespace {
    const char* getCodecAlias(const QString &codec)
    {
        if (codec.startsWith("avc1."))
            return "video/x-h264";

        if (codec.startsWith("mp4a."))
            return "audio/mpeg4";

        if (codec.startsWith("mp4v.20."))
            return "video/mpeg4";

        if (codec == "samr")
            return "audio/amr";

        return 0;
    }

    const char* getMimeTypeAlias(const QString &mimeType)
    {
        if (mimeType == "video/mp4")
            return "video/mpeg4";

        if (mimeType == "audio/mp4")
            return "audio/mpeg4";

        if (mimeType == "video/ogg"
            || mimeType == "audio/ogg")
            return "application/ogg";

        return 0;
    }
}

QMultimedia::SupportEstimate QGstUtils::hasSupport(const QString &mimeType,
                                                    const QStringList &codecs,
                                                    const QSet<QString> &supportedMimeTypeSet)
{
    if (supportedMimeTypeSet.isEmpty())
        return QMultimedia::NotSupported;

    QString mimeTypeLowcase = mimeType.toLower();
    bool containsMimeType = supportedMimeTypeSet.contains(mimeTypeLowcase);
    if (!containsMimeType) {
        const char* mimeTypeAlias = getMimeTypeAlias(mimeTypeLowcase);
        containsMimeType = supportedMimeTypeSet.contains(mimeTypeAlias);
        if (!containsMimeType) {
            containsMimeType = supportedMimeTypeSet.contains("video/" + mimeTypeLowcase)
                               || supportedMimeTypeSet.contains("video/x-" + mimeTypeLowcase)
                               || supportedMimeTypeSet.contains("audio/" + mimeTypeLowcase)
                               || supportedMimeTypeSet.contains("audio/x-" + mimeTypeLowcase);
        }
    }

    int supportedCodecCount = 0;
    foreach (const QString &codec, codecs) {
        QString codecLowcase = codec.toLower();
        const char* codecAlias = getCodecAlias(codecLowcase);
        if (codecAlias) {
            if (supportedMimeTypeSet.contains(codecAlias))
                supportedCodecCount++;
        } else if (supportedMimeTypeSet.contains("video/" + codecLowcase)
                   || supportedMimeTypeSet.contains("video/x-" + codecLowcase)
                   || supportedMimeTypeSet.contains("audio/" + codecLowcase)
                   || supportedMimeTypeSet.contains("audio/x-" + codecLowcase)) {
            supportedCodecCount++;
        }
    }
    if (supportedCodecCount > 0 && supportedCodecCount == codecs.size())
        return QMultimedia::ProbablySupported;

    if (supportedCodecCount == 0 && !containsMimeType)
        return QMultimedia::NotSupported;

    return QMultimedia::MaybeSupported;
}

namespace {

typedef QHash<GstElementFactory *, QVector<QGstUtils::CameraInfo> > FactoryCameraInfoMap;

Q_GLOBAL_STATIC(FactoryCameraInfoMap, qt_camera_device_info);

}

QVector<QGstUtils::CameraInfo> QGstUtils::enumerateCameras(GstElementFactory *factory)
{
    FactoryCameraInfoMap::const_iterator it = qt_camera_device_info()->constFind(factory);
    if (it != qt_camera_device_info()->constEnd())
        return *it;

    QVector<CameraInfo> &devices = (*qt_camera_device_info())[factory];

    if (factory) {
        bool hasVideoSource = false;

        const GType type = gst_element_factory_get_element_type(factory);
        GObjectClass * const objectClass = type
                ? static_cast<GObjectClass *>(g_type_class_ref(type))
                : 0;
        if (objectClass) {
            if (g_object_class_find_property(objectClass, "camera-device")) {
                const CameraInfo primary = {
                    QStringLiteral("primary"),
                    QGstreamerVideoInputDeviceControl::primaryCamera(),
                    0,
                    QCamera::BackFace,
                    QByteArray()
                };
                const CameraInfo secondary = {
                    QStringLiteral("secondary"),
                    QGstreamerVideoInputDeviceControl::secondaryCamera(),
                    0,
                    QCamera::FrontFace,
                    QByteArray()
                };

                devices.append(primary);
                devices.append(secondary);

                GstElement *camera = g_object_class_find_property(objectClass, "sensor-mount-angle")
                        ? gst_element_factory_create(factory, 0)
                        : 0;
                if (camera) {
                    if (gst_element_set_state(camera, GST_STATE_READY) != GST_STATE_CHANGE_SUCCESS) {
                        // no-op
                    } else for (int i = 0; i < 2; ++i) {
                        gint orientation = 0;
                        g_object_set(G_OBJECT(camera), "camera-device", i, NULL);
                        g_object_get(G_OBJECT(camera), "sensor-mount-angle", &orientation, NULL);

                        devices[i].orientation = (720 - orientation) % 360;
                    }
                    gst_element_set_state(camera, GST_STATE_NULL);
                    gst_object_unref(GST_OBJECT(camera));

                }
            } else if (g_object_class_find_property(objectClass, "video-source")) {
                hasVideoSource = true;
            }

            g_type_class_unref(objectClass);
        }

        if (!devices.isEmpty() || !hasVideoSource) {
            return devices;
        }
    }

#ifdef USE_V4L
    QDir devDir(QStringLiteral("/dev"));
    devDir.setFilter(QDir::System);

    QFileInfoList entries = devDir.entryInfoList(QStringList()
                << QStringLiteral("video*"));

    foreach (const QFileInfo &entryInfo, entries) {
        //qDebug() << "Try" << entryInfo.filePath();

        int fd = qt_safe_open(entryInfo.filePath().toLatin1().constData(), O_RDWR );
        if (fd == -1)
            continue;

        bool isCamera = false;

        v4l2_input input;
        memset(&input, 0, sizeof(input));
        for (; ::ioctl(fd, VIDIOC_ENUMINPUT, &input) >= 0; ++input.index) {
            if (input.type == V4L2_INPUT_TYPE_CAMERA || input.type == 0) {
                isCamera = ::ioctl(fd, VIDIOC_S_INPUT, input.index) != 0;
                break;
            }
        }

        if (isCamera) {
            // find out its driver "name"
            QByteArray driver;
            QString name;
            struct v4l2_capability vcap;
            memset(&vcap, 0, sizeof(struct v4l2_capability));

            if (ioctl(fd, VIDIOC_QUERYCAP, &vcap) != 0) {
                name = entryInfo.fileName();
            } else {
                driver = QByteArray((const char*)vcap.driver);
                name = QString::fromUtf8((const char*)vcap.card);
                if (name.isEmpty())
                    name = entryInfo.fileName();
            }
            //qDebug() << "found camera: " << name;


            CameraInfo device = {
                entryInfo.absoluteFilePath(),
                name,
                0,
                QCamera::UnspecifiedPosition,
                driver
            };
            devices.append(device);
        }
        qt_safe_close(fd);
    }
#endif // USE_V4L

    return devices;
}

QList<QByteArray> QGstUtils::cameraDevices(GstElementFactory * factory)
{
    QList<QByteArray> devices;

    foreach (const CameraInfo &camera, enumerateCameras(factory))
        devices.append(camera.name.toUtf8());

    return devices;
}

QString QGstUtils::cameraDescription(const QString &device, GstElementFactory * factory)
{
    foreach (const CameraInfo &camera, enumerateCameras(factory)) {
        if (camera.name == device)
            return camera.description;
    }
    return QString();
}

QCamera::Position QGstUtils::cameraPosition(const QString &device, GstElementFactory * factory)
{
    foreach (const CameraInfo &camera, enumerateCameras(factory)) {
        if (camera.name == device)
            return camera.position;
    }
    return QCamera::UnspecifiedPosition;
}

int QGstUtils::cameraOrientation(const QString &device, GstElementFactory * factory)
{
    foreach (const CameraInfo &camera, enumerateCameras(factory)) {
        if (camera.name == device)
            return camera.orientation;
    }
    return 0;
}

QByteArray QGstUtils::cameraDriver(const QString &device, GstElementFactory *factory)
{
    foreach (const CameraInfo &camera, enumerateCameras(factory)) {
        if (camera.name == device)
            return camera.driver;
    }
    return QByteArray();
}


void qt_gst_object_ref_sink(gpointer object)
{
#if (GST_VERSION_MAJOR >= 0) && (GST_VERSION_MINOR >= 10) && (GST_VERSION_MICRO >= 24)
    gst_object_ref_sink(object);
#else
    g_return_if_fail (GST_IS_OBJECT(object));

    GST_OBJECT_LOCK(object);
    if (G_LIKELY(GST_OBJECT_IS_FLOATING(object))) {
        GST_OBJECT_FLAG_UNSET(object, GST_OBJECT_FLOATING);
        GST_OBJECT_UNLOCK(object);
    } else {
        GST_OBJECT_UNLOCK(object);
        gst_object_ref(object);
    }
#endif
}

QT_END_NAMESPACE
