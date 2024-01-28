/*
    SPDX-FileCopyrightText: 2002 Arend van Beelen jr. <arend@auton.nl>
    SPDX-FileCopyrightText: 2007-2012 Urs Wolfer <uwolfer@kde.org>
    SPDX-FileCopyrightText: 2012 AceLan Kao <acelan@acelan.idv.tw>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "rdpview.h"

#include "krdc_debug.h"
#include "rdphostpreferences.h"

#include <KMessageBox>
#include <KPasswordDialog>
#include <KShell>
#include <KWindowSystem>

#include <QDir>
#include <QEvent>
#include <QInputDialog>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>
#include <QWindow>

#include "rdpsession.h"

RdpView::RdpView(QWidget *parent, const QUrl &url, KConfigGroup configGroup, const QString &user, const QString &password)
    : RemoteView(parent)
    , m_user(user)
    , m_password(password)
{
    m_url = url;
    m_host = url.host();
    m_port = url.port();

    if (m_user.isEmpty() && !m_url.userName().isEmpty()) {
        m_user = m_url.userName();
    }

    if (m_password.isEmpty() && !m_url.password().isEmpty()) {
        m_password = m_url.password();
    }

    if (m_port <= 0) {
        m_port = TCP_PORT_RDP;
    }

    setMouseTracking(true);

    m_hostPreferences = std::make_unique<RdpHostPreferences>(configGroup);
}

RdpView::~RdpView()
{
    startQuitting();
}

QSize RdpView::framebufferSize()
{
    if (m_session) {
        return m_session->size();
    }

    return QSize{};
}

void RdpView::scaleResize(int w, int h)
{
    RemoteView::scaleResize(w, h);

    // handle window resizes
    resize(sizeHint());
}

QSize RdpView::sizeHint() const
{
    if (!m_session) {
        return QSize{};
    }

    // when parent is resized and scaling is enabled, resize the view, preserving aspect ratio
    if (m_hostPreferences->scaleToSize()) {
        return m_session->size().scaled(parentWidget()->size(), Qt::KeepAspectRatio);
    }

    return m_session->size();
}

void RdpView::startQuitting()
{
    m_quitting = true;
    m_session->stop();
}

bool RdpView::isQuitting()
{
    return m_quitting;
}

bool RdpView::start()
{
    m_session = std::make_unique<RdpSession>(this);
    m_session->setHostPreferences(m_hostPreferences.get());
    m_session->setHost(m_host);
    m_session->setPort(m_port);
    m_session->setUser(m_user);
    m_session->setSize(initialSize());

    if (m_password.isEmpty()) {
        m_session->setPassword(readWalletPassword());
    } else {
        m_session->setPassword(m_password);
    }

    connect(m_session.get(), &RdpSession::sizeChanged, this, [this]() {
        resize(sizeHint());
        qCDebug(KRDC) << "freerdp resized rdp view" << sizeHint();
        Q_EMIT framebufferSizeChanged(width(), height());
    });
    connect(m_session.get(), &RdpSession::rectangleUpdated, this, &RdpView::onRectangleUpdated);
    connect(m_session.get(), &RdpSession::stateChanged, this, [this]() {
        switch (m_session->state()) {
        case RdpSession::State::Starting:
            setStatus(Authenticating);
            break;
        case RdpSession::State::Connected:
            setStatus(Preparing);
            break;
        case RdpSession::State::Running:
            setStatus(Connected);
            break;
        case RdpSession::State::Closed:
            setStatus(Disconnected);
            break;
        default:
            break;
        }
    });
    connect(m_session.get(), &RdpSession::errorMessage, this, [this](const QString &title, const QString &message) {
        KMessageBox::error(this, message, title);
    });

    setStatus(RdpView::Connecting);
    if (!m_session->start()) {
        Q_EMIT disconnected();
        return false;
    }

    setFocus();

    return true;
}

HostPreferences *RdpView::hostPreferences()
{
    return m_hostPreferences.get();
}

void RdpView::switchFullscreen(bool on)
{
    if (on) {
        showFullScreen();
    } else {
        showNormal();
    }
}

QPixmap RdpView::takeScreenshot()
{
    if (!m_session->videoBuffer()->isNull()) {
        return QPixmap::fromImage(*m_session->videoBuffer());
    }
    return QPixmap{};
}

bool RdpView::supportsScaling() const
{
    return true;
}

bool RdpView::scaling() const
{
    return m_hostPreferences->scaleToSize();
}

void RdpView::enableScaling(bool scale)
{
    m_hostPreferences->setScaleToSize(scale);
    qCDebug(KRDC) << "Scaling changed" << scale;
    resize(sizeHint());
    update();
}

void RdpView::setScaleFactor(float factor)
{
}

QSize RdpView::initialSize()
{
    switch (m_hostPreferences->resolution()) {
    case RdpHostPreferences::Resolution::Small:
        return QSize{1280, 720};
    case RdpHostPreferences::Resolution::Medium:
        return QSize{1600, 900};
    case RdpHostPreferences::Resolution::Large:
        return QSize{1920, 1080};
    case RdpHostPreferences::Resolution::MatchWindow:
        return parentWidget()->size();
    case RdpHostPreferences::Resolution::MatchScreen:
        return window()->windowHandle()->screen()->size();
    case RdpHostPreferences::Resolution::Custom:
        return QSize{m_hostPreferences->width(), m_hostPreferences->height()};
    }

    return parentWidget()->size();
}

void RdpView::savePassword(const QString &password)
{
    saveWalletPassword(password);
}

void RdpView::paintEvent(QPaintEvent *event)
{
    if (m_session->videoBuffer()->isNull()) {
        return;
    }

    QPainter painter;

    painter.begin(this);
    painter.setClipRect(event->rect());

    auto image = *m_session->videoBuffer();

    if (m_hostPreferences->scaleToSize()) {
        painter.drawImage(QPoint{0, 0}, image.scaled(size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else {
        painter.drawImage(QPoint{0, 0}, image);
    }
    painter.end();
}

void RdpView::keyPressEvent(QKeyEvent *event)
{
    m_session->sendEvent(event, this);
    event->accept();
}

void RdpView::keyReleaseEvent(QKeyEvent *event)
{
    m_session->sendEvent(event, this);
    event->accept();
}

void RdpView::mousePressEvent(QMouseEvent *event)
{
    if (!hasFocus()) {
        setFocus();
    }

    m_session->sendEvent(event, this);
    event->accept();
}

void RdpView::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (!hasFocus()) {
        setFocus();
    }

    m_session->sendEvent(event, this);
    event->accept();
}

void RdpView::mouseReleaseEvent(QMouseEvent *event)
{
    m_session->sendEvent(event, this);
    event->accept();
}

void RdpView::mouseMoveEvent(QMouseEvent *event)
{
    m_session->sendEvent(event, this);
    event->accept();
}

void RdpView::wheelEvent(QWheelEvent *event)
{
    m_session->sendEvent(event, this);
    event->accept();
}

void RdpView::onRectangleUpdated(const QRect &rect)
{
    m_pendingRectangle = rect;
    update();
}
