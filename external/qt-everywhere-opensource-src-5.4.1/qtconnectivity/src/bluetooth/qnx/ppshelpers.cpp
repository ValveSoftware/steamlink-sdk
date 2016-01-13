/***************************************************************************
**
** Copyright (C) 2012 Research In Motion
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtBluetooth module of the Qt Toolkit.
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

#include "ppshelpers_p.h"
#include <QtCore/private/qcore_unix_p.h>
#include <QDebug>
#include "../qbluetoothserver_p.h"
#include <QTimer>

QT_BEGIN_NAMESPACE

static int count = 0;

static int ppsCtrlFD = -1;

static QSocketNotifier *ppsCtrlNotifier = 0;

static const char btControlFDPath[] = "/pps/services/bluetooth/public/control";
static const char btSettingsFDPath[] = "/pps/services/bluetooth/settings";
static const char btRemoteDevFDPath[] = "/pps/services/bluetooth/remote_devices/";
static const char btMediaFDPath[] = "/pps/services/bluetooth/media/control";

static const int ppsBufferSize = 1024;

static int ctrlId = 20;

static QList<QPair<int, QObject*> > waitingCtrlMsgs;

static BBSocketNotifier bbSocketNotifier;

QHash<QBluetoothServerPrivate*, int> __fakeServerPorts;

QList<QPair<QString, QObject*> > evtRegistration;

void BBSocketNotifier::distribute()
{
    ppsDecodeControlResponse();
}

void BBSocketNotifier::closeControlFD()
{
    if (count <= 0) {
        delete ppsCtrlNotifier;
        qt_safe_close(ppsCtrlFD);
        ppsCtrlFD = -1;
        ppsCtrlNotifier = 0;
    }
}

QPair<int, QObject*> takeObjectInWList(int id)
{
    for (int i=0; i<waitingCtrlMsgs.size(); i++) {
        if (waitingCtrlMsgs.at(i).first == id)
            return waitingCtrlMsgs.takeAt(i);
    }
    return QPair<int, QObject*>(-1,0);
}

void ppsRegisterControl()
{
    count++;
    if (count == 1) {
        if (ppsCtrlFD != -1) {
            qCDebug(QT_BT_QNX) << "PPS control FD not properly deinitialized";
            return;
        }
        ppsCtrlFD = qt_safe_open(btControlFDPath, O_RDWR | O_SYNC);
        if (ppsCtrlFD == -1) {
            qCWarning(QT_BT_QNX) << Q_FUNC_INFO << "ppsCtrlFD - failed to qt_safe_open" << btControlFDPath;
        } else {
            ppsCtrlNotifier = new QSocketNotifier(ppsCtrlFD, QSocketNotifier::Read);
            QObject::connect(ppsCtrlNotifier, SIGNAL(activated(int)), &bbSocketNotifier, SLOT(distribute()));
        }
    }
}

void ppsUnregisterControl(QObject *obj)
{
    count--;
    if (count == 0) {
        //QMetaObject::invokeMethod(&bbSocketNotifier, "closeControlFD",Qt::QueuedConnection);
        //We need to postpone the closing of the file descriptor, otherwise the last message
        //might not have gone through...a queued connection is not enough here
        QTimer::singleShot(5, &bbSocketNotifier, SLOT(closeControlFD()));
    }
    for (int i = waitingCtrlMsgs.size()-1; i >= 0 ; i--) {
        if (waitingCtrlMsgs.at(i).second == obj)
            waitingCtrlMsgs.removeAt(i);
    }
}

pps_encoder_t *beginCtrlMessage(const char *msg, QObject *sender)
{
    pps_encoder_t *encoder= new pps_encoder_t;
    pps_encoder_initialize(encoder, 0);
    pps_encoder_add_string(encoder, "msg", msg);
    ctrlId++;
    pps_encoder_add_string(encoder, "id", QString::number(ctrlId).toStdString().c_str() );
    waitingCtrlMsgs.append(QPair<int, QObject*>(ctrlId, sender));
    return encoder;
}

bool endCtrlMessage(pps_encoder_t *encoder)
{
    qCDebug(QT_BT_QNX) << "writing" << pps_encoder_buffer(encoder);
    if (pps_encoder_buffer(encoder) != 0) {
        int res = qt_safe_write(ppsCtrlFD, pps_encoder_buffer(encoder), pps_encoder_length(encoder));
        if (res == -1) {
            qCWarning(QT_BT_QNX) << Q_FUNC_INFO << "Error when writing to control FD. Is Bluetooth powerd on?"
                       << errno << ppsCtrlFD << count;
            return false;
        }
    }

    pps_encoder_cleanup( encoder );
    return true;
}

bool ppsSendControlMessage(const char *msg, int service, const QBluetoothUuid &uuid, const QString &address, const QString &serviceName, QObject *sender, const int &subtype)
{
    pps_encoder_t *encoder = beginCtrlMessage(msg, sender);
    pps_encoder_start_object(encoder, "dat");
    pps_encoder_add_int(encoder, "service", service);
    if (subtype != -1)
        pps_encoder_add_int(encoder, "subtype", subtype);

    pps_encoder_add_string(encoder, "uuid", uuid.toString().mid(1,36).toUtf8().constData());

    if (!serviceName.isEmpty()) {
        pps_encoder_add_string(encoder, "name", serviceName.toUtf8().constData());
    }
    if (!address.isEmpty()) {
        pps_encoder_add_string(encoder, "addr", address.toUtf8().constData());
    }

    pps_encoder_error_t rese = pps_encoder_end_object(encoder);

    if (rese != PPS_ENCODER_OK) {
        errno = EPERM;
        return false;
    }

    return endCtrlMessage(encoder);
}

bool ppsSendControlMessage(const char *msg, const QString &dat, QObject *sender)
{
    pps_encoder_t *encoder = beginCtrlMessage(msg, sender);
    pps_encoder_add_json(encoder, "dat", dat.toUtf8().constData());
    return endCtrlMessage(encoder);
}

bool ppsSendControlMessage(const char *msg, QObject *sender)
{
    pps_encoder_t *encoder = beginCtrlMessage(msg, sender);
    return endCtrlMessage(encoder);
}

int __newHostMode = -1;

void ppsDecodeControlResponse()
{
    ppsResult result;
    ResultType resType = UNKNOWN;

    if (ppsCtrlFD != -1) {
        char buf[ppsBufferSize];
        qt_safe_read(ppsCtrlFD, &buf, sizeof(buf) );
        if (buf[0] != '@')
            return;
        qCDebug(QT_BT_QNX) << "CTRL Response" << buf;

        pps_decoder_t ppsDecoder;
        pps_decoder_initialize(&ppsDecoder, 0);
        if (pps_decoder_parse_pps_str(&ppsDecoder, buf) == PPS_DECODER_OK) {
            pps_decoder_push(&ppsDecoder, 0);
            const char *buf;

            //The pps response can either be of type 'res', 'msg' or 'evt'
            if (pps_decoder_get_string(&ppsDecoder, "res", &buf) == PPS_DECODER_OK) {
                result.msg = QString::fromUtf8(buf);
                resType = RESPONSE;
            } else if (pps_decoder_get_string(&ppsDecoder, "msg", &buf) == PPS_DECODER_OK) {
                result.msg = QString::fromUtf8(buf);
                resType = MESSAGE;
            } else if (pps_decoder_get_string(&ppsDecoder, "evt", &buf) == PPS_DECODER_OK) {
                result.msg = QString::fromUtf8(buf);
                resType = EVENT;
            }

            if (pps_decoder_get_string(&ppsDecoder, "id",  &buf) == PPS_DECODER_OK)
                result.id = QString::fromUtf8(buf).toInt();

            //read out the error message if there is one
            if (pps_decoder_get_string(&ppsDecoder, "errstr", &buf) == PPS_DECODER_OK)
                result.errorMsg = QString::fromUtf8(buf);

            int dat;
            if (pps_decoder_get_int(&ppsDecoder, "err", &dat) == PPS_DECODER_OK) {
                result.error = dat;
            }

            //The dat object can be either a string or a array
            pps_node_type_t nodeType = pps_decoder_type(&ppsDecoder,"dat");
            if (nodeType == PPS_TYPE_STRING) {
                pps_decoder_get_string(&ppsDecoder,"dat",&buf);
                result.dat << QString::fromUtf8(buf);
            } else if (nodeType == PPS_TYPE_OBJECT || nodeType == PPS_TYPE_ARRAY) {
                pps_decoder_push(&ppsDecoder,"dat");
                pps_decoder_goto_index(&ppsDecoder, 0);
                int len = pps_decoder_length(&ppsDecoder);

                for (int i = 0; i < len; ++i) {
                    switch ( pps_decoder_type(&ppsDecoder, 0)) {
                    case PPS_TYPE_STRING:
                         result.dat << QString::fromUtf8(pps_decoder_name(&ppsDecoder));
                         pps_decoder_get_string(&ppsDecoder, 0, &buf);
                         result.dat << QString::fromUtf8(buf);
                         break;
                     case PPS_TYPE_NUMBER:
                         result.dat << QString::fromUtf8(pps_decoder_name(&ppsDecoder));
                         double dvalue;
                         pps_decoder_get_double(&ppsDecoder, 0, &dvalue);
                         result.dat << QString::number(dvalue);
                         break;
                     default:
                         pps_decoder_next(&ppsDecoder);
                     }
                 }
            } else {
                qCDebug(QT_BT_QNX) << "Control Response: No node type" << result.msg;
            }
        }
        pps_decoder_cleanup(&ppsDecoder);
    }

    if (result.msg == QStringLiteral("radio_init")) {
        qCDebug(QT_BT_QNX) << "Radio initialized";
    } else if (result.msg == QStringLiteral("access_changed") && __newHostMode != -1) {
        qCDebug(QT_BT_QNX) << "Access changed after radio init";
        ppsSendControlMessage("set_access", QStringLiteral("{\"access\":%1}").arg(__newHostMode), 0);
        __newHostMode = -1;
    }

    if (resType == RESPONSE) {
        QPair<int, QObject*> wMessage = takeObjectInWList(result.id);
        if (wMessage.second != 0)
            wMessage.second->metaObject()->invokeMethod(wMessage.second, "controlReply", Q_ARG(ppsResult, result));
    } else if (resType == EVENT) {
        //qCDebug(QT_BT_QNX) << "Distributing event" << result.msg;
        for (int i=0; i < evtRegistration.size(); i++) {
            if (result.msg == evtRegistration.at(i).first)
                evtRegistration.at(i).second->metaObject()->invokeMethod(evtRegistration.at(i).second, "controlEvent", Q_ARG(ppsResult, result));
        }
    }
}

void ppsSendOpp(const char *msg, const QByteArray &url, const QBluetoothAddress &address, QObject *sender)
{
    pps_encoder_t *encoder = beginCtrlMessage(msg, sender);

    pps_encoder_start_object(encoder, "dat");
    pps_encoder_add_string(encoder, "address", address.toString().toUtf8().constData());
    pps_encoder_start_array(encoder, "urls");
    pps_encoder_add_string(encoder, 0, url.constData());
    pps_encoder_end_array(encoder);
    pps_encoder_end_object(encoder);

    endCtrlMessage(encoder);
}

QVariant ppsReadSetting(const char *property)
{
    int settingsFD;
    char buf[ppsBufferSize];
    if ((settingsFD = qt_safe_open(btSettingsFDPath, O_RDONLY)) == -1) {
        qCWarning(QT_BT_QNX) << Q_FUNC_INFO << "failed to open "<< btSettingsFDPath;
        return QVariant();
    }

    QVariant result;

    qt_safe_read( settingsFD, &buf, sizeof(buf));
    pps_decoder_t decoder;
    pps_decoder_initialize(&decoder, 0);

    if (pps_decoder_parse_pps_str(&decoder, buf) == PPS_DECODER_OK) {
        pps_decoder_push(&decoder, 0);
        pps_node_type_t nodeType = pps_decoder_type(&decoder, property);
        if (nodeType == PPS_TYPE_STRING) {
            const char *dat;
            if (pps_decoder_get_string(&decoder, property, &dat) == PPS_DECODER_OK) {
                result = QString::fromUtf8(dat);
                qCDebug(QT_BT_QNX) << "Read setting" << result;
            } else {
                qCWarning(QT_BT_QNX) << Q_FUNC_INFO << "could not read"<< property;
                return QVariant();
            }
        } else if (nodeType == PPS_TYPE_BOOL) {
            bool dat;
            if (pps_decoder_get_bool(&decoder, property, &dat) == PPS_DECODER_OK) {
                result = dat;
                qCDebug(QT_BT_QNX) << "Read setting" << result;
            } else {
                qCWarning(QT_BT_QNX) << Q_FUNC_INFO << "could not read"<< property;
                return QVariant();
            }
        } else if (nodeType == PPS_TYPE_NUMBER) {
            int dat;
            if (pps_decoder_get_int(&decoder, property, &dat) == PPS_DECODER_OK) {
                result = dat;
                qCDebug(QT_BT_QNX) << "Read setting" << result;
            } else {
                qCWarning(QT_BT_QNX) << Q_FUNC_INFO << "could not read"<< property;
                return QVariant();
            }
        } else {
            qCDebug(QT_BT_QNX) << Q_FUNC_INFO << "unrecognized entry for settings";
        }
    }
    pps_decoder_cleanup(&decoder);
    qt_safe_close(settingsFD);
    return result;
}

QVariant ppsRemoteDeviceStatus(const QByteArray &address, const char *property)
{
    int rmFD;
    char buf[ppsBufferSize];
    QByteArray filename = btRemoteDevFDPath;
    filename.append(address);

    if ((rmFD = qt_safe_open(filename.constData(), O_RDONLY)) < 0) {
        qCWarning(QT_BT_QNX) << Q_FUNC_INFO << "failed to open "<< btRemoteDevFDPath << address;
        return false;
    }

    QVariant res;

    qt_safe_read(rmFD, &buf, sizeof(buf));
    pps_decoder_t ppsDecoder;
    pps_decoder_initialize(&ppsDecoder, 0);
    if (pps_decoder_parse_pps_str(&ppsDecoder, buf) == PPS_DECODER_OK) {
        pps_decoder_push(&ppsDecoder, 0);

        //Find out about the node type
        pps_node_type_t nodeType = pps_decoder_type(&ppsDecoder, property);
        if (nodeType == PPS_TYPE_STRING) {
            const char *dat;
            pps_decoder_get_string(&ppsDecoder,property,&dat);
            res = QString::fromUtf8(dat);
        } else if (nodeType == PPS_TYPE_BOOL) {
            bool dat;
            pps_decoder_get_bool(&ppsDecoder,property,&dat);
            res = QVariant(dat);
        } else {
            qCDebug(QT_BT_QNX) << "RDStatus: No node type" << property;
        }
    }
    pps_decoder_cleanup(&ppsDecoder);
    qt_safe_close(rmFD);
    return res;
}

bool ppsReadRemoteDevice(int fd, pps_decoder_t *decoder, QBluetoothAddress *btAddr, QString *deviceName)
{
    char buf[ppsBufferSize * 2];
    char addr_buf[18];

    addr_buf[17] = '\0';

    if (qt_safe_read(fd, &buf, sizeof(buf)) == -1) {
        qCWarning(QT_BT_QNX) << Q_FUNC_INFO << "Could not qt_safe_read from pps remote device file";
        return false;
    }

    qCDebug(QT_BT_QNX) << "Remote device" << buf;

    //the address of the BT device is stored at the beginning of the qt_safe_read
    if (buf[0] != '-') {
        memcpy(&addr_buf, &buf[1], 17);
    } else { //The device was removed
        memcpy(&addr_buf, &buf[2], 17);
        return false;
    }

    *btAddr = QBluetoothAddress(QString::fromUtf8(addr_buf));

    if (pps_decoder_parse_pps_str(decoder, buf) == PPS_DECODER_OK) {
        const char* name;
        pps_decoder_push(decoder, 0);

        if (pps_decoder_get_string(decoder, "name", &name) == PPS_DECODER_OK)
            (*deviceName) = QString::fromUtf8(name);

        return true;
    }
    return false;
}

void ppsRegisterForEvent(const QString &evt, QObject *obj)
{
    //If the event was already registered, we don't register it again
    for (int i = 0; i < evtRegistration.size(); i++) {
        if (evtRegistration.at(i).first == evt && evtRegistration.at(i).second == obj )
            return;
    }
    evtRegistration.append(QPair<QString, QObject*>(evt,obj));
}

void ppsUnregisterForEvent(const QString &str, QObject *obj)
{
    for (int i=evtRegistration.size()-1; i >= 0; --i) {
        if (evtRegistration.at(i).first == str && evtRegistration.at(i).second == obj)
            evtRegistration.removeAt(i);
    }
}

QT_END_NAMESPACE
