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
** You should have received a copy of the GNU Library General Public License
** along with this library; see the file COPYING.LIB. If not, write to
** the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
** Boston, MA 02110-1301, USA.
**
****************************************************************************/

#include <kdialog.h>
#include <klocale.h>
#include <kmessagebox.h>
#include <kwallet.h>
#include <kpassworddialog.h>
#include <kvbox.h>

#include <QX11EmbedContainer>
#include <QEvent>

#undef Bool

#include "rdpview.h"
// #include "rdphostpref.h"
// #include "rdpprefs.h"

bool rdpAppDataConfigured = false;
extern KWallet::Wallet *wallet;

static RdpView *krdpview;

RdpView::RdpView(QWidget *parent,
                   const QString &host, int port,
                   const QString &user, const QString &password,
                   int flags, const QString &domain,
                   const QString &shell, const QString &directory,
                   const QString &caption)
  : RemoteView(parent),
    m_user(user),
    m_password(password),
    m_flags(flags),
    m_domain(domain),
    m_shell(shell),
    m_directory(directory),
    m_quitFlag(false),
    m_process(NULL),
    m_caption(caption),
    m_viewOnly(true)
{
    m_host = host;
    m_port = port;

    krdpview = this;
    if(m_port <= 0) {
        m_port = TCP_PORT_RDP;
    }

    m_container = new QX11EmbedContainer(this);
    m_container->installEventFilter(this);
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

// returns the size of the framebuffer
QSize RdpView::framebufferSize()
{
    return m_container->minimumSizeHint();
}

// returns the suggested size
QSize RdpView::sizeHint() const
{
    return maximumSize();
}

// start closing the connection
void RdpView::startQuitting()
{
    kDebug(5012) << "About to quit" << endl;
    m_quitFlag = true;
    if(m_process != NULL) {
        m_container->discardClient();
        // FIXME: we proably need to kill the rdesktop process here.
    }
}

// are we currently closing the connection?
bool RdpView::isQuitting()
{
    return m_quitFlag;
}

// open a connection
bool RdpView::start()
{
#if 0
    SmartPtr<RdpHostPref> hp, rdpDefaults;
    bool useKWallet = false;

    if(!rdpAppDataConfigured) {
        HostPreferences *hps = HostPreferences::instance();

        hp = SmartPtr<RdpHostPref>(hps->createHostPref(m_host, RdpHostPref::RdpType));
        int wv = hp->width();
        int hv = hp->height();
        int cd = hp->colorDepth();
        QString kl = hp->layout();
        bool kwallet = hp->useKWallet();
        if(hp->askOnConnect()) {
            // show preferences dialog
            KDialog *dlg = new KDialog(this);
            dlg->setObjectName("rdpPrefDlg");
            dlg->setModal(true);
            dlg->setCaption(i18n("RDP Host Preferences for %1", m_host));
            dlg->setButtons(KDialog::Ok | KDialog::Cancel);
            dlg->setDefaultButton(KDialog::Ok);
            dlg->showButtonSeparator(true);

            KVBox *vbox = new KVBox(this);
            dlg->setMainWidget(vbox);

            RdpPrefs *prefs = new RdpPrefs(vbox);
            QWidget *spacer = new QWidget(vbox);
            vbox->setStretchFactor(spacer, 10);

            prefs->setRdpWidth(wv);
            prefs->setRdpHeight(hv);
            prefs->setResolution();
            prefs->setColorDepth(cd);
            prefs->setKbLayout(keymap2int(kl));
            prefs->setShowPrefs(true);
            prefs->setUseKWallet(kwallet);

            if (dlg->exec() == QDialog::Rejected)
                return false;

            wv = prefs->rdpWidth();
            hv = prefs->rdpHeight();
            kl = int2keymap(prefs->kbLayout());
            hp->setAskOnConnect(prefs->showPrefs());
            hp->setWidth(wv);
            hp->setHeight(hv);
            hp->setColorDepth(prefs->colorDepth());
            hp->setLayout(kl);
            hp->setUseKWallet(prefs->useKWallet());
            hps->sync();
        }

        useKWallet = hp->useKWallet();
    }
#endif

    m_container->show();
    m_container->setWindowTitle(m_caption);

    m_process = new QProcess(m_container);

    QStringList arguments;
//     arguments << "-g" << (QString::number(hp->width()) + 'x' + QString::number(hp->height()));
//     arguments << "-k" << hp->layout();

    if(!m_user.isEmpty()) {
        arguments << "-u" << m_user;
    }

#if 0
    if(m_password.isEmpty() && useKWallet) {
        QString krdc_folder = "KRDC-RDP";

        // Bugfix: Check if wallet has been closed by an outside source
        if (wallet && !wallet->isOpen()) {
            delete wallet;
            wallet = 0;
        }

        // Do we need to open the wallet?
        if (!wallet) {
            QString walletName = KWallet::Wallet::NetworkWallet();
            wallet = KWallet::Wallet::openWallet(walletName, topLevelWidget()->winId());
        }

        if (wallet && wallet->isOpen()) {
            bool walletOK = wallet->hasFolder(krdc_folder);
            if (walletOK == false) {
                walletOK = wallet->createFolder(krdc_folder);
            }

            if (walletOK == true) {
                wallet->setFolder(krdc_folder);
                if (wallet->hasEntry(m_host)) {
                    wallet->readPassword(m_host, m_password);
                }
            }

            if (m_password.isEmpty()) {
                //There must not be an existing entry. Let's make one.
                KPasswordDialog dlg(this);
                dlg.setPrompt(i18n("Please enter the password."));
                if (dlg.exec() == KPasswordDialog::Accepted) {
                    m_password = dlg.password();
                    wallet->writePassword(m_host, m_password);
                }
            }
        }
    }
#endif

    if(!m_password.isEmpty()) {
        arguments << "-p" << m_password;
    }

    arguments << "-X" << QString::number(m_container->winId());
//     arguments << "-a" << QString::number(hp->colorDepth());
    arguments << (m_host + ':' + QString::number(m_port));

    for (int i = 0; i < arguments.size(); ++i)
        kDebug(5012) << arguments.at(i).toLocal8Bit().constData() << endl;

    setStatus(Connecting);

    connect(m_process, SIGNAL(error(QProcess::ProcessError)), SLOT(processError(QProcess::ProcessError)));
    connect(m_process, SIGNAL(readyReadStandardError()), SLOT(receivedStandardError()));
    connect(m_container, SIGNAL(clientClosed()), SLOT(connectionClosed()));
    connect(m_container, SIGNAL(clientIsEmbedded()), SLOT(connectionOpened()));

    m_process->start("rdesktop", arguments);

    return true;
}

void RdpView::switchFullscreen(bool on)
{
    if(on == true) {
        m_container->grabKeyboard();
    }
}

// captures pressed keys
void RdpView::pressKey(XEvent *e)
{
    Q_UNUSED(e);
    m_container->grabKeyboard();
}

bool RdpView::viewOnly()
{
    return m_viewOnly;
}

void RdpView::setViewOnly(bool s)
{
    m_viewOnly = s;
}

void RdpView::connectionOpened()
{
    kDebug(5012) << "Connection opened" << endl;
    QSize size = m_container->minimumSizeHint();
    kDebug(5012) << "Size hint: " << size.width() << " " << size.height() << endl;
    setStatus(Connected);
    setFixedSize(size);
    resize(size);
    // m_container->adjustSize() ?
    m_container->setFixedSize(size);
    emit changeSize(size.width(), size.height());
    emit connected();
    setFocus();
}

void RdpView::connectionClosed()
{
    emit disconnected();
    setStatus(Disconnected);
    m_quitFlag = true;
}

void RdpView::processError(QProcess::ProcessError error)
{
    if(m_status == Connecting) {
        setStatus(Disconnected);

        if (error == QProcess::FailedToStart) {
            KMessageBox::error(0, i18n("Could not start rdesktop; make sure rdesktop is properly installed."),
                                  i18n("rdesktop Failure"));
            return;
        }

        if(m_clientVersion.isEmpty()) {
            KMessageBox::error(0, i18n("Connection attempt to host failed."),
                                  i18n("Connection Failure"));
        } else {
            KMessageBox::error(0, i18n("The version of rdesktop you are using (%1) is too old:\n"
                                       "rdesktop 1.3.2 or greater is required.", m_clientVersion),
                                  i18n("rdesktop Failure"));
        }
        emit disconnectedError();
    }
}

void RdpView::receivedStandardError()
{
    QString output(m_process->readAllStandardError());
    QString line;
    int i = 0;
    while(!(line = output.section('\n', i, i)).isEmpty()) {
        if(line.startsWith("Version ")) {
            m_clientVersion = line.section(' ', 1, 1);
            m_clientVersion = m_clientVersion.left(m_clientVersion.length() - 1);
            return;
        } else {
            kDebug(5012) << "Process error output: " << line << endl;
        }
        i++;
    }
}

#include "rdpview.moc"
