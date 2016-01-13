/****************************************************************************
**
** Copyright (C) 2012 - 2013 BlackBerry Limited. All rights reserved.
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtPositioning module of the Qt Toolkit.
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

// Indicate which dependencies to mock when testing
// NOTE: These defines must occur before including any headers and
//       ProxyInjector.hpp must be the first header included
#define USE_QT_QSOCKETNOTIFIER_PROXY
#define USE_UNISTD_PROXY
#define USE_FCNTL_PROXY
#define USE_ERRNO_PROXY
#define USE_STRING_PROXY
//#include <private/proxy/ProxyInjector.hpp>

// Disable symbol renaming when testing for open, close, read, and
// write as these dependency names conflict with member method names
// and we don't want to rename the member methods. Instead, use dummy
// names for the dependencies in the production code and toggle
// between the real/proxy versions here.
#ifdef BB_TEST_BUILD
#   undef open
#   undef close
#   undef read
#   undef write
#   define PosixOpen    open_proxy
#   define PosixClose   close_proxy
#   define PosixRead    read_proxy
#   define PosixWrite   write_proxy
#else
#   define PosixOpen    ::open
#   define PosixClose   ::close
#   define PosixRead    ::read
#   define PosixWrite   ::write
#endif

#include "ppsobject.h"

#include "ppsattribute_p.h"
#include "safeassign.h"

#include <bb/PpsAttribute>

#include <QDebug>
#include <QObject>
#include <QSocketNotifier>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#include <exception>

extern "C" {
#include "sys/pps.h"
}

namespace bb
{

/**
 * From "QNX Persistent Publish/Subscribe Developer's Guide":
 *
 * "the maximum size for a PPS object is 64 kilobytes"
 */
static const int PPS_MAX_SIZE = 64 * 1024;

class PpsDecoder
{
public:
    PpsDecoder(char *str)
    {
        throwOnPpsDecoderError( pps_decoder_initialize(&_decoder, str) );
    }

    ~PpsDecoder()
    {
        pps_decoder_cleanup(&_decoder);
    }

    QMap<QString, PpsAttribute> decode()
    {
        return decodeObject();
    }

    static QVariantMap variantMapFromPpsAttributeMap(const QMap<QString, PpsAttribute> &data)
    {
        QVariantMap variantMap;

        for ( QMap<QString, PpsAttribute>::const_iterator it = data.begin(); it != data.end(); it++ ) {
            variantMap[it.key()] = variantFromPpsAttribute( it.value() );
        }

        return variantMap;
    }

private:
    pps_decoder_t _decoder;

    static void throwOnPpsDecoderError(pps_decoder_error_t error)
    {
        switch ( error ) {
        case PPS_DECODER_OK:
            return;
        default:
            throw std::exception();
        }
    }

    bb::PpsAttributeFlag::Types readFlags()
    {
        int rawFlags = pps_decoder_flags(&_decoder, NULL);

        bb::PpsAttributeFlag::Types attributeFlags;

        if ( rawFlags & PPS_INCOMPLETE ) {
            attributeFlags |= bb::PpsAttributeFlag::Incomplete;
        }
        if ( rawFlags & PPS_DELETED ) {
            attributeFlags |= bb::PpsAttributeFlag::Deleted;
        }
        if ( rawFlags & PPS_CREATED ) {
            attributeFlags |= bb::PpsAttributeFlag::Created;
        }
        if ( rawFlags & PPS_TRUNCATED ) {
            attributeFlags |= bb::PpsAttributeFlag::Truncated;
        }
        if ( rawFlags & PPS_PURGED ) {
            attributeFlags |= bb::PpsAttributeFlag::Purged;
        }

        return attributeFlags;
    }

    bb::PpsAttribute decodeString()
    {
        const char * value = NULL;
        throwOnPpsDecoderError( pps_decoder_get_string(&_decoder, NULL, &value) );
        bb::PpsAttributeFlag::Types flags = readFlags();
        return bb::PpsAttributePrivate::ppsAttribute( QString::fromUtf8(value), flags );
    }

