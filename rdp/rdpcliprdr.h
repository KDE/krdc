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
    RdpClipboard(RdpContext *rdpC, CliprdrClientContext *cliprdr);
    ~RdpClipboard();

    bool sendClipboard(const QMimeData *data);

private:
    RdpContext *m_rdpC;

    wClipboard *m_clipboard = nullptr;
    UINT32 m_requestedFormatId = 0;
    QList<CLIPRDR_FORMAT *> m_serverFormats;
    CliprdrClientContext *m_cliprdr = nullptr;
    UINT32 m_clipboardCapabilities = 0;

    UINT onSendClientFormatList();
    UINT onSendClientFormatDataRequest(UINT32 formatId);
    UINT onSendClientCapabilities();
    UINT onMonitorReady(const CLIPRDR_MONITOR_READY *monitorReady);
    UINT onServerCapabilities(const CLIPRDR_CAPABILITIES *capabilities);
    UINT onServerFormatList(const CLIPRDR_FORMAT_LIST *formatList);
    UINT onServerFormatListResponse(const CLIPRDR_FORMAT_LIST_RESPONSE *formatListResponse);
    UINT onServerLockClipboardData(const CLIPRDR_LOCK_CLIPBOARD_DATA *lockClipboardData);
    UINT onServerUnlockClipboardData(const CLIPRDR_UNLOCK_CLIPBOARD_DATA *unlockClipboardData);
    UINT onServerFormatDataRequest(const CLIPRDR_FORMAT_DATA_REQUEST *formatDataRequest);
    UINT onServerFormatDataResponse(const CLIPRDR_FORMAT_DATA_RESPONSE *formatDataResponse);
    UINT onServerFileContentsRequest(const CLIPRDR_FILE_CONTENTS_REQUEST *fileContentsRequest);
    UINT onServerFileContentsResponse(const CLIPRDR_FILE_CONTENTS_RESPONSE *fileContentsResponse);

    friend UINT krdc_cliprdr_send_client_format_list(CliprdrClientContext *cliprdr);
    friend UINT krdc_cliprdr_send_client_format_data_request(CliprdrClientContext *cliprdr, UINT32 formatId);
    friend UINT krdc_cliprdr_send_client_capabilities(CliprdrClientContext *cliprdr);
    friend UINT krdc_cliprdr_monitor_ready(CliprdrClientContext *cliprdr, const CLIPRDR_MONITOR_READY *monitorReady);
    friend UINT krdc_cliprdr_server_capabilities(CliprdrClientContext *cliprdr, const CLIPRDR_CAPABILITIES *capabilities);
    friend UINT krdc_cliprdr_server_format_list(CliprdrClientContext *cliprdr, const CLIPRDR_FORMAT_LIST *formatList);
    friend UINT krdc_cliprdr_server_format_list_response(CliprdrClientContext *cliprdr, const CLIPRDR_FORMAT_LIST_RESPONSE *formatListResponse);
    friend UINT krdc_cliprdr_server_lock_clipboard_data(CliprdrClientContext *cliprdr, const CLIPRDR_LOCK_CLIPBOARD_DATA *lockClipboardData);
    friend UINT krdc_cliprdr_server_unlock_clipboard_data(CliprdrClientContext *cliprdr, const CLIPRDR_UNLOCK_CLIPBOARD_DATA *unlockClipboardData);
    friend UINT krdc_cliprdr_server_format_data_request(CliprdrClientContext *cliprdr, const CLIPRDR_FORMAT_DATA_REQUEST *formatDataRequest);
    friend UINT krdc_cliprdr_server_format_data_response(CliprdrClientContext *cliprdr, const CLIPRDR_FORMAT_DATA_RESPONSE *formatDataResponse);
    friend UINT krdc_cliprdr_server_file_contents_request(CliprdrClientContext *cliprdr, const CLIPRDR_FILE_CONTENTS_REQUEST *fileContentsRequest);
    friend UINT krdc_cliprdr_server_file_contents_response(CliprdrClientContext *cliprdr, const CLIPRDR_FILE_CONTENTS_RESPONSE *fileContentsResponse);
};