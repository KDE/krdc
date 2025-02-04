/*
 * SPDX-FileCopyrightText: 2024 Fabio Bas <ctrlaltca@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QMimeData>

#include "rdpcliprdr.h"
#include "rdpsession.h"
#include "rdpview.h"
#include <freerdp/version.h>

UINT RdpClipboard::onSendClientFormatList(CliprdrClientContext *cliprdr)
{
    auto kclip = reinterpret_cast<RdpClipboard *>(cliprdr->custom);
    WINPR_ASSERT(kclip);

    if (!cliprdr) {
        return ERROR_INVALID_PARAMETER;
    }

    UINT32 *pFormatIds = nullptr;
    UINT32 numFormats = ClipboardGetFormatIds(kclip->m_clipboard, &pFormatIds);
    CLIPRDR_FORMAT *formats = reinterpret_cast<CLIPRDR_FORMAT *>(calloc(numFormats, sizeof(CLIPRDR_FORMAT)));

    if (!formats) {
        free(pFormatIds);
        free(formats);
        return ERROR_INTERNAL_ERROR;
    }

    for (UINT32 index = 0; index < numFormats; index++) {
        UINT32 formatId = pFormatIds[index];
        const char *formatName = ClipboardGetFormatName(kclip->m_clipboard, formatId);
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
    formatList.common.msgType = CB_FORMAT_LIST;
    formatList.common.msgFlags = 0;
    formatList.numFormats = numFormats;
    formatList.formats = formats;

    if (!cliprdr->ClientFormatList) {
        free(pFormatIds);
        free(formats);
        return ERROR_INTERNAL_ERROR;
    }

    auto rc = cliprdr->ClientFormatList(cliprdr, &formatList);
    free(pFormatIds);
    free(formats);
    return rc;
}

UINT RdpClipboard::onSendClientFormatDataRequest(CliprdrClientContext *cliprdr, UINT32 formatId)
{
    auto kclip = reinterpret_cast<RdpClipboard *>(cliprdr->custom);
    WINPR_ASSERT(kclip);

    if (!cliprdr->ClientFormatDataRequest) {
        return ERROR_INVALID_PARAMETER;
    }

    CLIPRDR_FORMAT_DATA_REQUEST formatDataRequest = {};
    formatDataRequest.common.msgType = CB_FORMAT_DATA_REQUEST;
    formatDataRequest.common.msgFlags = 0;
    formatDataRequest.requestedFormatId = formatId;

    kclip->m_requestedFormatId = formatId;
    return cliprdr->ClientFormatDataRequest(cliprdr, &formatDataRequest);
}

UINT RdpClipboard::onSendClientCapabilities(CliprdrClientContext *cliprdr)
{
    if (!cliprdr || !cliprdr->ClientCapabilities) {
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
    return cliprdr->ClientCapabilities(cliprdr, &capabilities);
}

UINT RdpClipboard::onMonitorReady(CliprdrClientContext *cliprdr, const CLIPRDR_MONITOR_READY *monitorReady)
{
    if (!cliprdr || !monitorReady) {
        return ERROR_INVALID_PARAMETER;
    }

    UINT rc;
    if ((rc = onSendClientCapabilities(cliprdr)) != CHANNEL_RC_OK) {
        return rc;
    }

    if ((rc = onSendClientFormatList(cliprdr)) != CHANNEL_RC_OK) {
        return rc;
    }

    return CHANNEL_RC_OK;
}

UINT RdpClipboard::onServerCapabilities(CliprdrClientContext *cliprdr, const CLIPRDR_CAPABILITIES *capabilities)
{
    auto kclip = reinterpret_cast<RdpClipboard *>(cliprdr->custom);
    WINPR_ASSERT(kclip);

    if (!cliprdr || !capabilities) {
        return ERROR_INVALID_PARAMETER;
    }

    for (UINT32 index = 0; index < capabilities->cCapabilitiesSets; index++) {
        CLIPRDR_CAPABILITY_SET *capabilitySet = &(capabilities->capabilitySets[index]);

        if ((capabilitySet->capabilitySetType == CB_CAPSTYPE_GENERAL) && (capabilitySet->capabilitySetLength >= CB_CAPSTYPE_GENERAL_LEN)) {
            CLIPRDR_GENERAL_CAPABILITY_SET *generalCapabilitySet = reinterpret_cast<CLIPRDR_GENERAL_CAPABILITY_SET *>(capabilitySet);
            kclip->m_clipboardCapabilities = generalCapabilitySet->generalFlags;
            break;
        }
    }

    return CHANNEL_RC_OK;
}

UINT RdpClipboard::onServerFormatList(CliprdrClientContext *cliprdr, const CLIPRDR_FORMAT_LIST *formatList)
{
    auto kclip = reinterpret_cast<RdpClipboard *>(cliprdr->custom);
    WINPR_ASSERT(kclip);

    if (!cliprdr || !formatList) {
        return ERROR_INVALID_PARAMETER;
    }

    qDeleteAll(kclip->m_serverFormats);
    kclip->m_serverFormats.clear();

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

        kclip->m_serverFormats.append(format);
    }

    UINT rc;
    for (auto format : kclip->m_serverFormats) {
        if (format->formatId == CF_UNICODETEXT) {
            if ((rc = onSendClientFormatDataRequest(cliprdr, CF_UNICODETEXT)) != CHANNEL_RC_OK)
                return rc;

            break;
        } else if (format->formatId == CF_TEXT) {
            if ((rc = onSendClientFormatDataRequest(cliprdr, CF_TEXT)) != CHANNEL_RC_OK)
                return rc;

            break;
        }
    }

    return CHANNEL_RC_OK;
}

UINT RdpClipboard::onServerFormatListResponse(CliprdrClientContext *cliprdr, const CLIPRDR_FORMAT_LIST_RESPONSE *formatListResponse)
{
    if (!cliprdr || !formatListResponse) {
        return ERROR_INVALID_PARAMETER;
    }

    return CHANNEL_RC_OK;
}

UINT RdpClipboard::onServerLockClipboardData(CliprdrClientContext *cliprdr, const CLIPRDR_LOCK_CLIPBOARD_DATA *lockClipboardData)
{
    if (!cliprdr || !lockClipboardData) {
        return ERROR_INVALID_PARAMETER;
    }

    return CHANNEL_RC_OK;
}

UINT RdpClipboard::onServerUnlockClipboardData(CliprdrClientContext *cliprdr, const CLIPRDR_UNLOCK_CLIPBOARD_DATA *unlockClipboardData)
{
    if (!cliprdr || !unlockClipboardData) {
        return ERROR_INVALID_PARAMETER;
    }

    return CHANNEL_RC_OK;
}

UINT RdpClipboard::onServerFormatDataRequest(CliprdrClientContext *cliprdr, const CLIPRDR_FORMAT_DATA_REQUEST *formatDataRequest)
{
    auto kclip = reinterpret_cast<RdpClipboard *>(cliprdr->custom);
    WINPR_ASSERT(kclip);

    if (!cliprdr || !formatDataRequest || !cliprdr->ClientFormatDataResponse) {
        return ERROR_INVALID_PARAMETER;
    }

    UINT32 size;
    auto data = reinterpret_cast<BYTE *>(ClipboardGetData(kclip->m_clipboard, formatDataRequest->requestedFormatId, &size));

    CLIPRDR_FORMAT_DATA_RESPONSE response = {};
    if (data) {
        response.common.msgFlags = CB_RESPONSE_OK;
        response.common.dataLen = size;
        response.requestedFormatData = data;
    } else {
        response.common.msgFlags = CB_RESPONSE_FAIL;
        response.common.dataLen = 0;
        response.requestedFormatData = nullptr;
    }

    auto rc = cliprdr->ClientFormatDataResponse(cliprdr, &response);
    free(data);
    return rc;
}

UINT RdpClipboard::onServerFormatDataResponse(CliprdrClientContext *cliprdr, const CLIPRDR_FORMAT_DATA_RESPONSE *formatDataResponse)
{
    auto kclip = reinterpret_cast<RdpClipboard *>(cliprdr->custom);
    WINPR_ASSERT(kclip);

    if (!cliprdr || !formatDataResponse) {
        return ERROR_INVALID_PARAMETER;
    }

    CLIPRDR_FORMAT *format = nullptr;
    for (auto tmpFormat : kclip->m_serverFormats) {
        if (kclip->m_requestedFormatId == tmpFormat->formatId) {
            format = tmpFormat;
        }
    }
    if (!format) {
        return ERROR_INTERNAL_ERROR;
    }

    UINT32 formatId;
    if (format->formatName) {
        formatId = ClipboardRegisterFormat(kclip->m_clipboard, format->formatName);
    } else {
        formatId = format->formatId;
    }

    UINT32 size = formatDataResponse->common.dataLen;
    if (!ClipboardSetData(kclip->m_clipboard, formatId, formatDataResponse->requestedFormatData, size)) {
        return ERROR_INTERNAL_ERROR;
    }

    if ((formatId == CF_TEXT) || (formatId == CF_UNICODETEXT)) {
        auto data = reinterpret_cast<char *>(ClipboardGetData(kclip->m_clipboard, CF_TEXT, &size));
        size = strnlen(data, size);

        QMimeData *mimeData = new QMimeData;
        mimeData->setText(QString::fromUtf8(data, size));
        kclip->m_krdp->session->rdpView()->remoteClipboardChanged(mimeData);
    }

    return CHANNEL_RC_OK;
}

UINT RdpClipboard::onServerFileContentsRequest(CliprdrClientContext *cliprdr, const CLIPRDR_FILE_CONTENTS_REQUEST *fileContentsRequest)
{
    if (!cliprdr || !fileContentsRequest) {
        return ERROR_INVALID_PARAMETER;
    }

    return CHANNEL_RC_OK;
}

UINT RdpClipboard::onServerFileContentsResponse(CliprdrClientContext *cliprdr, const CLIPRDR_FILE_CONTENTS_RESPONSE *fileContentsResponse)
{
    if (!cliprdr || !fileContentsResponse) {
        return ERROR_INVALID_PARAMETER;
    }

    return CHANNEL_RC_OK;
}

RdpClipboard::RdpClipboard(RdpContext *krdp, CliprdrClientContext *cliprdr)
{
    m_krdp = krdp;
    m_clipboard = ClipboardCreate();
    m_cliprdr = cliprdr;
    cliprdr->custom = reinterpret_cast<void *>(this);
    cliprdr->MonitorReady = onMonitorReady;
    cliprdr->ServerCapabilities = onServerCapabilities;
    cliprdr->ServerFormatList = onServerFormatList;
    cliprdr->ServerFormatListResponse = onServerFormatListResponse;
    cliprdr->ServerLockClipboardData = onServerLockClipboardData;
    cliprdr->ServerUnlockClipboardData = onServerUnlockClipboardData;
    cliprdr->ServerFormatDataRequest = onServerFormatDataRequest;
    cliprdr->ServerFormatDataResponse = onServerFormatDataResponse;
    cliprdr->ServerFileContentsRequest = onServerFileContentsRequest;
    cliprdr->ServerFileContentsResponse = onServerFileContentsResponse;
}

RdpClipboard::~RdpClipboard()
{
    qDeleteAll(m_serverFormats);
    m_serverFormats.clear();

    m_cliprdr->custom = nullptr;
    m_cliprdr = nullptr;
    ClipboardDestroy(m_clipboard);
    m_krdp->clipboard = nullptr;
}

bool RdpClipboard::sendClipboard(const QMimeData *data)
{
    // TODO: add support for other formats like hasImage(), hasHtml()
    if (data->hasText()) {
        const QString text = data->text();

        if (text.isEmpty()) {
            ClipboardEmpty(m_clipboard);
        } else {
            QByteArray bytes = text.toUtf8();
            ClipboardSetData(m_clipboard, CF_TEXT, bytes.data(), bytes.size() + 1);
        }

        onSendClientFormatList(m_cliprdr);
        return true;
    }

    return false;
}