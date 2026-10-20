/*
    SPDX-FileCopyrightText: 2026 Tony Murray <murraytony@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "connectionstatuswidget.h"
#include "krdc_debug.h"
#include "krdc_version.h"

#include <KBusyIndicatorWidget>
#include <KLocalizedString>

#include <functional>

#include <QApplication>
#include <QClipboard>
#include <QCoreApplication>
#include <QDateTime>
#include <QFontDatabase>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHostAddress>
#include <QHostInfo>
#include <QIcon>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScrollArea>
#include <QSysInfo>
#include <QTimer>
#include <QVBoxLayout>

// Breeze positive, negative and neutral colors
static const QColor s_doneColor(39, 174, 96);
static const QColor s_failedColor(218, 68, 83);
static const QColor s_noticeColor(61, 174, 233);
static const int s_iconSize = 22;

class StageIcon : public QWidget
{
public:
    enum class Kind {
        Pending,
        Done,
        Failed,
        Notice,
    };

    explicit StageIcon(QWidget *parent)
        : QWidget(parent)
    {
        setFixedSize(s_iconSize, s_iconSize);
    }

    void setKind(Kind kind)
    {
        m_kind = kind;
        // The state is only drawn, so tell screen readers in words.
        switch (kind) {
        case Kind::Pending:
            setAccessibleName(i18nc("@info:status connection step", "Not started"));
            break;
        case Kind::Done:
            setAccessibleName(i18nc("@info:status connection step", "Done"));
            break;
        case Kind::Failed:
            setAccessibleName(i18nc("@info:status connection step", "Failed"));
            break;
        case Kind::Notice:
            setAccessibleName(i18nc("@info:status connection step", "Information"));
            break;
        }
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        QColor color = palette().color(QPalette::PlaceholderText);
        if (m_kind == Kind::Done) {
            color = s_doneColor;
        } else if (m_kind == Kind::Failed) {
            color = s_failedColor;
        } else if (m_kind == Kind::Notice) {
            color = s_noticeColor;
        }

        const qreal penWidth = 2.0;
        const QRectF circle = QRectF(rect()).adjusted(penWidth, penWidth, -penWidth, -penWidth);
        QPen pen(color, penWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        painter.setPen(pen);
        painter.drawEllipse(circle);

        const QPointF c = circle.center();
        const qreal r = circle.width() / 2;
        if (m_kind == Kind::Done) {
            QPainterPath check;
            check.moveTo(c.x() - r * 0.45, c.y() + r * 0.02);
            check.lineTo(c.x() - r * 0.12, c.y() + r * 0.35);
            check.lineTo(c.x() + r * 0.48, c.y() - r * 0.32);
            painter.drawPath(check);
        } else if (m_kind == Kind::Failed) {
            painter.drawLine(QPointF(c.x(), c.y() - r * 0.5), QPointF(c.x(), c.y() + r * 0.1));
            painter.setBrush(color);
            painter.drawEllipse(QPointF(c.x(), c.y() + r * 0.45), penWidth / 2, penWidth / 2);
        } else if (m_kind == Kind::Notice) {
            painter.drawLine(QPointF(c.x(), c.y() - r * 0.05), QPointF(c.x(), c.y() + r * 0.5));
            painter.setBrush(color);
            painter.drawEllipse(QPointF(c.x(), c.y() - r * 0.42), penWidth / 2, penWidth / 2);
        }
    }

private:
    Kind m_kind = Kind::Pending;
};

// QUrl::RemovePassword would leave "vnc://@host" without a user name.
static QString displayUrl(QUrl url)
{
    url.setPassword(QString());
    if (url.userName().isEmpty()) {
        url.setUserInfo(QString());
    }
    return url.toDisplayString(QUrl::StripTrailingSlash);
}

static QString htmlList(const QStringList &items)
{
    QString html = QStringLiteral("<ul style=\"margin-left: -20px; margin-top: 2px;\">");
    for (const QString &item : items) {
        html += QStringLiteral("<li>") + item.toHtmlEscaped() + QStringLiteral("</li>");
    }
    return html + QStringLiteral("</ul>");
}

ConnectionStatusWidget::ConnectionStatusWidget(const QUrl &url, bool sshTunnel, QWidget *parent)
    : QWidget(parent)
    , m_url(url)
    , m_sshTunnel(sshTunnel)
{
    m_elapsed.start();

    // Use the application window color, not the remote desktop background set on the parent.
    QPalette ownPalette = palette();
    ownPalette.setBrush(QPalette::Window, QApplication::palette(this).window());
    setPalette(ownPalette);

    setAutoFillBackground(true);

    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    auto *scrollArea = new QScrollArea(this);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setWidgetResizable(true);
    scrollArea->viewport()->setAutoFillBackground(false);
    outerLayout->addWidget(scrollArea);

    auto *content = new QWidget(scrollArea);
    content->setAutoFillBackground(false);
    scrollArea->setWidget(content);
    m_content = content;
    m_scrollArea = scrollArea;
    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(32, 28, 32, 28);

    auto *column = new QWidget(content);
    column->setMaximumWidth(720);
    column->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto *columnRow = new QHBoxLayout;
    columnRow->addWidget(column, 1);
    columnRow->addStretch(0);
    contentLayout->addLayout(columnRow);
    contentLayout->addStretch(1);

    auto *columnLayout = new QVBoxLayout(column);
    columnLayout->setContentsMargins(0, 0, 0, 0);
    columnLayout->setSpacing(14);

    m_heading = new QLabel(column);
    m_heading->setTextFormat(Qt::PlainText);
    QFont headingFont = m_heading->font();
    headingFont.setPointSizeF(headingFont.pointSizeF() * 1.35);
    headingFont.setBold(true);
    m_heading->setFont(headingFont);
    m_heading->setWordWrap(true);
    columnLayout->addWidget(m_heading);

    auto *address = new QLabel(column);
    address->setTextFormat(Qt::PlainText);
    address->setText(displayUrl(url));
    address->setForegroundRole(QPalette::PlaceholderText);
    address->setTextInteractionFlags(Qt::TextSelectableByMouse);
    columnLayout->addWidget(address);

    m_rowsLayout = new QVBoxLayout;
    m_rowsLayout->setSpacing(12);
    columnLayout->addLayout(m_rowsLayout);

    // Moved under the failed step on error.
    m_buttons = new QWidget(column);
    m_buttonsLayout = new QHBoxLayout(m_buttons);
    m_buttonsLayout->setContentsMargins(0, 4, 0, 4);
    m_buttonsLayout->setAlignment(Qt::AlignLeft);
    columnLayout->addWidget(m_buttons);

    m_errorExtras = new QWidget(column);
    auto *extrasLayout = new QVBoxLayout(m_errorExtras);
    extrasLayout->setContentsMargins(0, 0, 0, 0);
    extrasLayout->setSpacing(8);
    m_guidance = new QLabel(m_errorExtras);
    m_guidance->setWordWrap(true);
    m_guidance->setTextFormat(Qt::RichText);
    extrasLayout->addWidget(m_guidance);
    m_extrasLayout = extrasLayout;
    m_links = new QLabel(m_errorExtras);
    m_links->setTextFormat(Qt::RichText);
    m_links->setTextInteractionFlags(Qt::LinksAccessibleByMouse | Qt::LinksAccessibleByKeyboard);
    extrasLayout->addWidget(m_links);

    m_details = new QWidget(m_errorExtras);
    auto *detailsLayout = new QVBoxLayout(m_details);
    detailsLayout->setContentsMargins(0, 0, 0, 0);
    m_technicalDetails = new QLabel(m_details);
    m_technicalDetails->setTextFormat(Qt::PlainText);
    m_technicalDetails->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_technicalDetails->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_technicalDetails->setWordWrap(true);
    m_technicalDetails->setFrameShape(QFrame::StyledPanel);
    m_technicalDetails->setMargin(8);
    m_technicalDetails->setBackgroundRole(QPalette::Base);
    m_technicalDetails->setAutoFillBackground(true);
    detailsLayout->addWidget(m_technicalDetails);
    m_details->hide();
    extrasLayout->addWidget(m_details);
    m_errorExtras->hide();

    connect(m_links, &QLabel::linkActivated, this, [this](const QString &link) {
        if (link == QLatin1String("details")) {
            m_details->setVisible(!m_details->isVisible());
            updateLinks();
            settleLayout();
        } else if (link == QLatin1String("copy")) {
            QGuiApplication::clipboard()->setText(diagnostics());
            m_links->setText(i18nc("@info", "Diagnostic information copied to the clipboard"));
            QTimer::singleShot(2500, m_links, [this]() {
                updateLinks();
            });
        }
    });

    m_heading->setText(i18nc("@title", "Connecting to %1", url.host()));
    log(i18nc("@info diagnostic timeline", "Connecting to %1", displayUrl(url)));

    // Our own lookup, to show this step; not needed for IP addresses.
    if (QHostAddress(url.host()).isNull() && !url.host().isEmpty()) {
        setRowState(addRow(ConnectionError::Stage::NameResolution), State::Active);
        m_lookupId = QHostInfo::lookupHost(url.host(), this, &ConnectionStatusWidget::hostLookedUp);
    } else {
        m_resolvedAddress = url.host();
    }
    if (m_sshTunnel) {
        setRowState(addRow(ConnectionError::Stage::SshTunnel), State::Active);
    }
    setRowState(addRow(ConnectionError::Stage::Network), m_sshTunnel ? State::Pending : State::Active);
    addRow(ConnectionError::Stage::Authentication);
    addRow(ConnectionError::Stage::Session);

    addButton(i18nc("@action:button", "Cancel"), QStringLiteral("dialog-cancel"), &ConnectionStatusWidget::cancelRequested);
}

ConnectionStatusWidget::~ConnectionStatusWidget()
{
    if (m_lookupId >= 0) {
        QHostInfo::abortHostLookup(m_lookupId);
    }
    qDeleteAll(m_rows);
}

bool ConnectionStatusWidget::hasError() const
{
    return m_hasError;
}

bool ConnectionStatusWidget::hasFatalError() const
{
    return m_hasError && m_error.fatal;
}

bool ConnectionStatusWidget::wasConnected() const
{
    return m_reached >= RemoteView::Connected;
}

QUrl ConnectionStatusWidget::url() const
{
    return m_url;
}

ConnectionStatusWidget::Row *ConnectionStatusWidget::row(ConnectionError::Stage stage)
{
    for (Row *r : std::as_const(m_rows)) {
        if (r->stage == stage) {
            return r;
        }
    }
    return nullptr;
}

ConnectionStatusWidget::Row *ConnectionStatusWidget::addRow(ConnectionError::Stage stage, int position)
{
    auto *r = new Row;
    r->stage = stage;
    r->container = new QWidget(this);

    auto *layout = new QHBoxLayout(r->container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    auto *iconColumn = new QVBoxLayout;
    iconColumn->setContentsMargins(0, 0, 0, 0);
    r->icon = new StageIcon(r->container);
    r->busy = new KBusyIndicatorWidget(r->container);
    r->busy->setFixedSize(s_iconSize, s_iconSize);
    r->busy->setAccessibleName(i18nc("@info:status connection step", "In progress"));
    r->busy->hide();
    iconColumn->addWidget(r->icon);
    iconColumn->addWidget(r->busy);
    iconColumn->addStretch(1);
    layout->addLayout(iconColumn);

    r->textLayout = new QVBoxLayout;
    r->textLayout->setSpacing(4);
    r->title = new QLabel(r->container);
    r->title->setTextFormat(Qt::PlainText);
    QFont titleFont = r->title->font();
    titleFont.setPointSizeF(titleFont.pointSizeF() * 1.1);
    titleFont.setBold(true);
    r->title->setFont(titleFont);
    r->title->setWordWrap(true);
    r->body = new QLabel(r->container);
    r->body->setTextFormat(Qt::PlainText);
    r->body->setWordWrap(true);
    r->body->setTextInteractionFlags(Qt::TextSelectableByMouse);
    r->body->hide();
    r->textLayout->addWidget(r->title);
    r->textLayout->addWidget(r->body);
    layout->addLayout(r->textLayout, 1);

    if (position < 0 || position > m_rows.size()) {
        position = m_rows.size();
    }
    m_rows.insert(position, r);
    m_rowsLayout->insertWidget(position, r->container);
    setRowState(r, State::Pending);
    return r;
}

void ConnectionStatusWidget::setRowState(Row *r, State state)
{
    if (!r) {
        return;
    }
    r->state = state;
    r->container->setVisible(state != State::Hidden);
    r->busy->setVisible(state == State::Active);
    r->icon->setVisible(state != State::Active);

    switch (state) {
    case State::Pending:
    case State::Hidden:
        r->icon->setKind(StageIcon::Kind::Pending);
        r->title->setText(activeTitle(r->stage));
        r->title->setForegroundRole(QPalette::PlaceholderText);
        r->body->hide();
        break;
    case State::Active:
        r->title->setText(activeTitle(r->stage) + QStringLiteral("…"));
        r->title->setForegroundRole(QPalette::WindowText);
        r->body->hide();
        break;
    case State::Done:
        r->icon->setKind(StageIcon::Kind::Done);
        r->title->setText(doneTitle(r->stage));
        r->title->setForegroundRole(QPalette::WindowText);
        r->body->setText(doneBody(r->stage));
        r->body->setForegroundRole(QPalette::PlaceholderText);
        r->body->setVisible(!r->body->text().isEmpty());
        break;
    case State::Failed:
        r->icon->setKind(StageIcon::Kind::Failed);
        r->title->setForegroundRole(QPalette::WindowText);
        break;
    }
}

QString ConnectionStatusWidget::activeTitle(ConnectionError::Stage stage) const
{
    switch (stage) {
    case ConnectionError::Stage::NameResolution:
        return i18nc("@info connection step", "Looking up the server address");
    case ConnectionError::Stage::SshTunnel:
        return i18nc("@info connection step", "Opening the SSH tunnel");
    case ConnectionError::Stage::Network:
        return i18nc("@info connection step", "Opening a network connection");
    case ConnectionError::Stage::Security:
        return i18nc("@info connection step", "Securing the connection");
    case ConnectionError::Stage::Authentication:
        return i18nc("@info connection step", "Logging in");
    case ConnectionError::Stage::Session:
        return i18nc("@info connection step", "Starting the remote session");
    case ConnectionError::Stage::ConnectionLost:
        return i18nc("@info connection step", "Reconnecting");
    default:
        return ConnectionError::stageName(stage);
    }
}

QString ConnectionStatusWidget::doneTitle(ConnectionError::Stage stage) const
{
    switch (stage) {
    case ConnectionError::Stage::NameResolution:
        return i18nc("@info connection step", "Server address resolved");
    case ConnectionError::Stage::SshTunnel:
        return i18nc("@info connection step", "SSH tunnel established");
    case ConnectionError::Stage::Network:
        return i18nc("@info connection step", "Network connection established");
    case ConnectionError::Stage::Security:
        return i18nc("@info connection step", "Connection secured");
    case ConnectionError::Stage::Authentication:
        // Not "logged in": servers without authentication pass this step too.
        return i18nc("@info connection step", "Access granted");
    case ConnectionError::Stage::Session:
        return i18nc("@info connection step", "Remote session started");
    default:
        return ConnectionError::stageName(stage);
    }
}

QString ConnectionStatusWidget::doneBody(ConnectionError::Stage stage) const
{
    switch (stage) {
    case ConnectionError::Stage::NameResolution:
        return m_resolvedAddress.isEmpty() ? QString() : i18nc("@info", "Resolved to %1.", m_resolvedAddress);
    default:
        return QString();
    }
}

void ConnectionStatusWidget::hostLookedUp(const QHostInfo &info)
{
    m_lookupId = -1;
    Row *r = row(ConnectionError::Stage::NameResolution);
    if (info.error() != QHostInfo::NoError || info.addresses().isEmpty()) {
        // A gateway or proxy may resolve names this computer cannot; let the backend decide.
        log(i18nc("@info diagnostic timeline", "Local address lookup failed: %1", info.errorString()));
        return;
    }

    m_resolvedAddress = info.addresses().constFirst().toString();
    log(i18nc("@info diagnostic timeline", "%1 resolved to %2", m_url.host(), m_resolvedAddress));
    if (r && r->state == State::Active && !m_hasError) {
        setRowState(r, State::Done);
    }
}

void ConnectionStatusWidget::advanceTo(ConnectionError::Stage stage)
{
    bool reached = false;
    for (Row *r : std::as_const(m_rows)) {
        if (r->stage == stage) {
            reached = true;
            if (r->state != State::Failed) {
                setRowState(r, State::Active);
            }
        } else if (!reached) {
            // Resolved elsewhere, e.g. by a gateway: don't claim it.
            const bool unverified = r->stage == ConnectionError::Stage::NameResolution && m_resolvedAddress.isEmpty();
            setRowState(r, unverified ? State::Hidden : State::Done);
        }
    }
}

void ConnectionStatusWidget::setStatus(RemoteView::RemoteStatus status)
{
    m_reached = qMax(m_reached, static_cast<int>(status));
    if (m_hasError) {
        return;
    }

    switch (status) {
    case RemoteView::Connecting:
        log(i18nc("@info diagnostic timeline", "Connecting to the remote desktop service"));
        advanceTo(ConnectionError::Stage::Network);
        if (Row *r = row(ConnectionError::Stage::NameResolution); r && m_lookupId >= 0) {
            setRowState(r, State::Active);
        }
        break;
    case RemoteView::Authenticating:
        log(i18nc("@info diagnostic timeline", "Authenticating"));
        advanceTo(ConnectionError::Stage::Authentication);
        break;
    case RemoteView::Preparing:
        log(i18nc("@info diagnostic timeline", "Preparing the session"));
        advanceTo(ConnectionError::Stage::Session);
        break;
    case RemoteView::Connected:
        log(i18nc("@info diagnostic timeline", "Connected"));
        break;
    default:
        break;
    }
}

bool ConnectionStatusWidget::observed(ConnectionError::Stage stage, ConnectionError::Stage failedStage) const
{
    using Stage = ConnectionError::Stage;
    // Failing at one of these stages proves the server answered.
    const bool serverAnswered = failedStage == Stage::Security || failedStage == Stage::Authentication || failedStage == Stage::Session;

    switch (stage) {
    case Stage::NameResolution:
        return !m_resolvedAddress.isEmpty();
    case Stage::SshTunnel:
        // The view only starts connecting once the tunnel is listening.
        return m_reached >= RemoteView::Connecting || failedStage == Stage::Network || serverAnswered;
    case Stage::Network:
        return m_reached >= RemoteView::Authenticating || serverAnswered;
    case Stage::Security:
    case Stage::Authentication:
        return m_reached >= RemoteView::Preparing;
    case Stage::Session:
        return m_reached >= RemoteView::Connected;
    default:
        return false;
    }
}

void ConnectionStatusWidget::setReconnecting(bool reconnecting, const QString &details)
{
    log(reconnecting ? i18nc("@info diagnostic timeline", "Connection dropped, reconnecting: %1", details) : i18nc("@info diagnostic timeline", "Reconnected"));
    if (!reconnecting) {
        return;
    }

    for (Row *r : std::as_const(m_rows)) {
        setRowState(r, State::Hidden);
    }
    Row *r = row(ConnectionError::Stage::ConnectionLost);
    if (!r) {
        r = addRow(ConnectionError::Stage::ConnectionLost);
    }
    setRowState(r, State::Active);
    r->body->setText(i18nc("@info", "KRDC keeps trying until the connection is back."));
    r->body->setForegroundRole(QPalette::WindowText);
    r->body->show();

    m_heading->setText(i18nc("@title", "Connection to %1 interrupted", m_url.host()));
    clearButtons();
    addButton(i18nc("@action:button", "Disconnect"), QStringLiteral("network-disconnect"), &ConnectionStatusWidget::cancelRequested);
    settleLayout();
}

void ConnectionStatusWidget::setError(const ConnectionError &error)
{
    using Stage = ConnectionError::Stage;

    m_hasError = true;
    m_error = error;
    log(i18nc("@info diagnostic timeline", "Failed while: %1", ConnectionError::stageName(error.stage)));
    if (m_lookupId >= 0) {
        QHostInfo::abortHostLookup(m_lookupId);
        m_lookupId = -1;
    }

    const bool ended = error.stage == Stage::SessionEnded;
    const bool afterConnecting = ended || error.stage == Stage::ConnectionLost;

    Row *failed = nullptr;
    if (afterConnecting) {
        for (Row *r : std::as_const(m_rows)) {
            setRowState(r, State::Hidden);
        }
        failed = row(error.stage);
        if (!failed) {
            failed = addRow(error.stage);
        }
    } else if (error.stage == Stage::Unknown) {
        for (Row *r : std::as_const(m_rows)) {
            if (r->state == State::Active) {
                failed = r;
            }
        }
        if (!failed) {
            failed = addRow(Stage::Unknown);
        }
    } else {
        failed = row(error.stage);
        if (!failed) {
            // Stages without a progress row, such as Security, appear only when they fail.
            int position = m_rows.size();
            for (int i = 0; i < m_rows.size(); ++i) {
                if (static_cast<int>(m_rows.at(i)->stage) > static_cast<int>(error.stage)) {
                    position = i;
                    break;
                }
            }
            failed = addRow(error.stage, position);
        }
    }

    if (!afterConnecting) {
        // Tick only steps with evidence; later steps never ran.
        bool afterFailed = false;
        for (Row *r : std::as_const(m_rows)) {
            if (r == failed) {
                afterFailed = true;
            } else if (afterFailed || !observed(r->stage, error.stage)) {
                setRowState(r, State::Hidden);
            } else {
                setRowState(r, State::Done);
            }
        }
    }

    setRowState(failed, State::Failed);
    if (ended) {
        failed->icon->setKind(StageIcon::Kind::Notice);
    }
    failed->title->setText(error.title);
    failed->body->setText(error.message);
    failed->body->setForegroundRole(QPalette::WindowText);
    failed->body->setVisible(!error.message.isEmpty());

    m_guidance->setText(error.steps.isEmpty() ? QString() : htmlList(error.steps));
    m_guidance->setVisible(!error.steps.isEmpty());

    // Only what the page does not already say; the copied diagnostics have everything.
    QStringList details;
    if (!error.details.isEmpty()) {
        details << error.details;
    }
    m_technicalDetails->setText(details.join(QLatin1Char('\n')));
    updateLinks();

    failed->textLayout->addWidget(m_errorExtras);
    m_extrasLayout->insertWidget(1, m_buttons); // after the steps to try, before the links
    m_errorExtras->show();

    if (ended) {
        m_heading->setText(i18nc("@title", "Session with %1 ended", m_url.host()));
    } else if (error.stage == Stage::ConnectionLost) {
        m_heading->setText(i18nc("@title", "Connection to %1 lost", m_url.host()));
    } else {
        m_heading->setText(i18nc("@title", "Could not connect to %1", m_url.host()));
    }

    clearButtons();
    QPushButton *defaultButton = nullptr;
    if (error.actions & ConnectionError::Retry) {
        defaultButton = addButton(afterConnecting ? i18nc("@action:button", "Reconnect") : i18nc("@action:button", "Connect Again"),
                                  QStringLiteral("view-refresh"),
                                  &ConnectionStatusWidget::retryRequested);
    }
    if (error.actions & ConnectionError::EditAddress) {
        addButton(i18nc("@action:button", "Edit Address"), QStringLiteral("document-edit"), &ConnectionStatusWidget::editAddressRequested);
    }
    if (error.actions & ConnectionError::EditSettings) {
        addButton(i18nc("@action:button", "Edit Host Settings…"), QStringLiteral("configure"), &ConnectionStatusWidget::editSettingsRequested);
    }
    addButton(i18nc("@action:button", "Close Tab"), QStringLiteral("tab-close"), &ConnectionStatusWidget::closeRequested);
    if (defaultButton) {
        defaultButton->setDefault(true);
        defaultButton->setFocus();
    }
    settleLayout();
}

void ConnectionStatusWidget::settleLayout()
{
    // Qt settles nested layouts inside out over several event loop passes, and a frame
    // painted in between shows sections stretched to their old size. Invalidate all
    // layouts, then activate them outside in, now.
    const std::function<void(QWidget *, bool)> visit = [&visit](QWidget *widget, bool activate) {
        if (QLayout *layout = widget->layout()) {
            if (activate) {
                layout->activate();
            } else {
                layout->invalidate();
            }
        }
        for (QObject *child : widget->children()) {
            auto *childWidget = qobject_cast<QWidget *>(child);
            if (childWidget && !childWidget->isWindow() && childWidget->isVisible()) {
                visit(childWidget, activate);
            }
        }
    };
    visit(m_content, false);
    visit(m_content, true);
    // Let the scroll area adopt the new content size now; only this widget's queued requests.
    for (QObject *receiver : {static_cast<QObject *>(m_content), static_cast<QObject *>(m_scrollArea->viewport()), static_cast<QObject *>(m_scrollArea)}) {
        QCoreApplication::sendPostedEvents(receiver, QEvent::LayoutRequest);
    }
}

void ConnectionStatusWidget::updateLinks()
{
    const QString copy = QStringLiteral("<a href=\"copy\">%1</a>").arg(i18nc("@action:button", "Copy diagnostic information"));
    if (m_technicalDetails->text().isEmpty()) {
        m_links->setText(copy);
        return;
    }
    const QString details = m_details->isVisible() ? i18nc("@action:button", "Hide details") : i18nc("@action:button", "Show details");
    m_links->setText(QStringLiteral("<a href=\"details\">%1</a> · %2").arg(details, copy));
}

void ConnectionStatusWidget::clearButtons()
{
    while (QLayoutItem *item = m_buttonsLayout->takeAt(0)) {
        delete item->widget();
        delete item;
    }
}

QPushButton *ConnectionStatusWidget::addButton(const QString &text, const QString &icon, void (ConnectionStatusWidget::*signal)())
{
    auto *button = new QPushButton(QIcon::fromTheme(icon), text, this);
    connect(button, &QPushButton::clicked, this, signal);
    m_buttonsLayout->addWidget(button);
    return button;
}

void ConnectionStatusWidget::log(const QString &event)
{
    m_timeline << QStringLiteral("+%1s  %2").arg(m_elapsed.elapsed() / 1000.0, 0, 'f', 2).arg(event);
}

QString ConnectionStatusWidget::diagnostics() const
{
    QStringList lines;
    lines << QStringLiteral("KRDC %1 connection diagnostics").arg(QStringLiteral(KRDC_VERSION_STRING));
    lines << QStringLiteral("Date: %1").arg(QDateTime::currentDateTime().toString(Qt::ISODate));
    lines << QStringLiteral("System: %1").arg(QSysInfo::prettyProductName());
    lines << QStringLiteral("Address: %1").arg(displayUrl(m_url));
    lines << QStringLiteral("Protocol: %1").arg(m_url.scheme());
    lines << QStringLiteral("SSH tunnel: %1").arg(m_sshTunnel ? QStringLiteral("yes") : QStringLiteral("no"));
    lines << QStringLiteral("Resolved address: %1").arg(m_resolvedAddress.isEmpty() ? QStringLiteral("unknown") : m_resolvedAddress);
    if (m_hasError) {
        lines << QStringLiteral("Failed stage: %1").arg(ConnectionError::stageName(m_error.stage));
        lines << QStringLiteral("Error: %1 - %2").arg(m_error.title, m_error.message);
        if (!m_error.details.isEmpty()) {
            lines << QStringLiteral("Details: %1").arg(m_error.details);
        }
    }
    lines << QStringLiteral("Timeline:");
    for (const QString &entry : m_timeline) {
        lines << QStringLiteral("  ") + entry;
    }
    return lines.join(QLatin1Char('\n'));
}
