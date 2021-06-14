/*
    SPDX-FileCopyrightText: 2009 Urs Wolfer <uwolfer@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "konsoleview.h"

#include <KParts/Part>
#include <KParts/ReadOnlyPart>
#include <KPluginFactory>
#include <KPluginLoader>
#include <KService>
#include <kde_terminal_interface.h>

#include <QDir>
#include <QEvent>
#include <QScrollArea>
#include <QVBoxLayout>

KonsoleView::KonsoleView(QWidget *parent, const QUrl &url, KConfigGroup configGroup)
        : RemoteView(parent)
{
    m_url = url;
    m_host = url.host();
    m_port = url.port();

    m_hostPreferences = new KonsoleHostPreferences(configGroup, this);

//     QSize size = QSize(640, 480);
    const QSize size = (qobject_cast<QWidget *>(parent))->size();
    setStatus(Connected);
    setFixedSize(size);
    setFixedSize(size);
    Q_EMIT framebufferSizeChanged(size.width(), size.height());

    KPluginFactory* factory = 0;
    KService::Ptr service = KService::serviceByDesktopName("konsolepart");
    if (service) {
        factory = KPluginLoader(service->library()).factory();
    }
    KParts::ReadOnlyPart* part = factory ? (factory->create<KParts::ReadOnlyPart>(this)) : 0;
    if (part != 0) {
//         connect(part, SIGNAL(destroyed(QObject*)), this, SLOT(terminalExited()));
        QVBoxLayout *mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->setSpacing(0);
        m_terminalWidget = part->widget();
        mainLayout->addWidget(m_terminalWidget);
        m_terminal = qobject_cast<TerminalInterface *>(part);
        m_terminal->showShellInDir(QDir::homePath());
        m_terminal->sendInput("echo " + url.userName() + '@' + url.host()/* + ':' + url.port()*/ + '\n');
//         m_terminal->sendInput("clear\n");
        m_terminalWidget->resize(size);
    }
}

KonsoleView::~KonsoleView()
{
    Q_EMIT disconnected();
    setStatus(Disconnected);
}

bool KonsoleView::eventFilter(QObject *obj, QEvent *event)
{
    if (m_viewOnly) {
        if (event->type() == QEvent::KeyPress ||
                event->type() == QEvent::KeyRelease ||
                event->type() == QEvent::MouseButtonDblClick ||
                event->type() == QEvent::MouseButtonPress ||
                event->type() == QEvent::MouseButtonRelease ||
                event->type() == QEvent::MouseMove)
            return true;
    }
    return RemoteView::eventFilter(obj, event);
}

QSize KonsoleView::framebufferSize()
{
    return minimumSizeHint();
}

QSize KonsoleView::sizeHint() const
{
    return RemoteView::sizeHint();
    return maximumSize();
}

bool KonsoleView::isQuitting()
{
    return false;
}

bool KonsoleView::start()
{
    setStatus(Connected);
    Q_EMIT connected();
    m_terminalWidget->setFocus();
    return true;
}

HostPreferences* KonsoleView::hostPreferences()
{
    return m_hostPreferences;
}

void KonsoleView::switchFullscreen(bool on)
{
    Q_UNUSED(on);
}

