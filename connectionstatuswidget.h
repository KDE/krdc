/*
    SPDX-FileCopyrightText: 2026 Tony Murray <murraytony@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef CONNECTIONSTATUSWIDGET_H
#define CONNECTIONSTATUSWIDGET_H

#include "connectionerror.h"
#include "remoteview.h"

#include <QElapsedTimer>
#include <QList>
#include <QUrl>
#include <QWidget>

class KBusyIndicatorWidget;
class QHBoxLayout;
class QHostInfo;
class QLabel;
class QPushButton;
class QScrollArea;
class QVBoxLayout;
class StageIcon;

/**
 * Connection progress and failures, shown as a checklist of the connection steps.
 */
class ConnectionStatusWidget : public QWidget
{
    Q_OBJECT
public:
    ConnectionStatusWidget(const QUrl &url, bool sshTunnel, QWidget *parent);
    ~ConnectionStatusWidget() override;

    void setStatus(RemoteView::RemoteStatus status);
    void setError(const ConnectionError &error);
    /// Show that an established connection dropped and the backend is restoring it.
    void setReconnecting(bool reconnecting, const QString &details);
    bool hasError() const;
    bool hasFatalError() const;
    /// Whether the connection reached Connected at some point.
    bool wasConnected() const;
    /// The address the connection was opened with; it names the host's settings group.
    QUrl url() const;

Q_SIGNALS:
    void cancelRequested();
    void retryRequested();
    void editAddressRequested();
    void editSettingsRequested();
    void closeRequested();

private:
    enum class State {
        Pending,
        Active,
        Done,
        Failed,
        Hidden,
    };

    struct Row {
        ConnectionError::Stage stage;
        State state = State::Pending;
        QWidget *container = nullptr;
        StageIcon *icon = nullptr;
        KBusyIndicatorWidget *busy = nullptr;
        QLabel *title = nullptr;
        QLabel *body = nullptr;
        QVBoxLayout *textLayout = nullptr;
    };

    Row *row(ConnectionError::Stage stage);
    Row *addRow(ConnectionError::Stage stage, int position = -1);
    void setRowState(Row *row, State state);
    void advanceTo(ConnectionError::Stage stage);
    QString activeTitle(ConnectionError::Stage stage) const;
    QString doneTitle(ConnectionError::Stage stage) const;
    QString doneBody(ConnectionError::Stage stage) const;
    bool observed(ConnectionError::Stage stage, ConnectionError::Stage failedStage) const;
    void hostLookedUp(const QHostInfo &info);
    void log(const QString &event);
    QString diagnostics() const;
    void settleLayout();
    void updateLinks();
    void clearButtons();
    QPushButton *addButton(const QString &text, const QString &icon, void (ConnectionStatusWidget::*signal)());

    QUrl m_url;
    bool m_sshTunnel;
    QString m_resolvedAddress;
    int m_lookupId = -1;
    bool m_hasError = false;
    int m_reached = -1; // furthest RemoteView::RemoteStatus seen while connecting
    ConnectionError m_error;

    QElapsedTimer m_elapsed;
    QStringList m_timeline;

    QList<Row *> m_rows;
    QLabel *m_heading;
    QVBoxLayout *m_rowsLayout;
    QWidget *m_content;
    QScrollArea *m_scrollArea;
    QWidget *m_errorExtras;
    QLabel *m_guidance;
    QLabel *m_links;
    QWidget *m_details;
    QLabel *m_technicalDetails;
    QWidget *m_buttons;
    QHBoxLayout *m_buttonsLayout;
    QVBoxLayout *m_extrasLayout;
};

#endif
