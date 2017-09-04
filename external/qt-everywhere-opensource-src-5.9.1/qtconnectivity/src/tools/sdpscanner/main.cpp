/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtBluetooth module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QtCore/QByteArray>
#include <QtCore/QDebug>
#include <stdio.h>
#include <string>
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
    fprintf(stderr, "\tsdpscanner <remote bdaddr> <local bdaddr> [Options] ({uuids})\n\n");
    fprintf(stderr, "Performs an SDP scan on remote device, using the SDP server\n"
                    "represented by the local Bluetooth device.\n\n"
                    "Options:\n"
                    "   -p                  Show scan results in human-readable form\n"
                    "   -u [list of uuids]  List of uuids which should be scanned for.\n"
                    "                       Each uuid must be enclosed in {}.\n"
                    "                       If the list is empty PUBLIC_BROWSE_GROUP scan is used.\n");
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
        for (int i = 0; i < text.count(); i++) {
            if (text[i] == '\0') {
                text.resize(i); // cut trailing content
                break;
            } else if (!isprint(text[i])) {
                hasNonPrintableChar = true;
                text.resize(text.indexOf('\0')); // cut trailing content
                break;
            }
        }

        if (hasNonPrintableChar) {
            xmlOutput.append("encoding=\"hex\" value=\"");
            xmlOutput.append(text.toHex());
        } else {
            text.replace('&', "&amp;");
            text.replace('<', "&lt;");
            text.replace('>', "&gt;");
            text.replace('"', "&quot;");

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
    if (argc < 3) {
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

    bool showHumanReadable = false;
    std::vector<std::string> targetServices;

    for (int i = 3; i < argc; i++) {
        if (argv[i][0] != '-') {
            usage();
            return RETURN_USAGE;
        }

        switch (argv[i][1])
        {
        case 'p':
            showHumanReadable = true;
            break;
        case 'u':
            i++;

            for ( ; i < argc && argv[i][0] == '{'; i++)
                targetServices.push_back(argv[i]);

            i--; // outer loop increments again
            break;
        default:
            fprintf(stderr, "Wrong argument: %s\n", argv[i]);
            usage();
            return RETURN_USAGE;
        }
    }

    std::vector<uuid_t> uuids;
    for (std::vector<std::string>::const_iterator iter = targetServices.cbegin();
         iter != targetServices.cend(); ++iter) {

        uint128_t temp128;
        uint16_t field1, field2, field3, field5;
        uint32_t field0, field4;

        fprintf(stderr, "Target scan for %s\n", (*iter).c_str());
        if (sscanf((*iter).c_str(), "{%08x-%04hx-%04hx-%04hx-%08x%04hx}", &field0,
                   &field1, &field2, &field3, &field4, &field5) != 6) {
            fprintf(stderr, "Skipping invalid uuid: %s\n", ((*iter).c_str()));
            continue;
        }

        // we need uuid_t conversion based on
        // http://www.spinics.net/lists/linux-bluetooth/msg20356.html
        field0 = htonl(field0);
        field4 = htonl(field4);
        field1 = htons(field1);
        field2 = htons(field2);
        field3 = htons(field3);
        field5 = htons(field5);

        uint8_t* temp = (uint8_t*) &temp128;
        memcpy(&temp[0], &field0, 4);
        memcpy(&temp[4], &field1, 2);
        memcpy(&temp[6], &field2, 2);
        memcpy(&temp[8], &field3, 2);
        memcpy(&temp[10], &field4, 4);
        memcpy(&temp[14], &field5, 2);

        uuid_t sdpUuid;
        sdp_uuid128_create(&sdpUuid, &temp128);
        uuids.push_back(sdpUuid);
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
    if (uuids.empty()) {
        fprintf(stderr, "Using PUBLIC_BROWSE_GROUP for SDP search\n");
        uuid_t publicBrowseGroupUuid;
        sdp_uuid16_create(&publicBrowseGroupUuid, PUBLIC_BROWSE_GROUP);
        uuids.push_back(publicBrowseGroupUuid);
    }

    uint32_t attributeRange = 0x0000ffff; //all attributes
    sdp_list_t *attributes;
    attributes = sdp_list_append(0, &attributeRange);

    sdp_list_t *sdpResults, *sdpIter;
    sdp_list_t *totalResults = NULL;
    sdp_list_t* serviceFilter;

    for (uint i = 0; i < uuids.size(); ++i) {
        serviceFilter = sdp_list_append(0, &uuids[i]);
        result = sdp_service_search_attr_req(session, serviceFilter,
                                         SDP_ATTR_REQ_RANGE,
                                         attributes, &sdpResults);
        sdp_list_free(serviceFilter, 0);
        if (result != 0) {
            fprintf(stderr, "sdp_service_search_attr_req failed\n");
            sdp_list_free(attributes, 0);
            sdp_close(session);
            return RETURN_SDP_ERROR;
        }

        if (!sdpResults)
            continue;

        if (!totalResults) {
            totalResults = sdpResults;
            sdpIter = totalResults;
        } else {
            // attach each new result list to the end of totalResults
            sdpIter->next = sdpResults;
        }

        while (sdpIter->next) // skip to end of list
            sdpIter = sdpIter->next;
    }
    sdp_list_free(attributes, 0);

    // start XML generation from the front
    sdpResults = totalResults;

    QByteArray total;
    while (sdpResults) {
        sdp_record_t *record = (sdp_record_t *) sdpResults->data;

        const QByteArray xml = parseSdpRecord(record);
        total += xml;

        sdpIter = sdpResults;
        sdpResults = sdpResults->next;
        free(sdpIter);
        sdp_record_free(record);
    }

    if (!total.isEmpty()) {
        if (showHumanReadable)
            printf("%s", total.constData());
        else
            printf("%s", total.toBase64().constData());
    }

    sdp_close(session);

    return RETURN_SUCCESS;
}