    bb::PpsAttribute decodeNumber()
    {
        // In order to support more number types, we have to do something stupid because
        // the PPS library won't let us work any other way
        // Basically, we have to probe the encoded type in order to try to get exactly what
        // we want
        long long llValue;
        double dValue;
        int iValue;
        bb::PpsAttributeFlag::Types flags;

        if (pps_decoder_is_integer( &_decoder, NULL )) {
            int result = pps_decoder_get_int(&_decoder, NULL, &iValue );
            switch (result) {
            case PPS_DECODER_OK:
                flags = readFlags();
                return bb::PpsAttributePrivate::ppsAttribute( iValue, flags );
            case PPS_DECODER_CONVERSION_FAILED:
                throwOnPpsDecoderError( pps_decoder_get_int64(&_decoder, NULL, &llValue) );
                flags = readFlags();
                return bb::PpsAttributePrivate::ppsAttribute( llValue, flags );
            default:
                throw std::exception();
            }
        } else {
            throwOnPpsDecoderError( pps_decoder_get_double(&_decoder, NULL, &dValue));
            flags = readFlags();
            return bb::PpsAttributePrivate::ppsAttribute( dValue, flags );
        }
    }

    bb::PpsAttribute decodeBool()
    {
        bool value;
        throwOnPpsDecoderError( pps_decoder_get_bool(&_decoder, NULL, &value) );
        bb::PpsAttributeFlag::Types flags = readFlags();
        return bb::PpsAttributePrivate::ppsAttribute( value, flags );
    }

    bb::PpsAttribute decodeData()
    {
        pps_node_type_t nodeType = pps_decoder_type(&_decoder, NULL);
        switch ( nodeType ) {
        case PPS_TYPE_BOOL:
            return decodeBool();
        case PPS_TYPE_NUMBER:
            return decodeNumber();
        case PPS_TYPE_STRING:
            return decodeString();
        case PPS_TYPE_ARRAY: {
            // We must read the flags before we push into the array, otherwise we'll get the flags for the first element in the array.
            bb::PpsAttributeFlag::Types flags = readFlags();
            throwOnPpsDecoderError( pps_decoder_push(&_decoder, NULL) );
            bb::PpsAttribute returnVal = bb::PpsAttributePrivate::ppsAttribute( decodeArray(), flags );
            throwOnPpsDecoderError( pps_decoder_pop(&_decoder) );
            return returnVal;
        }
        case PPS_TYPE_OBJECT: {
            // We must read the flags before we push into the object, otherwise we'll get the flags for the first alement in the object.
            bb::PpsAttributeFlag::Types flags = readFlags();
            throwOnPpsDecoderError( pps_decoder_push(&_decoder, NULL) );
            bb::PpsAttribute returnVal = bb::PpsAttributePrivate::ppsAttribute( decodeObject(), flags );
            throwOnPpsDecoderError( pps_decoder_pop(&_decoder) );
            return returnVal;
        }
        case PPS_TYPE_NULL:
        case PPS_TYPE_NONE:
        case PPS_TYPE_UNKNOWN:
        case PPS_TYPE_DELETED:
        default:
            return bb::PpsAttribute();
        }
    }

    QList<bb::PpsAttribute> decodeArray()
    {
        QList<bb::PpsAttribute> list;

        int length = pps_decoder_length( &_decoder );
        for ( int i = 0; i < length; i += 1 ) {
            // Force movement to a specific index.
            throwOnPpsDecoderError( pps_decoder_goto_index(&_decoder, i) );
            list << decodeData();
        }

        return list;
    }

    QMap<QString, bb::PpsAttribute> decodeObject()
    {
        QMap<QString, bb::PpsAttribute> map;

        int length = pps_decoder_length(&_decoder);
        for ( int i = 0; i < length; i += 1 ) {
            // Force movement to a specific index.
            throwOnPpsDecoderError( pps_decoder_goto_index(&_decoder, i) );
            QString name = QString::fromUtf8( pps_decoder_name(&_decoder) );
            map[name] = decodeData();
        }

        return map;
    }

