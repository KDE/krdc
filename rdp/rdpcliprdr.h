/*
 * SPDX-FileCopyrightText: 2024 Fabio Bas <ctrlaltca@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QString>

#include <freerdp/client/client_cliprdr_file.h>
#include <freerdp/client/cliprdr.h>
#include <winpr/clipboard.h>

struct RdpContext;
class QMimeData;

class RdpClipboard
{
public:
    RdpClipboard(RdpContext *krdp, CliprdrClientContext *cliprdr);
    ~RdpClipboard();

    bool sendClipboard(const QMimeData *data);

    static UINT onSendClientFormatList(CliprdrClientContext *cliprdr);
    static UINT onSendClientFormatListResponse(CliprdrClientContext *cliprdr, bool ok);
    static UINT onSendClientFormatDataRequest(CliprdrClientContext *cliprdr, UINT32 formatId);
    static UINT onSendClientCapabilities(CliprdrClientContext *cliprdr);
    static UINT onMonitorReady(CliprdrClientContext *cliprdr, const CLIPRDR_MONITOR_READY *monitorReady);
    static UINT onServerCapabilities(CliprdrClientContext *cliprdr, const CLIPRDR_CAPABILITIES *capabilities);
    static UINT onServerFormatList(CliprdrClientContext *cliprdr, const CLIPRDR_FORMAT_LIST *formatList);
    static UINT onServerFormatListResponse(CliprdrClientContext *cliprdr, const CLIPRDR_FORMAT_LIST_RESPONSE *formatListResponse);
    static UINT onServerFormatDataRequest(CliprdrClientContext *cliprdr, const CLIPRDR_FORMAT_DATA_REQUEST *formatDataRequest);
    static UINT onServerFormatDataResponse(CliprdrClientContext *cliprdr, const CLIPRDR_FORMAT_DATA_RESPONSE *formatDataResponse);
    static UINT onServerFileContentsRequest(CliprdrClientContext *cliprdr, const CLIPRDR_FILE_CONTENTS_REQUEST *fileContentsRequest);

private:
    static RdpClipboard *from(CliprdrClientContext *cliprdr);

    static UINT onDelegateFileSizeSuccess(wClipboardDelegate *delegate, const wClipboardFileSizeRequest *request, UINT64 fileSize);
    static UINT onDelegateFileSizeFailure(wClipboardDelegate *delegate, const wClipboardFileSizeRequest *request, UINT errorCode);
    static UINT onDelegateFileRangeSuccess(wClipboardDelegate *delegate, const wClipboardFileRangeRequest *request, const BYTE *data, UINT32 size);
    static UINT onDelegateFileRangeFailure(wClipboardDelegate *delegate, const wClipboardFileRangeRequest *request, UINT errorCode);
    UINT sendFileContentsResponse(UINT32 streamId, const QByteArray &payload, bool ok);

    void publishReceivedFiles();

    RdpContext *m_krdp;

    wClipboard *m_clipboard = nullptr;
    CliprdrFileContext *m_fileContext = nullptr;
    UINT32 m_requestedFormatId = 0;
    QList<CLIPRDR_FORMAT *> m_serverFormats;
    CliprdrClientContext *m_cliprdr = nullptr;
    UINT32 m_clipboardCapabilities = 0;
    static constexpr quint64 s_fileChunkSize = 4 * 1024 * 1024;
};