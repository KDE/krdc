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
    auto clipboard = reinterpret_cast<RdpClipboard *>(cliprdr->custom);
    return clipboard->onSendClientFormatList();
}

UINT krdc_cliprdr_send_client_format_data_request(CliprdrClientContext *cliprdr, UINT32 formatId)
{
    auto clipboard = reinterpret_cast<RdpClipboard *>(cliprdr->custom);
    return clipboard->onSendClientFormatDataRequest(formatId);
}

UINT krdc_cliprdr_send_client_capabilities(CliprdrClientContext *cliprdr)
{
    auto clipboard = reinterpret_cast<RdpClipboard *>(cliprdr->custom);
    return clipboard->onSendClientCapabilities();
}

UINT krdc_cliprdr_monitor_ready(CliprdrClientContext *cliprdr, const CLIPRDR_MONITOR_READY *monitorReady)
{
    auto clipboard = reinterpret_cast<RdpClipboard *>(cliprdr->custom);
    return clipboard->onMonitorReady(monitorReady);
}

UINT krdc_cliprdr_server_capabilities(CliprdrClientContext *cliprdr, const CLIPRDR_CAPABILITIES *capabilities)
{
    auto clipboard = reinterpret_cast<RdpClipboard *>(cliprdr->custom);
    return clipboard->onServerCapabilities(capabilities);
}

UINT krdc_cliprdr_server_format_list(CliprdrClientContext *cliprdr, const CLIPRDR_FORMAT_LIST *formatList)
{
    auto clipboard = reinterpret_cast<RdpClipboard *>(cliprdr->custom);
    return clipboard->onServerFormatList(formatList);
}

UINT krdc_cliprdr_server_format_list_response(CliprdrClientContext *cliprdr, const CLIPRDR_FORMAT_LIST_RESPONSE *formatListResponse)
{
    auto clipboard = reinterpret_cast<RdpClipboard *>(cliprdr->custom);
    return clipboard->onServerFormatListResponse(formatListResponse);
}

UINT krdc_cliprdr_server_lock_clipboard_data(CliprdrClientContext *cliprdr, const CLIPRDR_LOCK_CLIPBOARD_DATA *lockClipboardData)
{
    auto clipboard = reinterpret_cast<RdpClipboard *>(cliprdr->custom);
    return clipboard->onServerLockClipboardData(lockClipboardData);
}

UINT krdc_cliprdr_server_unlock_clipboard_data(CliprdrClientContext *cliprdr, const CLIPRDR_UNLOCK_CLIPBOARD_DATA *unlockClipboardData)
{
    auto clipboard = reinterpret_cast<RdpClipboard *>(cliprdr->custom);
    return clipboard->onServerUnlockClipboardData(unlockClipboardData);
}

UINT krdc_cliprdr_server_format_data_request(CliprdrClientContext *cliprdr, const CLIPRDR_FORMAT_DATA_REQUEST *formatDataRequest)
{
    auto clipboard = reinterpret_cast<RdpClipboard *>(cliprdr->custom);
    return clipboard->onServerFormatDataRequest(formatDataRequest);
}

UINT krdc_cliprdr_server_format_data_response(CliprdrClientContext *cliprdr, const CLIPRDR_FORMAT_DATA_RESPONSE *formatDataResponse)
{
    auto clipboard = reinterpret_cast<RdpClipboard *>(cliprdr->custom);
    return clipboard->onServerFormatDataResponse(formatDataResponse);
}

UINT krdc_cliprdr_server_file_contents_request(CliprdrClientContext *cliprdr, const CLIPRDR_FILE_CONTENTS_REQUEST *fileContentsRequest)
{
    auto clipboard = reinterpret_cast<RdpClipboard *>(cliprdr->custom);
    return clipboard->onServerFileContentsRequest(fileContentsRequest);
}

UINT krdc_cliprdr_server_file_contents_response(CliprdrClientContext *cliprdr, const CLIPRDR_FILE_CONTENTS_RESPONSE *fileContentsResponse)
{
    auto clipboard = reinterpret_cast<RdpClipboard *>(cliprdr->custom);
    return clipboard->onServerFileContentsResponse(fileContentsResponse);
}

