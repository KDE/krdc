/*
    SPDX-FileCopyrightText: 2002 Arend van Beelen jr. <arend@auton.nl>
    SPDX-FileCopyrightText: 2007-2012 Urs Wolfer <uwolfer@kde.org>
    SPDX-FileCopyrightText: 2012 AceLan Kao <acelan@acelan.idv.tw>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "rdpview.h"

#include "rdphostpreferences.h"
#include "settings.h"
#include "krdc_debug.h"

#include <KMessageBox>
#include <KPasswordDialog>
#include <KShell>
#include <KWindowSystem>

#include <QScreen>
#include <QWindow>
#include <QDir>
#include <QEvent>
#include <QInputDialog>
#include <QPainter>
#include <QKeyEvent>
#include <QMouseEvent>

#include "rdpsession.h"

RdpView::RdpView(QWidget *parent,
                 const QUrl &url,
                 KConfigGroup configGroup,
                 const QString &user, const QString &password)
        : RemoteView(parent),
        m_user(user),
        m_password(password)
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

    resize(1920, 1080);
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

QSize RdpView::sizeHint() const
{
    if (m_session) {
        return m_session->size();
    }

    return QSize{};
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
    m_session->setHost(m_host);
    m_session->setPort(m_port);
    m_session->setUser(m_user);

    if (m_password.isEmpty()) {
        m_session->setPassword(readWalletPassword());
    } else {
        m_session->setPassword(m_password);
    }

    connect(m_session.get(), &RdpSession::sizeChanged, this, [this]() {
        resize(m_session->size());
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

    setStatus(RdpView::Connecting);
    return m_session->start();
}

HostPreferences* RdpView::hostPreferences()
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
    if (!m_pendingData.isNull()) {
        return QPixmap::fromImage(m_pendingData);
    }
    return QPixmap{};
}

void RdpView::setGrabAllKeys(bool grabAllKeys)
{
    if (grabAllKeys) {
        setFocus();
    } else {
        clearFocus();
    }
}

void RdpView::savePassword(const QString& password)
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

    painter.drawImage(QPoint{0, 0}, image);
    painter.end();
}

void RdpView::keyPressEvent(QKeyEvent *event)
{
    m_session->sendEvent(event, this);
}

void RdpView::keyReleaseEvent(QKeyEvent *event)
{
    m_session->sendEvent(event, this);
}

void RdpView::mousePressEvent(QMouseEvent *event)
{
    m_session->sendEvent(event, this);
}

void RdpView::mouseReleaseEvent(QMouseEvent *event)
{
    m_session->sendEvent(event, this);
}

void RdpView::mouseMoveEvent(QMouseEvent *event)
{
    m_session->sendEvent(event, this);
}

void RdpView::onRectangleUpdated(const QRect &rect)
{
    m_pendingRectangle = rect;
    update();
}
