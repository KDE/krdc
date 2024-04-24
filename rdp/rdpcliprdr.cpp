/*
 * SPDX-FileCopyrightText: 2024 Fabio Bas <ctrlaltca@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QMimeData>

#include "rdpcliprdr.h"
#include "rdpsession.h"
#include "rdpview.h"

UINT krdc_cliprdr_send_client_format_list(CliprdrClientContext *cliprdr)
{
    UINT32 formatId;
    UINT32 numFormats;
    UINT32 *pFormatIds = nullptr;
    const char *formatName;
    CLIPRDR_FORMAT *formats;
    CLIPRDR_FORMAT_LIST formatList = {};

    if (!cliprdr)
        return ERROR_INVALID_PARAMETER;

    auto rdpC = reinterpret_cast<RdpContext *>(cliprdr->custom);
    if (!rdpC || !rdpC->cliprdr)
        return ERROR_INVALID_PARAMETER;

    numFormats = ClipboardGetFormatIds(rdpC->clipboard, &pFormatIds);
    formats = reinterpret_cast<CLIPRDR_FORMAT *>(calloc(numFormats, sizeof(CLIPRDR_FORMAT)));

    if (!formats) {
        free(pFormatIds);
        free(formats);
        return ERROR_INTERNAL_ERROR;
    }

    for (UINT32 index = 0; index < numFormats; index++) {
        formatId = pFormatIds[index];
        formatName = ClipboardGetFormatName(rdpC->clipboard, formatId);
        formats[index].formatId = formatId;
        formats[index].formatName = nullptr;

        if ((formatId > CF_MAX) && formatName) {
            formats[index].formatName = _strdup(formatName);

            if (!formats[index].formatName) {
                free(pFormatIds);
                free(formats);
                return ERROR_INTERNAL_ERROR;
            }
        }
    }

    formatList.msgFlags = CB_RESPONSE_OK;
    formatList.numFormats = numFormats;
    formatList.formats = formats;
    formatList.msgType = CB_FORMAT_LIST;

    if (!rdpC->cliprdr->ClientFormatList) {
        free(pFormatIds);
        free(formats);
        return ERROR_INTERNAL_ERROR;
    }

    auto rc = rdpC->cliprdr->ClientFormatList(rdpC->cliprdr, &formatList);
    free(pFormatIds);
    free(formats);
    return rc;
}

static UINT krdc_cliprdr_send_client_format_data_request(CliprdrClientContext *cliprdr, UINT32 formatId)
{
    CLIPRDR_FORMAT_DATA_REQUEST formatDataRequest = {};

    if (!cliprdr)
        return ERROR_INVALID_PARAMETER;

    auto rdpC = reinterpret_cast<RdpContext *>(cliprdr->custom);
    if (!rdpC || !cliprdr->ClientFormatDataRequest)
        return ERROR_INVALID_PARAMETER;

    formatDataRequest.msgType = CB_FORMAT_DATA_REQUEST;
    formatDataRequest.msgFlags = 0;
    formatDataRequest.requestedFormatId = formatId;
    rdpC->requestedFormatId = formatId;
    return cliprdr->ClientFormatDataRequest(cliprdr, &formatDataRequest);
}

static UINT krdc_cliprdr_send_client_capabilities(CliprdrClientContext *cliprdr)
{
    CLIPRDR_CAPABILITIES capabilities;
    CLIPRDR_GENERAL_CAPABILITY_SET generalCapabilitySet;

    if (!cliprdr || !cliprdr->ClientCapabilities)
        return ERROR_INVALID_PARAMETER;

    capabilities.cCapabilitiesSets = 1;
    capabilities.capabilitySets = reinterpret_cast<CLIPRDR_CAPABILITY_SET *>(&(generalCapabilitySet));
    generalCapabilitySet.capabilitySetType = CB_CAPSTYPE_GENERAL;
    generalCapabilitySet.capabilitySetLength = 12;
    generalCapabilitySet.version = CB_CAPS_VERSION_2;
    generalCapabilitySet.generalFlags = CB_USE_LONG_FORMAT_NAMES;
    return cliprdr->ClientCapabilities(cliprdr, &capabilities);
}

static UINT krdc_cliprdr_monitor_ready(CliprdrClientContext *cliprdr, const CLIPRDR_MONITOR_READY *monitorReady)
{
    UINT rc;

    if (!cliprdr || !monitorReady)
        return ERROR_INVALID_PARAMETER;

    auto rdpC = reinterpret_cast<RdpContext *>(cliprdr->custom);
    if (!rdpC)
        return ERROR_INVALID_PARAMETER;

    if ((rc = krdc_cliprdr_send_client_capabilities(cliprdr)) != CHANNEL_RC_OK)
        return rc;

    if ((rc = krdc_cliprdr_send_client_format_list(cliprdr)) != CHANNEL_RC_OK)
        return rc;

    return CHANNEL_RC_OK;
}

static UINT krdc_cliprdr_server_capabilities(CliprdrClientContext *cliprdr, const CLIPRDR_CAPABILITIES *capabilities)
{
    CLIPRDR_CAPABILITY_SET *capabilitySet;

    if (!cliprdr || !capabilities)
        return ERROR_INVALID_PARAMETER;

    auto rdpC = reinterpret_cast<RdpContext *>(cliprdr->custom);
    if (!rdpC)
        return ERROR_INVALID_PARAMETER;

    for (UINT32 index = 0; index < capabilities->cCapabilitiesSets; index++) {
        capabilitySet = &(capabilities->capabilitySets[index]);

        if ((capabilitySet->capabilitySetType == CB_CAPSTYPE_GENERAL) && (capabilitySet->capabilitySetLength >= CB_CAPSTYPE_GENERAL_LEN)) {
            CLIPRDR_GENERAL_CAPABILITY_SET *generalCapabilitySet = reinterpret_cast<CLIPRDR_GENERAL_CAPABILITY_SET *>(capabilitySet);
            rdpC->clipboardCapabilities = generalCapabilitySet->generalFlags;
            break;
        }
    }

    return CHANNEL_RC_OK;
}

static UINT krdc_cliprdr_server_format_list(CliprdrClientContext *cliprdr, const CLIPRDR_FORMAT_LIST *formatList)
{
    UINT rc;
    CLIPRDR_FORMAT *format;

    if (!cliprdr || !formatList)
        return ERROR_INVALID_PARAMETER;

    auto rdpC = reinterpret_cast<RdpContext *>(cliprdr->custom);
    if (!rdpC)
        return ERROR_INVALID_PARAMETER;

    if (rdpC->serverFormats) {
        for (UINT32 index = 0; index < rdpC->numServerFormats; index++)
            free(rdpC->serverFormats[index].formatName);

        free(rdpC->serverFormats);
        rdpC->serverFormats = nullptr;
        rdpC->numServerFormats = 0;
    }

    if (formatList->numFormats < 1)
        return CHANNEL_RC_OK;

    rdpC->numServerFormats = formatList->numFormats;
    rdpC->serverFormats = reinterpret_cast<CLIPRDR_FORMAT *>(calloc(rdpC->numServerFormats, sizeof(CLIPRDR_FORMAT)));

    if (!rdpC->serverFormats)
        return CHANNEL_RC_NO_MEMORY;

    for (UINT32 index = 0; index < rdpC->numServerFormats; index++) {
        rdpC->serverFormats[index].formatId = formatList->formats[index].formatId;
        rdpC->serverFormats[index].formatName = nullptr;

        if (formatList->formats[index].formatName) {
            rdpC->serverFormats[index].formatName = _strdup(formatList->formats[index].formatName);

            if (!rdpC->serverFormats[index].formatName)
                return CHANNEL_RC_NO_MEMORY;
        }
    }

    for (UINT32 index = 0; index < rdpC->numServerFormats; index++) {
        format = &(rdpC->serverFormats[index]);

        if (format->formatId == CF_UNICODETEXT) {
            if ((rc = krdc_cliprdr_send_client_format_data_request(cliprdr, CF_UNICODETEXT)) != CHANNEL_RC_OK)
                return rc;

            break;
        } else if (format->formatId == CF_TEXT) {
            if ((rc = krdc_cliprdr_send_client_format_data_request(cliprdr, CF_TEXT)) != CHANNEL_RC_OK)
                return rc;

            break;
        }
    }

    return CHANNEL_RC_OK;
}

static UINT krdc_cliprdr_server_format_list_response(CliprdrClientContext *cliprdr, const CLIPRDR_FORMAT_LIST_RESPONSE *formatListResponse)
{
    if (!cliprdr || !formatListResponse)
        return ERROR_INVALID_PARAMETER;

    return CHANNEL_RC_OK;
}

static UINT krdc_cliprdr_server_lock_clipboard_data(CliprdrClientContext *cliprdr, const CLIPRDR_LOCK_CLIPBOARD_DATA *lockClipboardData)
{
    if (!cliprdr || !lockClipboardData)
        return ERROR_INVALID_PARAMETER;

    return CHANNEL_RC_OK;
}

static UINT krdc_cliprdr_server_unlock_clipboard_data(CliprdrClientContext *cliprdr, const CLIPRDR_UNLOCK_CLIPBOARD_DATA *unlockClipboardData)
{
    if (!cliprdr || !unlockClipboardData)
        return ERROR_INVALID_PARAMETER;

    return CHANNEL_RC_OK;
}

static UINT krdc_cliprdr_server_format_data_request(CliprdrClientContext *cliprdr, const CLIPRDR_FORMAT_DATA_REQUEST *formatDataRequest)
{
    UINT32 size;
    CLIPRDR_FORMAT_DATA_RESPONSE response = {};

    if (!cliprdr || !formatDataRequest || !cliprdr->ClientFormatDataResponse)
        return ERROR_INVALID_PARAMETER;

    auto rdpC = reinterpret_cast<RdpContext *>(cliprdr->custom);
    if (!rdpC)
        return ERROR_INVALID_PARAMETER;

    auto data = reinterpret_cast<BYTE *>(ClipboardGetData(rdpC->clipboard, formatDataRequest->requestedFormatId, &size));
    if (data) {
        response.msgFlags = CB_RESPONSE_OK;
        response.dataLen = size;
        response.requestedFormatData = data;
    } else {
        response.msgFlags = CB_RESPONSE_FAIL;
        response.dataLen = 0;
        response.requestedFormatData = nullptr;
    }

    auto rc = cliprdr->ClientFormatDataResponse(cliprdr, &response);
    free(data);
    return rc;
}

static UINT krdc_cliprdr_server_format_data_response(CliprdrClientContext *cliprdr, const CLIPRDR_FORMAT_DATA_RESPONSE *formatDataResponse)
{
    UINT32 size;
    UINT32 formatId;
    CLIPRDR_FORMAT *format = nullptr;

    if (!cliprdr || !formatDataResponse)
        return ERROR_INVALID_PARAMETER;

    auto rdpC = reinterpret_cast<RdpContext *>(cliprdr->custom);

    if (!rdpC)
        return ERROR_INVALID_PARAMETER;

    for (UINT32 index = 0; index < rdpC->numServerFormats; index++) {
        if (rdpC->requestedFormatId == rdpC->serverFormats[index].formatId)
            format = &(rdpC->serverFormats[index]);
    }

    if (!format)
        return ERROR_INTERNAL_ERROR;

    if (format->formatName) {
        formatId = ClipboardRegisterFormat(rdpC->clipboard, format->formatName);
    } else {
        formatId = format->formatId;
    }

    size = formatDataResponse->dataLen;

    if (!ClipboardSetData(rdpC->clipboard, formatId, formatDataResponse->requestedFormatData, size))
        return ERROR_INTERNAL_ERROR;

    if ((formatId == CF_TEXT) || (formatId == CF_UNICODETEXT)) {
        formatId = ClipboardRegisterFormat(rdpC->clipboard, "UTF8_STRING");
        auto data = reinterpret_cast<char *>(ClipboardGetData(rdpC->clipboard, formatId, &size));
        size = strnlen(data, size);

        QMimeData *mimeData = new QMimeData;
        mimeData->setText(QString::fromUtf8(data, size));
        rdpC->session->rdpView()->remoteClipboardChanged(mimeData);
    }

    return CHANNEL_RC_OK;
}

static UINT krdc_cliprdr_server_file_contents_request(CliprdrClientContext *cliprdr, const CLIPRDR_FILE_CONTENTS_REQUEST *fileContentsRequest)
{
    if (!cliprdr || !fileContentsRequest)
        return ERROR_INVALID_PARAMETER;

    return CHANNEL_RC_OK;
}

static UINT krdc_cliprdr_server_file_contents_response(CliprdrClientContext *cliprdr, const CLIPRDR_FILE_CONTENTS_RESPONSE *fileContentsResponse)
{
    if (!cliprdr || !fileContentsResponse)
        return ERROR_INVALID_PARAMETER;

    return CHANNEL_RC_OK;
}

RdpClipboard::RdpClipboard(RdpContext *rdpC, CliprdrClientContext *cliprdr)
    : QObject(nullptr)
    , m_rdpC(rdpC)
{
    m_rdpC->cliprdr = cliprdr;
    m_rdpC->clipboard = ClipboardCreate();
    m_rdpC->cliprdr->custom = reinterpret_cast<void *>(m_rdpC);
    m_rdpC->cliprdr->MonitorReady = krdc_cliprdr_monitor_ready;
    m_rdpC->cliprdr->ServerCapabilities = krdc_cliprdr_server_capabilities;
    m_rdpC->cliprdr->ServerFormatList = krdc_cliprdr_server_format_list;
    m_rdpC->cliprdr->ServerFormatListResponse = krdc_cliprdr_server_format_list_response;
    m_rdpC->cliprdr->ServerLockClipboardData = krdc_cliprdr_server_lock_clipboard_data;
    m_rdpC->cliprdr->ServerUnlockClipboardData = krdc_cliprdr_server_unlock_clipboard_data;
    m_rdpC->cliprdr->ServerFormatDataRequest = krdc_cliprdr_server_format_data_request;
    m_rdpC->cliprdr->ServerFormatDataResponse = krdc_cliprdr_server_format_data_response;
    m_rdpC->cliprdr->ServerFileContentsRequest = krdc_cliprdr_server_file_contents_request;
    m_rdpC->cliprdr->ServerFileContentsResponse = krdc_cliprdr_server_file_contents_response;
}

RdpClipboard::~RdpClipboard()
{
    m_rdpC->cliprdr->custom = nullptr;
    m_rdpC->cliprdr = nullptr;
    ClipboardDestroy(m_rdpC->clipboard);
    m_rdpC->clipboard = nullptr;
}

bool RdpClipboard::sendClipboard(const QMimeData *data)
{
    // TODO: add support for other formats like hasImage(), hasHtml()
    if (data->hasText()) {
        const QString text = data->text();

        if (text.isEmpty()) {
            ClipboardEmpty(m_rdpC->clipboard);
        } else {
            auto formatId = ClipboardRegisterFormat(m_rdpC->clipboard, "UTF8_STRING");
            QByteArray data = text.toUtf8();
            ClipboardSetData(m_rdpC->clipboard, formatId, data.data(), data.size() + 1);
        }

        krdc_cliprdr_send_client_format_list(m_rdpC->cliprdr);
        return true;
    }

    return false;
}