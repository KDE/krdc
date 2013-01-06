/****************************************************************************
**
** Copyright (C) 2002 Arend van Beelen jr. <arend@auton.nl>
** Copyright (C) 2007 - 2012 Urs Wolfer <uwolfer @ kde.org>
** Copyright (C) 2012 AceLan Kao <acelan @ acelan.idv.tw>
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

    int width, height;
    if (m_hostPreferences->width() > 0) {
        width = m_hostPreferences->width();
        height = m_hostPreferences->height();
    } else {
        width = this->parentWidget()->size().width();
        height = this->parentWidget()->size().height();
    }
    arguments << "-g" << QString::number(width) + 'x' + QString::number(height);

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

    switch (m_hostPreferences->sound()) {
    case 1:
        arguments << "-o";
        break;
    case 0:
        arguments << "--plugin" << "rdpsnd";
        break;
    case 2:
    default:
        break;
    }

    if (!m_hostPreferences->shareMedia().isEmpty()) {
        QStringList shareMedia;
        shareMedia << "--plugin" << "rdpdr" << "--data" << "disk:media:" + m_hostPreferences->shareMedia() << "--";
        arguments += shareMedia;
    }

    QString performance;
    switch (m_hostPreferences->performance()) {
    case 0:
        performance = 'm';
        break;
    case 1:
        performance = 'b';
        break;
    case 2:
        performance = 'l';
        break;
    default:
        break;
    }

    arguments << "-x" << performance;

    if (m_hostPreferences->console()) {
        arguments << "-0";
    }

    if (m_hostPreferences->remoteFX()) {
        arguments << "--rfx";
    }

    if (!m_hostPreferences->extraOptions().isEmpty()) {
        const QStringList additionalArguments = KShell::splitArgs(m_hostPreferences->extraOptions());
        arguments += additionalArguments;
    }

    // krdc has no support for certificate management yet; it would not be possbile to connect to any host:
    // "The host key for example.com has changed" ...
    // "Add correct host key in ~/.freerdp/known_hosts to get rid of this message."
    arguments << "--ignore-certificate";

    // clipboard sharing is activated in KRDC; user can disable it at runtime
    arguments << "--plugin" << "cliprdr";

    arguments << (m_host + ':' + QString::number(m_port));

    kDebug(5012) << "Starting xfreerdp with arguments:" << arguments;

    setStatus(Connecting);

    connect(m_process, SIGNAL(error(QProcess::ProcessError)), SLOT(processError(QProcess::ProcessError)));
    connect(m_process, SIGNAL(readyReadStandardError()), SLOT(receivedStandardError()));
    connect(m_process, SIGNAL(readyReadStandardOutput()), SLOT(receivedStandardOutput()));
    connect(m_container, SIGNAL(clientClosed()), SLOT(connectionClosed()));
    connect(m_container, SIGNAL(clientIsEmbedded()), SLOT(connectionOpened()));

    m_process->start("xfreerdp", arguments);

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

void RdpView::connectionError()
{
    emit disconnectedError();
    connectionClosed();
}

void RdpView::processError(QProcess::ProcessError error)
{
    kDebug(5012) << "processError:" << error;
    if (m_quitFlag) // do not try to show error messages while quitting (prevent crashes)
        return;

    if (m_status == Connecting) {
        if (error == QProcess::FailedToStart) {
            KMessageBox::error(0, i18n("Could not start \"xfreerdp\"; make sure xfreerdp is properly installed."),
                               i18n("RDP Failure"));
            connectionError();
            return;
        }
    }
}

void RdpView::receivedStandardError()
{
    const QString output(m_process->readAllStandardError());
    kDebug(5012) << "receivedStandardError:" << output;
    QString line;
    int i = 0;
    while (!(line = output.section('\n', i, i)).isEmpty()) {
        
        // the following error is issued by freerdp because of a bug in freerdp 1.0.1 and below;
        // see: https://github.com/FreeRDP/FreeRDP/pull/576
        //"X Error of failed request:  BadWindow (invalid Window parameter)
        //   Major opcode of failed request:  7 (X_ReparentWindow)
        //   Resource id in failed request:  0x71303348
        //   Serial number of failed request:  36
        //   Current serial number in output stream:  36"
        if (line.contains(QLatin1String("X_ReparentWindow"))) {
            KMessageBox::error(0, i18n("The version of \"xfreerdp\" you are using is too old.\n"
                                       "xfreerdp 1.0.2 or greater is required."),
                               i18n("RDP Failure"));
            connectionError();
            return;
        }
        i++;
    }
}

void RdpView::receivedStandardOutput()
{
    const QString output(m_process->readAllStandardOutput());
    kDebug(5012) << "receivedStandardOutput:" << output;
    QString line;
    int i = 0;
    while (!(line = output.section('\n', i, i)).isEmpty()) {

        // full xfreerdp message: "transport_connect: getaddrinfo (Name or service not known)"
        if (line.contains(QLatin1String("Name or service not known"))) {
            KMessageBox::error(0, i18n("Name or service not known."),
                               i18n("Connection Failure"));
            connectionError();
            return;

        // full xfreerdp message: "unable to connect to example.com:3389"
        } else if (line.contains(QLatin1String("unable to connect to"))) {
            KMessageBox::error(0, i18n("Connection attempt to host failed."),
                               i18n("Connection Failure"));
            connectionError();
            return;

        // looks like some generic xfreerdp error message, handle it if nothing was handled:
        // "Error: protocol security negotiation failure"
        } else if (line.contains(QLatin1String("Error: protocol security negotiation failure"))) {
            KMessageBox::error(0, i18n("Connection attempt to host failed."),
                               i18n("Connection Failure"));
            connectionError();
            return;
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