UINT RdpClipboard::onSendClientFormatList()
{
    if (!m_rdpC || !m_cliprdr) {
        return ERROR_INVALID_PARAMETER;
    }

    UINT32 *pFormatIds = nullptr;
    UINT32 numFormats = ClipboardGetFormatIds(m_clipboard, &pFormatIds);
    CLIPRDR_FORMAT *formats = reinterpret_cast<CLIPRDR_FORMAT *>(calloc(numFormats, sizeof(CLIPRDR_FORMAT)));

    if (!formats) {
        free(pFormatIds);
        free(formats);
        return ERROR_INTERNAL_ERROR;
    }

    for (UINT32 index = 0; index < numFormats; index++) {
        UINT32 formatId = pFormatIds[index];
        const char *formatName = ClipboardGetFormatName(m_clipboard, formatId);
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

    CLIPRDR_FORMAT_LIST formatList = {};
    formatList.msgFlags = CB_RESPONSE_OK;
    formatList.numFormats = numFormats;
    formatList.formats = formats;
    formatList.msgType = CB_FORMAT_LIST;

    if (!m_cliprdr->ClientFormatList) {
        free(pFormatIds);
        free(formats);
        return ERROR_INTERNAL_ERROR;
    }

    auto rc = m_cliprdr->ClientFormatList(m_cliprdr, &formatList);
    free(pFormatIds);
    free(formats);
    return rc;
}

UINT RdpClipboard::onSendClientFormatDataRequest(UINT32 formatId)
{
    if (!m_rdpC || !m_cliprdr->ClientFormatDataRequest) {
        return ERROR_INVALID_PARAMETER;
    }

    CLIPRDR_FORMAT_DATA_REQUEST formatDataRequest = {};
    formatDataRequest.msgType = CB_FORMAT_DATA_REQUEST;
    formatDataRequest.msgFlags = 0;
    formatDataRequest.requestedFormatId = formatId;
    m_requestedFormatId = formatId;
    return m_cliprdr->ClientFormatDataRequest(m_cliprdr, &formatDataRequest);
}

UINT RdpClipboard::onSendClientCapabilities()
{
    if (!m_cliprdr || !m_cliprdr->ClientCapabilities) {
        return ERROR_INVALID_PARAMETER;
    }

    CLIPRDR_GENERAL_CAPABILITY_SET generalCapabilitySet;
    CLIPRDR_CAPABILITIES capabilities;
    capabilities.cCapabilitiesSets = 1;
    capabilities.capabilitySets = reinterpret_cast<CLIPRDR_CAPABILITY_SET *>(&(generalCapabilitySet));
    generalCapabilitySet.capabilitySetType = CB_CAPSTYPE_GENERAL;
    generalCapabilitySet.capabilitySetLength = 12;
    generalCapabilitySet.version = CB_CAPS_VERSION_2;
    generalCapabilitySet.generalFlags = CB_USE_LONG_FORMAT_NAMES;
    return m_cliprdr->ClientCapabilities(m_cliprdr, &capabilities);
}

UINT RdpClipboard::onMonitorReady(const CLIPRDR_MONITOR_READY *monitorReady)
{
    if (!m_rdpC || !m_cliprdr || !monitorReady) {
        return ERROR_INVALID_PARAMETER;
    }

    UINT rc;
    if ((rc = onSendClientCapabilities()) != CHANNEL_RC_OK) {
        return rc;
    }

    if ((rc = onSendClientFormatList()) != CHANNEL_RC_OK) {
        return rc;
    }

    return CHANNEL_RC_OK;
}

UINT RdpClipboard::onServerCapabilities(const CLIPRDR_CAPABILITIES *capabilities)
{
    if (!m_rdpC || !m_cliprdr || !capabilities) {
        return ERROR_INVALID_PARAMETER;
    }

    for (UINT32 index = 0; index < capabilities->cCapabilitiesSets; index++) {
        CLIPRDR_CAPABILITY_SET *capabilitySet = &(capabilities->capabilitySets[index]);

        if ((capabilitySet->capabilitySetType == CB_CAPSTYPE_GENERAL) && (capabilitySet->capabilitySetLength >= CB_CAPSTYPE_GENERAL_LEN)) {
            CLIPRDR_GENERAL_CAPABILITY_SET *generalCapabilitySet = reinterpret_cast<CLIPRDR_GENERAL_CAPABILITY_SET *>(capabilitySet);
            m_clipboardCapabilities = generalCapabilitySet->generalFlags;
            break;
        }
    }

    return CHANNEL_RC_OK;
}

UINT RdpClipboard::onServerFormatList(const CLIPRDR_FORMAT_LIST *formatList)
{
    if (!m_rdpC || !m_cliprdr || !formatList) {
        return ERROR_INVALID_PARAMETER;
    }

    qDeleteAll(m_serverFormats);
    m_serverFormats.clear();

    if (formatList->numFormats < 1) {
        return CHANNEL_RC_OK;
    }

    for (UINT32 index = 0; index < formatList->numFormats; index++) {
        CLIPRDR_FORMAT *format = new CLIPRDR_FORMAT;
        format->formatId = formatList->formats[index].formatId;
        format->formatName = nullptr;

        if (formatList->formats[index].formatName) {
            format->formatName = _strdup(formatList->formats[index].formatName);

            if (!format->formatName) {
                return CHANNEL_RC_NO_MEMORY;
            }
        }

        m_serverFormats.append(format);
    }

    UINT rc;
    for (auto format : m_serverFormats) {
        if (format->formatId == CF_UNICODETEXT) {
            if ((rc = onSendClientFormatDataRequest(CF_UNICODETEXT)) != CHANNEL_RC_OK)
                return rc;

            break;
        } else if (format->formatId == CF_TEXT) {
            if ((rc = onSendClientFormatDataRequest(CF_TEXT)) != CHANNEL_RC_OK)
                return rc;

            break;
        }
    }

    return CHANNEL_RC_OK;
}

UINT RdpClipboard::onServerFormatListResponse(const CLIPRDR_FORMAT_LIST_RESPONSE *formatListResponse)
{
    if (!m_cliprdr || !formatListResponse) {
        return ERROR_INVALID_PARAMETER;
    }

    return CHANNEL_RC_OK;
}

UINT RdpClipboard::onServerLockClipboardData(const CLIPRDR_LOCK_CLIPBOARD_DATA *lockClipboardData)
{
    if (!m_cliprdr || !lockClipboardData) {
        return ERROR_INVALID_PARAMETER;
    }

    return CHANNEL_RC_OK;
}

UINT RdpClipboard::onServerUnlockClipboardData(const CLIPRDR_UNLOCK_CLIPBOARD_DATA *unlockClipboardData)
{
    if (!m_cliprdr || !unlockClipboardData) {
        return ERROR_INVALID_PARAMETER;
    }

    return CHANNEL_RC_OK;
}

UINT RdpClipboard::onServerFormatDataRequest(const CLIPRDR_FORMAT_DATA_REQUEST *formatDataRequest)
{
    if (!m_rdpC || !m_cliprdr || !formatDataRequest || !m_cliprdr->ClientFormatDataResponse) {
        return ERROR_INVALID_PARAMETER;
    }

    UINT32 size;
    auto data = reinterpret_cast<BYTE *>(ClipboardGetData(m_clipboard, formatDataRequest->requestedFormatId, &size));

    CLIPRDR_FORMAT_DATA_RESPONSE response = {};
    if (data) {
        response.msgFlags = CB_RESPONSE_OK;
        response.dataLen = size;
        response.requestedFormatData = data;
    } else {
        response.msgFlags = CB_RESPONSE_FAIL;
        response.dataLen = 0;
        response.requestedFormatData = nullptr;
    }

    auto rc = m_cliprdr->ClientFormatDataResponse(m_cliprdr, &response);
    free(data);
    return rc;
}

UINT RdpClipboard::onServerFormatDataResponse(const CLIPRDR_FORMAT_DATA_RESPONSE *formatDataResponse)
{
    if (!m_rdpC || !m_cliprdr || !formatDataResponse) {
        return ERROR_INVALID_PARAMETER;
    }

    CLIPRDR_FORMAT *format = nullptr;
    for (auto tmpFormat : m_serverFormats) {
        if (m_requestedFormatId == tmpFormat->formatId) {
            format = tmpFormat;
        }
    }

    if (!format) {
        return ERROR_INTERNAL_ERROR;
    }

    UINT32 formatId;
    if (format->formatName) {
        formatId = ClipboardRegisterFormat(m_clipboard, format->formatName);
    } else {
        formatId = format->formatId;
    }

    UINT32 size = formatDataResponse->dataLen;
    if (!ClipboardSetData(m_clipboard, formatId, formatDataResponse->requestedFormatData, size)) {
        return ERROR_INTERNAL_ERROR;
    }

    if ((formatId == CF_TEXT) || (formatId == CF_UNICODETEXT)) {
        formatId = ClipboardRegisterFormat(m_clipboard, "UTF8_STRING");
        auto data = reinterpret_cast<char *>(ClipboardGetData(m_clipboard, formatId, &size));
        size = strnlen(data, size);

        QMimeData *mimeData = new QMimeData;
        mimeData->setText(QString::fromUtf8(data, size));
        m_rdpC->session->rdpView()->remoteClipboardChanged(mimeData);
    }

    return CHANNEL_RC_OK;
}

UINT RdpClipboard::onServerFileContentsRequest(const CLIPRDR_FILE_CONTENTS_REQUEST *fileContentsRequest)
{
    if (!m_cliprdr || !fileContentsRequest) {
        return ERROR_INVALID_PARAMETER;
    }

    return CHANNEL_RC_OK;
}

UINT RdpClipboard::onServerFileContentsResponse(const CLIPRDR_FILE_CONTENTS_RESPONSE *fileContentsResponse)
{
    if (!m_cliprdr || !fileContentsResponse) {
        return ERROR_INVALID_PARAMETER;
    }

    return CHANNEL_RC_OK;
}

RdpClipboard::RdpClipboard(RdpContext *rdpC, CliprdrClientContext *cliprdr)
    : m_rdpC(rdpC)
{
    m_cliprdr = cliprdr;
    m_clipboard = ClipboardCreate();
    m_cliprdr->custom = reinterpret_cast<void *>(this);
    m_cliprdr->MonitorReady = krdc_cliprdr_monitor_ready;
    m_cliprdr->ServerCapabilities = krdc_cliprdr_server_capabilities;
    m_cliprdr->ServerFormatList = krdc_cliprdr_server_format_list;
    m_cliprdr->ServerFormatListResponse = krdc_cliprdr_server_format_list_response;
    m_cliprdr->ServerLockClipboardData = krdc_cliprdr_server_lock_clipboard_data;
    m_cliprdr->ServerUnlockClipboardData = krdc_cliprdr_server_unlock_clipboard_data;
    m_cliprdr->ServerFormatDataRequest = krdc_cliprdr_server_format_data_request;
    m_cliprdr->ServerFormatDataResponse = krdc_cliprdr_server_format_data_response;
    m_cliprdr->ServerFileContentsRequest = krdc_cliprdr_server_file_contents_request;
    m_cliprdr->ServerFileContentsResponse = krdc_cliprdr_server_file_contents_response;
}

RdpClipboard::~RdpClipboard()
{
    qDeleteAll(m_serverFormats);
    m_serverFormats.clear();

    m_cliprdr->custom = nullptr;
    m_cliprdr = nullptr;
    ClipboardDestroy(m_clipboard);
    m_rdpC->clipboard = nullptr;
}

bool RdpClipboard::sendClipboard(const QMimeData *data)
{
    // TODO: add support for other formats like hasImage(), hasHtml()
    if (data->hasText()) {
        const QString text = data->text();

        if (text.isEmpty()) {
            ClipboardEmpty(m_clipboard);
        } else {
            auto formatId = ClipboardRegisterFormat(m_clipboard, "UTF8_STRING");
            QByteArray bytes = text.toUtf8();
            ClipboardSetData(m_clipboard, formatId, bytes.data(), bytes.size() + 1);
        }

        onSendClientFormatList();
        return true;
    }

    return false;
}