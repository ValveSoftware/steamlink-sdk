/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
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

#include <QtCore/QByteArray>
#include <QtCore/QDebug>
#include <stdio.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/sdp.h>
#include <bluetooth/sdp_lib.h>

#define RETURN_SUCCESS      0
#define RETURN_USAGE        1
#define RETURN_INVALPARAM   2
#define RETURN_SDP_ERROR    3

void usage()
{
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "\tsdpscanner <remote bdaddr> <local bdaddr>\n\n");
    fprintf(stderr, "Performs an SDP scan on remote device, using the SDP server\n"
                    "represented by the local Bluetooth device.\n");
}

#define BUFFER_SIZE 1024

static void parseAttributeValues(sdp_data_t *data, int indentation, QByteArray &xmlOutput)
{
    if (!data)
        return;

    const int length = indentation*2 + 1;
    QByteArray indentString(length, ' ');

    char snBuffer[BUFFER_SIZE];

    xmlOutput.append(indentString);

    // deal with every dtd type
    switch (data->dtd) {
    case SDP_DATA_NIL:
        xmlOutput.append("<nil/>\n");
        break;
    case SDP_UINT8:
        qsnprintf(snBuffer, BUFFER_SIZE, "<uint8 value=\"0x%02x\"/>\n", data->val.uint8);
        xmlOutput.append(snBuffer);
        break;
    case SDP_UINT16:
        qsnprintf(snBuffer, BUFFER_SIZE, "<uint16 value=\"0x%04x\"/>\n", data->val.uint16);
        xmlOutput.append(snBuffer);
        break;
    case SDP_UINT32:
        qsnprintf(snBuffer, BUFFER_SIZE, "<uint32 value=\"0x%08x\"/>\n", data->val.uint32);
        xmlOutput.append(snBuffer);
        break;
    case SDP_UINT64:
        qsnprintf(snBuffer, BUFFER_SIZE, "<uint64 value=\"0x%016x\"/>\n", data->val.uint64);
        xmlOutput.append(snBuffer);
        break;
    case SDP_UINT128:
        xmlOutput.append("<uint128 value=\"0x");
        for (int i = 0; i < 16; i++)
            ::sprintf(&snBuffer[i * 2], "%02x", data->val.uint128.data[i]);
        xmlOutput.append(snBuffer);
        xmlOutput.append("\"/>\n");
        break;
    case SDP_INT8:
        qsnprintf(snBuffer, BUFFER_SIZE, "<int8 value=\"%d\"/>/n", data->val.int8);
        xmlOutput.append(snBuffer);
        break;
    case SDP_INT16:
        qsnprintf(snBuffer, BUFFER_SIZE, "<int16 value=\"%d\"/>/n", data->val.int16);
        xmlOutput.append(snBuffer);
        break;
    case SDP_INT32:
        qsnprintf(snBuffer, BUFFER_SIZE, "<int32 value=\"%d\"/>/n", data->val.int32);
        xmlOutput.append(snBuffer);
        break;
    case SDP_INT64:
        qsnprintf(snBuffer, BUFFER_SIZE, "<int64 value=\"%d\"/>/n", data->val.int64);
        xmlOutput.append(snBuffer);
        break;
    case SDP_INT128:
        xmlOutput.append("<int128 value=\"0x");
        for (int i = 0; i < 16; i++)
            ::sprintf(&snBuffer[i * 2], "%02x", data->val.int128.data[i]);
        xmlOutput.append(snBuffer);
        xmlOutput.append("\"/>\n");
        break;
    case SDP_UUID_UNSPEC:
        break;
    case SDP_UUID16:
    case SDP_UUID32:
        xmlOutput.append("<uuid value=\"0x");
        sdp_uuid2strn(&(data->val.uuid), snBuffer, BUFFER_SIZE);
        xmlOutput.append(snBuffer);
        xmlOutput.append("\"/>\n");
        break;
    case SDP_UUID128:
        xmlOutput.append("<uuid value=\"");
        sdp_uuid2strn(&(data->val.uuid), snBuffer, BUFFER_SIZE);
        xmlOutput.append(snBuffer);
        xmlOutput.append("\"/>\n");
        break;
    case SDP_TEXT_STR_UNSPEC:
        break;
    case SDP_TEXT_STR8:
    case SDP_TEXT_STR16:
    case SDP_TEXT_STR32:
    {
        xmlOutput.append("<text ");
        QByteArray text = QByteArray::fromRawData(data->val.str, data->unitSize);

        bool hasNonPrintableChar = false;
        for (int i = 0; i < text.count() && !hasNonPrintableChar; i++) {
            if (!isprint(text[i])) {
                hasNonPrintableChar = true;
                break;
            }
        }

        if (hasNonPrintableChar) {
            xmlOutput.append("encoding=\"hex\" value=\"");
            xmlOutput.append(text.toHex());
        } else {
            text.replace("&", "&amp");
            text.replace("<", "&lt");
            text.replace(">", "&gt");
            text.replace("\"", "&quot");

            xmlOutput.append("value=\"");
            xmlOutput.append(text);
        }

        xmlOutput.append("\"/>\n");
        break;
    }
    case SDP_BOOL:
        if (data->val.uint8)
            xmlOutput.append("<boolean value=\"true\"/>\n");
        else
            xmlOutput.append("<boolean value=\"false\"/>\n");
        break;
    case SDP_SEQ_UNSPEC:
        break;
    case SDP_SEQ8:
    case SDP_SEQ16:
    case SDP_SEQ32:
        xmlOutput.append("<sequence>\n");
        parseAttributeValues(data->val.dataseq, indentation + 1, xmlOutput);
        xmlOutput.append(indentString);
        xmlOutput.append("</sequence>\n");
        break;
    case SDP_ALT_UNSPEC:
        break;
    case SDP_ALT8:
    case SDP_ALT16:
    case SDP_ALT32:
        xmlOutput.append("<alternate>\n");
        parseAttributeValues(data->val.dataseq, indentation + 1, xmlOutput);
        xmlOutput.append(indentString);
        xmlOutput.append("</alternate>\n");
        break;
    case SDP_URL_STR_UNSPEC:
        break;
    case SDP_URL_STR8:
    case SDP_URL_STR16:
    case SDP_URL_STR32:
        strncpy(snBuffer, data->val.str, data->unitSize - 1);
        xmlOutput.append("<url value=\"");
        xmlOutput.append(snBuffer);
        xmlOutput.append("\"/>\n");
        break;
    default:
        fprintf(stderr, "Unknown dtd type\n");
    }

    parseAttributeValues(data->next, indentation, xmlOutput);
}

