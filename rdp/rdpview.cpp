/****************************************************************************
**
** Copyright (C) 2002 Arend van Beelen jr. <arend@auton.nl>
** Copyright (C) 2007 Urs Wolfer <uwolfer @ kde.org>
**
** This file is part of KDE.
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; see the file COPYING. If not, write to
** the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
** Boston, MA 02110-1301, USA.
**
****************************************************************************/

#include "rdpview.h"

#include "settings.h"

#include <KInputDialog>
#include <KMessageBox>
#include <KPasswordDialog>
#include <KShell>

#include <QX11EmbedContainer>
#include <QEvent>

RdpView::RdpView(QWidget *parent,
                 const KUrl &url,
                 KConfigGroup configGroup,
                 const QString &user, const QString &password,
                 int flags, const QString &domain,
                 const QString &shell, const QString &directory)
        : RemoteView(parent),
        m_user(user),
        m_password(password),
        m_flags(flags),
        m_domain(domain),
        m_shell(shell),
        m_directory(directory),
        m_quitFlag(false),
        m_process(NULL)
{
    m_url = url;
    m_host = url.host();
    m_port = url.port();

    if (m_port <= 0) {
        m_port = TCP_PORT_RDP;
    }

    m_container = new QX11EmbedContainer(this);
    m_container->installEventFilter(this);
    
    m_hostPreferences = new RdpHostPreferences(configGroup, this);
}

RdpView::~RdpView()
{
    startQuitting();
}

// filter out key and mouse events to the container if we are view only
//FIXME: X11 events are passed to the app before getting caught in the Qt event processing
bool RdpView::eventFilter(QObject *obj, QEvent *event)
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

QSize RdpView::framebufferSize()
{
    return m_container->minimumSizeHint();
}

QSize RdpView::sizeHint() const
{
    return maximumSize();
}

void RdpView::startQuitting()
{
    kDebug(5012) << "About to quit";
    m_quitFlag = true;
    if (m_process) {
        m_process->terminate();
        m_process->waitForFinished(1000);
        m_container->discardClient();
    }
}

bool RdpView::isQuitting()
{
    return m_quitFlag;
}

bool RdpView::start()
{
    m_container->show();

    if (m_hostPreferences->walletSupport()) {
        if (m_url.userName().isEmpty()) {
            QString userName;
            bool ok = false;

            userName = KInputDialog::getText(i18n("Enter Username"),
                                             i18n("Please enter the username you would like to use for login."),
                                             Settings::defaultRdpUserName(), &ok, this);

            if (ok)
                m_url.setUserName(userName);
        }

        if (!m_url.userName().isEmpty()) {
            const bool useLdapLogin = Settings::recognizeLdapLogins() && m_url.userName().contains('\\');
            kDebug(5012) << "Is LDAP login:" << useLdapLogin << m_url.userName();
            QString walletPassword = readWalletPassword(useLdapLogin);

            if (!walletPassword.isNull())
                m_url.setPassword(walletPassword);
            else {
                KPasswordDialog dialog(this);
                dialog.setPrompt(i18n("Access to the system requires a password."));
                if (dialog.exec() == KPasswordDialog::Accepted) {
                    m_url.setPassword(dialog.password());

                    if (m_hostPreferences->walletSupport())
                        saveWalletPassword(dialog.password(), useLdapLogin);
                }
            }
        }
    }

    m_process = new QProcess(m_container);

    QStringList arguments;
    arguments << "-g" << (QString::number(m_hostPreferences->width()) + 'x' +
                          QString::number(m_hostPreferences->height()));
    arguments << "-k" << m_hostPreferences->keyboardLayout();

    if (!m_url.userName().isEmpty())
        arguments << "-u" << m_url.userName();
    else
        arguments << "-u" << "";

    if (!m_url.password().isNull())
        arguments << "-p" << m_url.password();

    arguments << "-D";  // request the window has no decorations
    arguments << "-X" << QString::number(m_container->winId());
    arguments << "-a" << QString::number((m_hostPreferences->colorDepth() + 1) * 8);

    QString sound;
    switch (m_hostPreferences->sound()) {
    case 0:
        sound = "local";
        break;
    case 1:
        sound = "remote";
        break;
    case 2:
    default:
        sound = "off";
    }
    arguments << "-r" << "sound:" + sound;

    if (m_hostPreferences->console()) {
        arguments << "-0";
    }

    if (!m_hostPreferences->extraOptions().isEmpty()) {
        const QStringList additionalArguments = KShell::splitArgs(m_hostPreferences->extraOptions());
        arguments += additionalArguments;
    }

    arguments << (m_host + ':' + QString::number(m_port));

    setStatus(Connecting);

    connect(m_process, SIGNAL(error(QProcess::ProcessError)), SLOT(processError(QProcess::ProcessError)));
    connect(m_process, SIGNAL(readyReadStandardError()), SLOT(receivedStandardError()));
    connect(m_container, SIGNAL(clientClosed()), SLOT(connectionClosed()));
    connect(m_container, SIGNAL(clientIsEmbedded()), SLOT(connectionOpened()));

    m_process->start("rdesktop", arguments);

    return true;
}

HostPreferences* RdpView::hostPreferences()
{
    return m_hostPreferences;
}

void RdpView::switchFullscreen(bool on)
{
    if (on == true) {
        m_container->grabKeyboard();
    }
}

void RdpView::connectionOpened()
{
    kDebug(5012) << "Connection opened";
    const QSize size = m_container->minimumSizeHint();
    kDebug(5012) << "Size hint: " << size.width() << " " << size.height();
    setStatus(Connected);
    setFixedSize(size);
    resize(size);
    m_container->setFixedSize(size);
    emit framebufferSizeChanged(size.width(), size.height());
    emit connected();
    setFocus();
}

QPixmap RdpView::takeScreenshot()
{
    return QPixmap::grabWindow(m_container->clientWinId());
}

void RdpView::connectionClosed()
{
    emit disconnected();
    setStatus(Disconnected);
    m_quitFlag = true;
}

void RdpView::processError(QProcess::ProcessError error)
{
    if (m_quitFlag) // do not try to show error messages while quitting (prevent crashes)
        return;

    if (m_status == Connecting) {
        setStatus(Disconnected);

        if (error == QProcess::FailedToStart) {
            KMessageBox::error(0, i18n("Could not start \"rdesktop\"; make sure rdesktop is properly installed."),
                               i18n("RDP Failure"));
            return;
        }

        if (m_clientVersion.isEmpty()) {
            KMessageBox::error(0, i18n("Connection attempt to host failed."),
                               i18n("Connection Failure"));
        } else {
            KMessageBox::error(0, i18n("The version of \"rdesktop\" you are using (%1) is too old:\n"
                                       "rdesktop 1.3.2 or greater is required.", m_clientVersion),
                               i18n("RDP Failure"));
        }
        emit disconnectedError();
    }
}

void RdpView::receivedStandardError()
{
    const QString output(m_process->readAllStandardError());
    QString line;
    int i = 0;
    while (!(line = output.section('\n', i, i)).isEmpty()) {
        if (line.startsWith(QLatin1String("Version "))) {
            m_clientVersion = line.section(' ', 1, 1);
            m_clientVersion = m_clientVersion.left(m_clientVersion.length() - 1);
            return;
        } else {
            kDebug(5012) << "Process error output: " << line;
        }
        i++;
    }
}

void RdpView::setGrabAllKeys(bool grabAllKeys)
{
    Q_UNUSED(grabAllKeys);
    // do nothing.. grabKeyboard seems not to be supported in QX11EmbedContainer
}

#include "rdpview.moc"
