/*
 * SPDX-FileCopyrightText: 2024 Fabio Bas <ctrlaltca@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

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
    static UINT onSendClientFormatDataRequest(CliprdrClientContext *cliprdr, UINT32 formatId);
    static UINT onSendClientCapabilities(CliprdrClientContext *cliprdr);
    static UINT onMonitorReady(CliprdrClientContext *cliprdr, const CLIPRDR_MONITOR_READY *monitorReady);
    static UINT onServerCapabilities(CliprdrClientContext *cliprdr, const CLIPRDR_CAPABILITIES *capabilities);
    static UINT onServerFormatList(CliprdrClientContext *cliprdr, const CLIPRDR_FORMAT_LIST *formatList);
    static UINT onServerFormatListResponse(CliprdrClientContext *cliprdr, const CLIPRDR_FORMAT_LIST_RESPONSE *formatListResponse);
    static UINT onServerLockClipboardData(CliprdrClientContext *cliprdr, const CLIPRDR_LOCK_CLIPBOARD_DATA *lockClipboardData);
    static UINT onServerUnlockClipboardData(CliprdrClientContext *cliprdr, const CLIPRDR_UNLOCK_CLIPBOARD_DATA *unlockClipboardData);
    static UINT onServerFormatDataRequest(CliprdrClientContext *cliprdr, const CLIPRDR_FORMAT_DATA_REQUEST *formatDataRequest);
    static UINT onServerFormatDataResponse(CliprdrClientContext *cliprdr, const CLIPRDR_FORMAT_DATA_RESPONSE *formatDataResponse);
    static UINT onServerFileContentsRequest(CliprdrClientContext *cliprdr, const CLIPRDR_FILE_CONTENTS_REQUEST *fileContentsRequest);
    static UINT onServerFileContentsResponse(CliprdrClientContext *cliprdr, const CLIPRDR_FILE_CONTENTS_RESPONSE *fileContentsResponse);

private:
    RdpContext *m_krdp;

    wClipboard *m_clipboard = nullptr;
    UINT32 m_requestedFormatId = 0;
    QList<CLIPRDR_FORMAT *> m_serverFormats;
    CliprdrClientContext *m_cliprdr = nullptr;
    UINT32 m_clipboardCapabilities = 0;
};