    static QVariant variantFromPpsAttribute(const PpsAttribute &attribute)
    {
        switch ( attribute.type() ) {
            case PpsAttribute::Number:
                switch (attribute.toVariant().type()) {
                case QVariant::Int:
                    return attribute.toInt();
                    break;
                case QVariant::LongLong:
                    return attribute.toLongLong();
                    break;
                default:
                    return attribute.toDouble();
                    break;
                }
                break;
            case PpsAttribute::Bool:
                return attribute.toBool();
                break;
            case PpsAttribute::String:
                return attribute.toString();
                break;
            case PpsAttribute::Array: {
                QVariantList variantList;
                Q_FOREACH ( PpsAttribute attr, attribute.toList() ) {
                    variantList << variantFromPpsAttribute(attr);
                }
                return variantList;
            }
            case PpsAttribute::Object: {
                return variantMapFromPpsAttributeMap( attribute.toMap() );
            }
            case PpsAttribute::None:
            default:
                return QVariant();
        }
    }
};

///////////////////////////////////////////////////////////////////////////////

class PpsEncoder
{
public:
    PpsEncoder()
    {
        pps_encoder_initialize(&_encoder, false);
    }

    ~PpsEncoder()
    {
        pps_encoder_cleanup(&_encoder);
    }

    QByteArray encode(const QVariantMap &ppsData)
    {
        encodeObject(ppsData);
        const char *rawData = pps_encoder_buffer(&_encoder);
        if (!rawData) {
            throw std::exception();
        }
        return QByteArray(rawData);
    }

private:
    pps_encoder_t _encoder;

    static void throwOnPpsEncoderError(pps_encoder_error_t error)
    {
        switch ( error ) {
        case PPS_ENCODER_OK:
            return;
        default:
            throw std::exception();
        }
    }

    void encodeData(const char *name, QVariant data)
    {
        switch ( data.type() ) {
        case QVariant::Bool:
            throwOnPpsEncoderError( pps_encoder_add_bool(&_encoder, name, data.toBool()) );
            break;
        // We want to support encoding uint even though libpps doesn't support it directly.  We can't encoding uint as an int
        // since that will lose precision (e.g. 2^31+1 can't be encoded that way).  However, we can convert uint to double
        // without losing precision.  QVariant.toDouble() conveniently takes care of the conversion for us.
        case QVariant::UInt:
        case QVariant::Double:
            throwOnPpsEncoderError( pps_encoder_add_double(&_encoder, name, data.toDouble()) );
            break;
        case QVariant::Int:
            throwOnPpsEncoderError( pps_encoder_add_int(&_encoder, name, data.toInt()) );
            break;
        case QVariant::LongLong:
            throwOnPpsEncoderError( pps_encoder_add_int64(&_encoder, name, data.toLongLong()) );
            break;
        case QVariant::String:
            throwOnPpsEncoderError( pps_encoder_add_string(&_encoder, name, data.toString().toUtf8().constData()) );
            break;
        case QVariant::List: {
            throwOnPpsEncoderError( pps_encoder_start_array(&_encoder, name) );
            encodeArray( data.toList() );
            throwOnPpsEncoderError( pps_encoder_end_array(&_encoder) );
            break;
        }
        case QVariant::Map: {
            throwOnPpsEncoderError( pps_encoder_start_object(&_encoder, name) );
            encodeObject( data.toMap() );
            throwOnPpsEncoderError( pps_encoder_end_object(&_encoder) );
            break;
        }
        case QVariant::Invalid: {
            throwOnPpsEncoderError( pps_encoder_add_null(&_encoder, name) );
            break;
        }
        default:
            throw std::exception();
        }
    }

    void encodeArray(QVariantList data)
    {
        for ( QVariantList::const_iterator it = data.constBegin(); it != data.constEnd(); it ++ ) {
            encodeData( NULL, *it );
        }
    }

