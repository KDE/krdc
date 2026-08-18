/*
 * SPDX-FileCopyrightText: 2024 Fabio Bas <ctrlaltca@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMimeData>
#include <QTemporaryDir>
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

static bool isSafePastePath(const QString &relativePath)
{
    if (relativePath.isEmpty()) {
        return false;
    }
    const QString clean = QDir::cleanPath(relativePath);
    return !QDir::isAbsolutePath(clean) && clean != QLatin1String("..") && !clean.startsWith(QLatin1String("../"));
}

UINT RdpClipboard::onSendClientFormatList(CliprdrClientContext *cliprdr)
{
    auto kclip = reinterpret_cast<RdpClipboard *>(cliprdr->custom);
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

    // Prefer files over text: they are rarely advertised together, and when copying files
    // the server also offers a textual rendering of the path we do not want.
    for (auto format : kclip->m_serverFormats) {
        if (format->formatName && qstrcmp(format->formatName, "FileGroupDescriptorW") == 0) {
            return onSendClientFormatDataRequest(cliprdr, format->formatId);
        }
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

    if (format->formatName && qstrcmp(format->formatName, "FileGroupDescriptorW") == 0) {
        kclip->beginFileFetch(formatDataResponse);
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

UINT RdpClipboard::onServerFileContentsRequest(CliprdrClientContext *cliprdr, const CLIPRDR_FILE_CONTENTS_REQUEST *fileContentsRequest)
{
    auto kclip = reinterpret_cast<RdpClipboard *>(cliprdr->custom);
    WINPR_ASSERT(kclip);

    if (!cliprdr || !fileContentsRequest || !cliprdr->ClientFileContentsResponse) {
        return ERROR_INVALID_PARAMETER;
    }

    const auto respond = [&](const QByteArray &payload, bool ok) {
        CLIPRDR_FILE_CONTENTS_RESPONSE response = {};
        response.common.msgFlags = ok ? CB_RESPONSE_OK : CB_RESPONSE_FAIL;
        response.streamId = fileContentsRequest->streamId;
        response.cbRequested = ok ? UINT32(payload.size()) : 0;
        response.requestedData = ok ? reinterpret_cast<const BYTE *>(payload.constData()) : nullptr;
        return cliprdr->ClientFileContentsResponse(cliprdr, &response);
    };

    QFile file(kclip->m_localFiles.value(int(fileContentsRequest->listIndex)));
    if (!file.open(QIODevice::ReadOnly)) {
        return respond({}, false);
    }

    if (fileContentsRequest->dwFlags & FILECONTENTS_SIZE) {
        QByteArray sizeLe(8, Qt::Uninitialized);
        qToLittleEndian<quint64>(quint64(file.size()), sizeLe.data());
        return respond(sizeLe, true);
    }
    if (fileContentsRequest->dwFlags & FILECONTENTS_RANGE) {
        const quint64 offset = (quint64(fileContentsRequest->nPositionHigh) << 32) | fileContentsRequest->nPositionLow;
        if (!file.seek(offset)) {
            return respond({}, false);
        }
        return respond(file.read(std::min<qint64>(fileContentsRequest->cbRequested, 4 * 1024 * 1024)), true);
    }
    return respond({}, false);
}

void RdpClipboard::beginFileFetch(const CLIPRDR_FORMAT_DATA_RESPONSE *descriptorData)
{
    FILEDESCRIPTORW *descriptors = nullptr;
    UINT32 count = 0;
    if (cliprdr_parse_file_list(descriptorData->requestedFormatData, descriptorData->common.dataLen, &descriptors, &count) != CHANNEL_RC_OK || count == 0) {
        free(descriptors);
        return;
    }

    auto dir = std::make_shared<QTemporaryDir>(QDir(QDir::tempPath()).filePath(QStringLiteral("krdc-clip-XXXXXX")));
    if (!dir->isValid()) {
        free(descriptors);
        return;
    }

    FileFetch f;
    f.dir = dir;
    f.files.reserve(count);
    for (UINT32 i = 0; i < count; ++i) {
        const FILEDESCRIPTORW &fd = descriptors[i];
        IncomingFile entry;
        const auto *raw = reinterpret_cast<const char16_t *>(fd.cFileName);
        int len = 0;
        while (len < 260 && raw[len]) {
            ++len;
        }
        entry.relativePath = QString::fromUtf16(raw, len);
        entry.relativePath.replace(u'\\', u'/'); // Windows uses backslash separators
        if (!isSafePastePath(entry.relativePath)) {
            free(descriptors);
            return;
        }
        entry.isDirectory = (fd.dwFlags & FD_ATTRIBUTES) && (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
        if (fd.dwFlags & FD_FILESIZE) {
            entry.size = (quint64(fd.nFileSizeHigh) << 32) | fd.nFileSizeLow;
        }
        f.files.push_back(entry);
    }
    free(descriptors);

    m_fetch = std::move(f);
    advanceFetch();
}

void RdpClipboard::advanceFetch()
{
    while (m_fetch && m_fetch->index < int(m_fetch->files.size())) {
        const IncomingFile &entry = m_fetch->files[m_fetch->index];
        const QString absolute = m_fetch->dir->filePath(entry.relativePath);

        if (entry.isDirectory) {
            QDir().mkpath(absolute);
            m_fetch->index++;
            continue;
        }

        QDir().mkpath(QFileInfo(absolute).absolutePath());
        if (entry.size == 0) {
            (void)QFile(absolute).open(QIODevice::WriteOnly);
            m_fetch->index++;
            continue;
        }

        m_fetch->current = std::make_unique<QFile>(absolute);
        if (!m_fetch->current->open(QIODevice::WriteOnly)) {
            abortFetch();
            return;
        }
        m_fetch->offset = 0;
        requestChunk();
        return; // wait for the server's response
    }

    finishFetch();
}

void RdpClipboard::requestChunk()
{
    const IncomingFile &entry = m_fetch->files[m_fetch->index];
    const quint64 remaining = entry.size - m_fetch->offset;
    const UINT32 chunk = UINT32(std::min<quint64>(remaining, s_fileChunkSize));

    CLIPRDR_FILE_CONTENTS_REQUEST request = {};
    request.common.msgType = CB_FILECONTENTS_REQUEST;
    request.streamId = m_fetch->streamId = ++m_nextFileContentsStreamId;
    request.listIndex = UINT32(m_fetch->index);
    request.dwFlags = FILECONTENTS_RANGE;
    request.nPositionLow = quint32(m_fetch->offset & 0xFFFFFFFFu);
    request.nPositionHigh = quint32(m_fetch->offset >> 32);
    request.cbRequested = chunk;
    request.haveClipDataId = FALSE;
    m_cliprdr->ClientFileContentsRequest(m_cliprdr, &request);
}

void RdpClipboard::finishFetch()
{
    if (!m_fetch) {
        return;
    }

    // Publish the top-level entries (the first path component of each descriptor) as the
    // client clipboard; subdirectories live underneath and come along with their parent.
    QStringList tops;
    QList<QUrl> urls;
    for (const IncomingFile &entry : m_fetch->files) {
        const QString top = entry.relativePath.section(u'/', 0, 0);
        if (top.isEmpty() || tops.contains(top)) {
            continue;
        }
        tops << top;
        urls << QUrl::fromLocalFile(m_fetch->dir->filePath(top));
    }

    if (urls.isEmpty()) {
        abortFetch();
        return;
    }

    m_remoteFiles = m_fetch->dir; // keep the materialised files alive while they sit on the clipboard
    QMimeData *mimeData = new QMimeData;
    mimeData->setUrls(urls);

    // GTK file managers (Nautilus, Caja, ...) only enable Paste for their own copied-files
    // format, not the plain text/uri-list QMimeData::setUrls() sets. No trailing newline:
    // it parses as an empty extra URI and crashes Caja's status-message formatting.
    QStringList uriStrings;
    for (const QUrl &url : urls) {
        uriStrings << url.toString(QUrl::FullyEncoded);
    }
    const QByteArray copiedFiles = QByteArrayLiteral("copy\n") + uriStrings.join(QLatin1Char('\n')).toUtf8();
    mimeData->setData(QStringLiteral("x-special/mate-copied-files"), copiedFiles);
    mimeData->setData(QStringLiteral("x-special/gnome-copied-files"), copiedFiles);

    m_fetch.reset();
    m_krdp->session->rdpView()->remoteClipboardChanged(mimeData);
}

void RdpClipboard::abortFetch()
{
    if (m_fetch && m_fetch->current) {
        m_fetch->current->close();
    }
    m_fetch.reset();
}

UINT RdpClipboard::onServerFileContentsResponse(CliprdrClientContext *cliprdr, const CLIPRDR_FILE_CONTENTS_RESPONSE *fileContentsResponse)
{
    auto kclip = reinterpret_cast<RdpClipboard *>(cliprdr->custom);
    WINPR_ASSERT(kclip);

    if (!cliprdr || !fileContentsResponse) {
        return ERROR_INVALID_PARAMETER;
    }

    if (!kclip->m_fetch || !kclip->m_fetch->current || fileContentsResponse->streamId != kclip->m_fetch->streamId) {
        return CHANNEL_RC_OK;
    }
    if (!(fileContentsResponse->common.msgFlags & CB_RESPONSE_OK)) {
        kclip->abortFetch();
        return CHANNEL_RC_OK;
    }

    const UINT32 received = fileContentsResponse->cbRequested;
    if (received > 0 && fileContentsResponse->requestedData) {
        if (kclip->m_fetch->current->write(reinterpret_cast<const char *>(fileContentsResponse->requestedData), received) != qint64(received)) {
            kclip->abortFetch();
            return CHANNEL_RC_OK;
        }
        kclip->m_fetch->offset += received;
    }

    const IncomingFile &entry = kclip->m_fetch->files[kclip->m_fetch->index];
    if (kclip->m_fetch->offset >= entry.size || received == 0) {
        kclip->m_fetch->current->close();
        kclip->m_fetch->current.reset();
        kclip->m_fetch->index++;
        kclip->advanceFetch();
    } else {
        kclip->requestChunk();
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

    if (data->hasUrls()) {
        QStringList localFiles;
        QByteArray uriList;
        for (const QUrl &url : data->urls()) {
            if (!url.isLocalFile() || !QFileInfo::exists(url.toLocalFile())) {
                continue;
            }
            localFiles << url.toLocalFile();
            uriList += url.toString(QUrl::FullyEncoded).toUtf8() + "\r\n";
        }

        if (!localFiles.isEmpty()) {
            m_localFiles = localFiles;
            ClipboardSetData(m_clipboard, ClipboardGetFormatId(m_clipboard, "text/uri-list"), uriList.constData(), UINT32(uriList.size()));
            onSendClientFormatList(m_cliprdr);
            return true;
        }
    }

    if (data->hasText()) {
        const QString text = data->text();

        m_localFiles.clear();

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