static void parseAttribute(void *value, void *extraData)
{
    sdp_data_t *data = (sdp_data_t *) value;
    QByteArray *xmlOutput = static_cast<QByteArray *>(extraData);

    char buffer[BUFFER_SIZE];

    ::qsnprintf(buffer, BUFFER_SIZE, "  <attribute id=\"0x%04x\">\n", data->attrId);
    xmlOutput->append(buffer);

    parseAttributeValues(data, 2, *xmlOutput);

    xmlOutput->append("  </attribute>\n");
}

// the resulting xml output is based on the already used xml parser
QByteArray parseSdpRecord(sdp_record_t *record)
{
    if (!record || !record->attrlist)
        return QByteArray();

    QByteArray xmlOutput;

    xmlOutput.append("<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<record>\n");

    sdp_list_foreach(record->attrlist, parseAttribute, &xmlOutput);
    xmlOutput.append("</record>");

    return xmlOutput;
}


int main(int argc, char **argv)
{
    if (argc != 3) {
        usage();
        return RETURN_USAGE;
    }

    fprintf(stderr, "SDP for %s %s\n", argv[1], argv[2]);

    bdaddr_t remote;
    bdaddr_t local;
    int result = str2ba(argv[1], &remote);
    if (result < 0) {
        fprintf(stderr, "Invalid remote address: %s\n", argv[1]);
        return RETURN_INVALPARAM;
    }

    result = str2ba(argv[2], &local);
    if (result < 0) {
        fprintf(stderr, "Invalid local address: %s\n", argv[2]);
        return RETURN_INVALPARAM;
    }

    sdp_session_t *session = sdp_connect( &local, &remote, SDP_RETRY_IF_BUSY);
    if (!session) {
        //try one more time if first time failed
        session = sdp_connect( &local, &remote, SDP_RETRY_IF_BUSY);
    }

    if (!session) {
        fprintf(stderr, "Cannot establish sdp session\n");
        return RETURN_SDP_ERROR;
    }

    // set the filter for service matches
    uuid_t publicBrowseGroupUuid;
    sdp_uuid16_create(&publicBrowseGroupUuid, PUBLIC_BROWSE_GROUP);
    sdp_list_t *serviceFilter;
    serviceFilter = sdp_list_append(0, &publicBrowseGroupUuid);

    uint32_t attributeRange = 0x0000ffff; //all attributes
    sdp_list_t *attributes;
    attributes = sdp_list_append(0, &attributeRange);

    sdp_list_t *sdpResults, *previous;
    result = sdp_service_search_attr_req(session, serviceFilter,
                                         SDP_ATTR_REQ_RANGE,
                                         attributes, &sdpResults);
    sdp_list_free(attributes, 0);
    sdp_list_free(serviceFilter, 0);

    if (result != 0) {
        fprintf(stderr, "sdp_service_search_attr_req failed\n");
        sdp_close(session);
        return RETURN_SDP_ERROR;
    }

    QByteArray total;
    while (sdpResults) {
        sdp_record_t *record = (sdp_record_t *) sdpResults->data;

        const QByteArray xml = parseSdpRecord(record);
        total += xml;

        previous = sdpResults;
        sdpResults = sdpResults->next;
        free(previous);
        sdp_record_free(record);
    }

    if (!total.isEmpty()) {
        printf("%s", total.toBase64().constData());
    }

    sdp_close(session);

    return RETURN_SUCCESS;
}