    void encodeObject(QVariantMap data) {
        for ( QVariantMap::const_iterator it = data.constBegin(); it != data.constEnd(); it ++ ) {
            encodeData( it.key().toUtf8().constData(), it.value() );
        }
    }
};

///////////////////////////////////////////////////////////////////////////////

class PpsObjectPrivate
{
public:
    QSocketNotifier *_notifier;
    QString _path;
    mutable int _error;
    int _fd;
    bool _readyReadEnabled;

    explicit PpsObjectPrivate(const QString &path) :
        _notifier(NULL),
        _path(path),
        _error(EOK),
        _fd(-1),
        _readyReadEnabled(true)
    {
    }
};

///////////////////////////////////////////////////////////////////////////////

PpsObject::PpsObject(const QString &path, QObject *parent) :
    QObject(parent),
    d_ptr(new PpsObjectPrivate(path))
{
}

PpsObject::~PpsObject()
{
    // RAII - ensure file gets closed
    if (isOpen()) {
        close();
    }
}

int PpsObject::error() const
{
    Q_D(const PpsObject);
    return d->_error;
}

QString PpsObject::errorString() const
{
    Q_D(const PpsObject);
    return QString(strerror(d->_error));
}

bool PpsObject::isReadyReadEnabled() const
{
    Q_D(const PpsObject);

    // query state of read ready signal
    return d->_readyReadEnabled;
}

void PpsObject::setReadyReadEnabled(bool enable)
{
    Q_D(PpsObject);

    // toggle whether socket notifier will emit a signal on read ready
    d->_readyReadEnabled = enable;
    if (isOpen()) {
        d->_notifier->setEnabled(enable);
    }
}

bool PpsObject::isBlocking() const
{
    Q_D(const PpsObject);

    // reset last error
    d->_error = EOK;

    // abort if file not open
    if (!isOpen()) {
        d->_error = EBADF;
        return false;
    }

    // query file status flags
    int flags = fcntl(d->_fd, F_GETFL);
    if (flags != -1) {
        // check if non-blocking flag is unset
        return ((flags & O_NONBLOCK) != O_NONBLOCK);
    } else {
        d->_error = errno;
        return false;
    }
}

bool PpsObject::setBlocking(bool enable)
{
    Q_D(PpsObject);

    // reset last error
    d->_error = EOK;

    // abort if file not open
    if (!isOpen()) {
        d->_error = EBADF;
        return false;
    }

    // query file status flags
    int flags = fcntl(d->_fd, F_GETFL);
    if (flags == -1) {
        d->_error = errno;
        return false;
    }

    // configure non-blocking flag
    if (enable) {
        flags &= ~O_NONBLOCK;
    } else {
        flags |= O_NONBLOCK;
    }

    // update file status flags
    flags = fcntl(d->_fd, F_SETFL, flags);
    if (flags == -1) {
        d->_error = errno;
        return false;
    }

    return true;
}

bool PpsObject::isOpen() const
{
    Q_D(const PpsObject);
    return (d->_fd != -1);
}

bool PpsObject::open(PpsOpenMode::Types mode)
{
    Q_D(PpsObject);

    // reset last error
    d->_error = EOK;

    // abort if file already open
    if (isOpen()) {
        d->_error = EBUSY;
        return false;
    }

    // convert pps flags to open flags
    int oflags = 0;
    if ((mode & PpsOpenMode::Publish) && (mode & PpsOpenMode::Subscribe)) {
        oflags |= O_RDWR;
    } else if (mode & PpsOpenMode::Publish) {
        oflags |= O_WRONLY;
    } else if (mode & PpsOpenMode::Subscribe) {
        oflags |= O_RDONLY;
    }

    if (mode & PpsOpenMode::Create) {
        oflags |= O_CREAT | O_EXCL;
    }

    if (mode & PpsOpenMode::DeleteContents) {
        oflags |= O_TRUNC;
    }

    // open pps file
    d->_fd = PosixOpen(d->_path.toUtf8().data(), oflags, 0666);

    // wire up socket notifier to know when reads are ready
    if (d->_fd != -1) {
        d->_notifier = new QSocketNotifier(d->_fd, QSocketNotifier::Read, this);
        d->_notifier->setEnabled(d->_readyReadEnabled);
        QObject::connect(d->_notifier, SIGNAL(activated(int)), this, SIGNAL(readyRead()));
        return true;
    } else {
        d->_error = errno;
        return false;
    }
}

