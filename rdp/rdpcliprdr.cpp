/*
 * SPDX-FileCopyrightText: 2024 Fabio Bas <ctrlaltca@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QDir>
#include <QFileInfo>
#include <QMimeData>
#include <QUrl>
#include <QtEndian>

#include "rdpcliprdr.h"
#include "rdpsession.h"
#include "rdpview.h"
#include <freerdp/utils/cliprdr_utils.h>
#include <freerdp/version.h>

static void cliprdr_format_free(CLIPRDR_FORMAT *formats, size_t count)
{
    if (!formats)
        return;

    for (size_t x = 0; x < count; x++) {
        free(formats[x].formatName);
    }

    delete[] formats;
}

RdpClipboard *RdpClipboard::from(CliprdrClientContext *cliprdr)
{
    return reinterpret_cast<RdpClipboard *>(cliprdr_file_context_get_context(reinterpret_cast<CliprdrFileContext *>(cliprdr->custom)));
}

UINT RdpClipboard::onSendClientFormatList(CliprdrClientContext *cliprdr)
{
    auto kclip = from(cliprdr);
    WINPR_ASSERT(kclip);

    if (!cliprdr) {
        return ERROR_INVALID_PARAMETER;
    }

    UINT32 *pFormatIds = nullptr;
    UINT32 numFormats = ClipboardGetFormatIds(kclip->m_clipboard, &pFormatIds);
    auto formats = new CLIPRDR_FORMAT[numFormats];

    if (!formats) {
        free(pFormatIds);
        cliprdr_format_free(formats, numFormats);
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
                cliprdr_format_free(formats, numFormats);
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
        cliprdr_format_free(formats, numFormats);
        return ERROR_INTERNAL_ERROR;
    }

    auto rc = cliprdr->ClientFormatList(cliprdr, &formatList);
    free(pFormatIds);
    cliprdr_format_free(formats, numFormats);
    return rc;
}

UINT RdpClipboard::onSendClientFormatListResponse(CliprdrClientContext *cliprdr, bool ok)
{
    if (!cliprdr || !cliprdr->ClientFormatListResponse) {
        return ERROR_INVALID_PARAMETER;
    }

    CLIPRDR_FORMAT_LIST_RESPONSE response = {};
    response.common.msgType = CB_FORMAT_LIST_RESPONSE;
    response.common.msgFlags = ok ? CB_RESPONSE_OK : CB_RESPONSE_FAIL;
    return cliprdr->ClientFormatListResponse(cliprdr, &response);
}

UINT RdpClipboard::onSendClientFormatDataRequest(CliprdrClientContext *cliprdr, UINT32 formatId)
{
    auto kclip = from(cliprdr);
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
    generalCapabilitySet.generalFlags = CB_USE_LONG_FORMAT_NAMES | CB_STREAM_FILECLIP_ENABLED | CB_FILECLIP_NO_FILE_PATHS;
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
    auto kclip = from(cliprdr);
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
    auto kclip = from(cliprdr);
    WINPR_ASSERT(kclip);

    if (!cliprdr || !formatList) {
        return ERROR_INVALID_PARAMETER;
    }

    qDeleteAll(kclip->m_serverFormats);
    kclip->m_serverFormats.clear();

    if (formatList->numFormats < 1) {
        return onSendClientFormatListResponse(cliprdr, true);
    }

    for (UINT32 index = 0; index < formatList->numFormats; index++) {
        CLIPRDR_FORMAT *format = new CLIPRDR_FORMAT;
        format->formatId = formatList->formats[index].formatId;
        format->formatName = nullptr;

        if (formatList->formats[index].formatName) {
            format->formatName = _strdup(formatList->formats[index].formatName);

            if (!format->formatName) {
                delete format;
                (void)onSendClientFormatListResponse(cliprdr, false);
                return CHANNEL_RC_NO_MEMORY;
            }
        }

        kclip->m_serverFormats.append(format);
    }

    UINT rc = onSendClientFormatListResponse(cliprdr, true);
    if (rc != CHANNEL_RC_OK) {
        return rc;
    }

    cliprdr_file_context_notify_new_server_format_list(kclip->m_fileContext);

    // Prefer files over text: they are rarely advertised together, and when copying files
    // the server also offers a textual rendering of the path we do not want.
    for (auto format : kclip->m_serverFormats) {
        if (format->formatName && qstrcmp(format->formatName, "FileGroupDescriptorW") == 0) {
            return onSendClientFormatDataRequest(cliprdr, format->formatId);
        }
    }

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

UINT RdpClipboard::onServerFormatDataRequest(CliprdrClientContext *cliprdr, const CLIPRDR_FORMAT_DATA_REQUEST *formatDataRequest)
{
    auto kclip = from(cliprdr);
    WINPR_ASSERT(kclip);

    if (!cliprdr || !formatDataRequest || !cliprdr->ClientFormatDataResponse) {
        return ERROR_INVALID_PARAMETER;
    }

    UINT32 size;
    auto data = reinterpret_cast<BYTE *>(ClipboardGetData(kclip->m_clipboard, formatDataRequest->requestedFormatId, &size));

    // WinPR hands back a raw FILEDESCRIPTORW array; the wire format needs a cItems prefix.
    if (data && formatDataRequest->requestedFormatId == ClipboardGetFormatId(kclip->m_clipboard, "FileGroupDescriptorW")) {
        BYTE *wireData = nullptr;
        UINT32 wireSize = 0;
        UINT error = cliprdr_serialize_file_list(reinterpret_cast<FILEDESCRIPTORW *>(data), size / sizeof(FILEDESCRIPTORW), &wireData, &wireSize);
        free(data);
        data = (error == NO_ERROR) ? wireData : nullptr;
        size = wireSize;
    }

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
    auto kclip = from(cliprdr);
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

    if (format->formatName && qstrcmp(format->formatName, "FileGroupDescriptorW") == 0) {
        if (!cliprdr_file_context_update_server_data(kclip->m_fileContext,
                                                     kclip->m_clipboard,
                                                     formatDataResponse->requestedFormatData,
                                                     formatDataResponse->common.dataLen)) {
            return ERROR_INTERNAL_ERROR;
        }
        ClipboardSetData(kclip->m_clipboard,
                         ClipboardGetFormatId(kclip->m_clipboard, "FileGroupDescriptorW"),
                         formatDataResponse->requestedFormatData,
                         formatDataResponse->common.dataLen);
        kclip->publishReceivedFiles();
        return CHANNEL_RC_OK;
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

UINT RdpClipboard::sendFileContentsResponse(UINT32 streamId, const QByteArray &payload, bool ok)
{
    CLIPRDR_FILE_CONTENTS_RESPONSE response = {};
    response.common.msgFlags = ok ? CB_RESPONSE_OK : CB_RESPONSE_FAIL;
    response.streamId = streamId;
    response.cbRequested = ok ? UINT32(payload.size()) : 0;
    response.requestedData = ok ? reinterpret_cast<const BYTE *>(payload.constData()) : nullptr;
    return m_cliprdr->ClientFileContentsResponse(m_cliprdr, &response);
}

UINT RdpClipboard::onDelegateFileSizeSuccess(wClipboardDelegate *delegate, const wClipboardFileSizeRequest *request, UINT64 fileSize)
{
    auto kclip = reinterpret_cast<RdpClipboard *>(delegate->custom);
    QByteArray sizeLe(8, Qt::Uninitialized);
    qToLittleEndian<quint64>(fileSize, sizeLe.data());
    return kclip->sendFileContentsResponse(request->streamId, sizeLe, true);
}

UINT RdpClipboard::onDelegateFileSizeFailure(wClipboardDelegate *delegate, const wClipboardFileSizeRequest *request, UINT)
{
    auto kclip = reinterpret_cast<RdpClipboard *>(delegate->custom);
    return kclip->sendFileContentsResponse(request->streamId, {}, false);
}

UINT RdpClipboard::onDelegateFileRangeSuccess(wClipboardDelegate *delegate, const wClipboardFileRangeRequest *request, const BYTE *data, UINT32 size)
{
    auto kclip = reinterpret_cast<RdpClipboard *>(delegate->custom);
    return kclip->sendFileContentsResponse(request->streamId, QByteArray(reinterpret_cast<const char *>(data), int(size)), true);
}

UINT RdpClipboard::onDelegateFileRangeFailure(wClipboardDelegate *delegate, const wClipboardFileRangeRequest *request, UINT)
{
    auto kclip = reinterpret_cast<RdpClipboard *>(delegate->custom);
    return kclip->sendFileContentsResponse(request->streamId, {}, false);
}

UINT RdpClipboard::onServerFileContentsRequest(CliprdrClientContext *cliprdr, const CLIPRDR_FILE_CONTENTS_REQUEST *fileContentsRequest)
{
    auto kclip = from(cliprdr);
    WINPR_ASSERT(kclip);

    if (!cliprdr || !fileContentsRequest || !cliprdr->ClientFileContentsResponse) {
        return ERROR_INVALID_PARAMETER;
    }

    wClipboardDelegate *delegate = ClipboardGetDelegate(kclip->m_clipboard);

    if (fileContentsRequest->dwFlags & FILECONTENTS_SIZE) {
        wClipboardFileSizeRequest request = {};
        request.streamId = fileContentsRequest->streamId;
        request.listIndex = fileContentsRequest->listIndex;
        if (delegate->ClientRequestFileSize(delegate, &request) == CHANNEL_RC_OK) {
            return CHANNEL_RC_OK;
        }
        return kclip->sendFileContentsResponse(fileContentsRequest->streamId, {}, false);
    }
    if (fileContentsRequest->dwFlags & FILECONTENTS_RANGE) {
        wClipboardFileRangeRequest request = {};
        request.streamId = fileContentsRequest->streamId;
        request.listIndex = fileContentsRequest->listIndex;
        request.nPositionLow = fileContentsRequest->nPositionLow;
        request.nPositionHigh = fileContentsRequest->nPositionHigh;
        request.cbRequested = UINT32(std::min<UINT32>(fileContentsRequest->cbRequested, UINT32(s_fileChunkSize)));
        if (delegate->ClientRequestFileRange(delegate, &request) == CHANNEL_RC_OK) {
            return CHANNEL_RC_OK;
        }
        return kclip->sendFileContentsResponse(fileContentsRequest->streamId, {}, false);
    }
    return kclip->sendFileContentsResponse(fileContentsRequest->streamId, {}, false);
}

void RdpClipboard::publishReceivedFiles()
{
    const char *basePath = ClipboardGetDelegate(m_clipboard)->basePath;
    if (!basePath) {
        return;
    }

    QDir dir(QString::fromUtf8(basePath));
    QList<QUrl> urls;
    for (const QString &name : dir.entryList(QDir::AllEntries | QDir::NoDotAndDotDot)) {
        urls << QUrl::fromLocalFile(dir.filePath(name));
    }
    if (urls.isEmpty()) {
        return;
    }

    QMimeData *mimeData = new QMimeData;
    mimeData->setUrls(urls);

    for (const char *mime : {"x-special/gnome-copied-files", "x-special/mate-copied-files"}) {
        UINT32 size = 0;
        if (auto *data = reinterpret_cast<char *>(ClipboardGetData(m_clipboard, ClipboardGetFormatId(m_clipboard, mime), &size))) {
            mimeData->setData(QString::fromUtf8(mime), QByteArray(data, int(size)));
            free(data);
        }
    }

    m_krdp->session->rdpView()->remoteClipboardChanged(mimeData);
}

RdpClipboard::RdpClipboard(RdpContext *krdp, CliprdrClientContext *cliprdr)
{
    m_krdp = krdp;
    m_clipboard = ClipboardCreate();
    m_cliprdr = cliprdr;
    cliprdr->MonitorReady = onMonitorReady;
    cliprdr->ServerCapabilities = onServerCapabilities;
    cliprdr->ServerFormatList = onServerFormatList;
    cliprdr->ServerFormatListResponse = onServerFormatListResponse;
    cliprdr->ServerFormatDataRequest = onServerFormatDataRequest;
    cliprdr->ServerFormatDataResponse = onServerFormatDataResponse;

    m_fileContext = cliprdr_file_context_new(this);
    cliprdr_file_context_init(m_fileContext, cliprdr);
    cliprdr->ServerFileContentsRequest = onServerFileContentsRequest;

    wClipboardDelegate *delegate = ClipboardGetDelegate(m_clipboard);
    delegate->custom = this;
    delegate->ClipboardFileSizeSuccess = onDelegateFileSizeSuccess;
    delegate->ClipboardFileSizeFailure = onDelegateFileSizeFailure;
    delegate->ClipboardFileRangeSuccess = onDelegateFileRangeSuccess;
    delegate->ClipboardFileRangeFailure = onDelegateFileRangeFailure;
}

RdpClipboard::~RdpClipboard()
{
    qDeleteAll(m_serverFormats);
    m_serverFormats.clear();

    cliprdr_file_context_uninit(m_fileContext, m_cliprdr);
    cliprdr_file_context_free(m_fileContext);

    m_cliprdr->custom = nullptr;
    m_cliprdr = nullptr;
    ClipboardDestroy(m_clipboard);
    m_krdp->clipboard = nullptr;
}

bool RdpClipboard::sendClipboard(const QMimeData *data)
{
    // TODO: add support for other formats like hasImage(), hasHtml()

    if (data->hasUrls()) {
        bool haveLocalFiles = false;
        QByteArray uriList;
        for (const QUrl &url : data->urls()) {
            if (!url.isLocalFile() || !QFileInfo::exists(url.toLocalFile())) {
                continue;
            }
            haveLocalFiles = true;
            uriList += url.toString(QUrl::FullyEncoded).toUtf8() + "\r\n";
        }

        if (haveLocalFiles) {
            ClipboardSetData(m_clipboard, ClipboardGetFormatId(m_clipboard, "text/uri-list"), uriList.constData(), UINT32(uriList.size()));
            onSendClientFormatList(m_cliprdr);
            return true;
        }
    }

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