bool PpsObject::close()
{
    Q_D(PpsObject);

    // reset last error
    d->_error = EOK;

    // abort if file not open
    if (!isOpen()) {
        d->_error = EBADF;
        return false;
    }

    // shutdown socket notifier
    delete d->_notifier;
    d->_notifier = NULL;

    // close pps file
    int result = PosixClose(d->_fd);
    d->_fd = -1;

    // check success of operation
    if (result == 0) {
        return true;
    } else {
        d->_error = errno;
        return false;
    }
}

QByteArray PpsObject::read(bool * ok)
{
    Q_D(PpsObject);

    // reset last error
    d->_error = EOK;

    // abort if file not open
    if (!isOpen()) {
        d->_error = EBADF;
        safeAssign(ok, false);
        return QByteArray();
    }

    QByteArray byteArray;
    byteArray.resize(PPS_MAX_SIZE); // resize doesn't initialize the data
    int result = PosixRead(d->_fd, byteArray.data(), byteArray.size());

    // check result of read operation
    if (result > 0) {
        // resize byte array to amount actually read
        byteArray.resize(result);
        safeAssign(ok, true);
        return byteArray;
    } else if (result == 0) {
        // normalize the behavior of read() when no data is ready so a pps object
        // put in non-blocking mode via opening w/o ?wait (read returns 0) looks
        // the same as a pps object put in non-blocking mode by setting O_NONBLOCK
        // (read returns EAGAIN)
        d->_error = EAGAIN;
        safeAssign(ok, false);
        return QByteArray(); // Specifically return a default-constructed QByteArray.
    } else {
        d->_error = errno;
        qWarning() << "PpsObject::read failed to read pps data, error " << errorString();
        safeAssign(ok, false);
        return QByteArray(); // Specifically return a default-constructed QByteArray.
    }
}

bool PpsObject::write(const QByteArray &byteArray)
{
    Q_D(PpsObject);

    // reset last error
    d->_error = EOK;

    // abort if file not open
    if (!isOpen()) {
        d->_error = EBADF;
        return false;
    }

    // write entire byte array to pps file
    int result = PosixWrite(d->_fd, byteArray.data(), byteArray.size());
    if (result == -1) {
        d->_error = errno;
    }
    return (result == byteArray.size());
}

bool PpsObject::remove()
{
    Q_D(PpsObject);

    // reset last error
    d->_error = EOK;

    // delete pps file
    int result = unlink(d->_path.toUtf8().data());

    // check success of operation
    if (result == 0) {
        return true;
    } else {
        d->_error = errno;
        return false;
    }
}

QVariantMap PpsObject::decode(const QByteArray &rawData, bool *ok)
{
    QMap<QString, PpsAttribute> mapData = decodeWithFlags(rawData, ok);

    // If *ok is false, then mapData is empty, so the resulting QVariantMap will also be empty, as desired.
    return PpsDecoder::variantMapFromPpsAttributeMap(mapData);
}

QMap<QString, PpsAttribute> PpsObject::decodeWithFlags(const QByteArray &rawData, bool *ok)
{
    bb::safeAssign(ok, true);

    try {
        QByteArray mutableData(rawData);
        PpsDecoder decoder(mutableData.data());
        return decoder.decode();
    } catch (...) {
        bb::safeAssign(ok, false);
        return QMap<QString, bb::PpsAttribute>();
    }
}

QByteArray PpsObject::encode(const QVariantMap &ppsData, bool *ok)
{
    bb::safeAssign( ok, true );

    try {
        PpsEncoder encoder;
        return encoder.encode(ppsData);
    } catch (...) {
        bb::safeAssign(ok, false);
        return QByteArray();
    }
}

} // namespace bb
