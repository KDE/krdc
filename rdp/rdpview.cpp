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
#include "krdc_debug.h"

#include <KMessageBox>
#include <KPasswordDialog>
#include <KShell>

#include <QWindow>
#include <QEvent>
#include <QInputDialog>

RdpView::RdpView(QWidget *parent,
                 const QUrl &url,
                 KConfigGroup configGroup,
                 const QString &user, const QString &password)
        : RemoteView(parent),
        m_user(user),
        m_password(password),
        m_quitFlag(false),
        m_process(NULL)
{
    m_url = url;
    m_host = url.host();
    m_port = url.port();

    if (m_port <= 0) {
        m_port = TCP_PORT_RDP;
    }

    m_container = new QWindow();
    m_containerWidget = QWidget::createWindowContainer(m_container, this);
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
    return m_containerWidget->minimumSize();
}

QSize RdpView::sizeHint() const
{
    return maximumSize();
}

void RdpView::startQuitting()
{
    qCDebug(KRDC) << "About to quit";
    m_quitFlag = true;
    if (m_process) {
        m_process->terminate();
        m_process->waitForFinished(1000);
        m_container->destroy();
    }
}

bool RdpView::isQuitting()
{
    return m_quitFlag;
}

bool RdpView::start()
{
    m_containerWidget->show();

    if (m_url.userName().isEmpty()) {
        QString userName;
        bool ok = false;

        userName = QInputDialog::getText(this, i18n("Enter Username"),
                                         i18n("Please enter the username you would like to use for login."),
                                         QLineEdit::Normal, Settings::defaultRdpUserName(), &ok);

        if (ok) {
            m_url.setUserName(userName);
        }
    }

    if (!m_url.userName().isEmpty()) {
        const bool useLdapLogin = Settings::recognizeLdapLogins() && m_url.userName().contains(QLatin1Char('\\'));
        qCDebug(KRDC) << "Is LDAP login:" << useLdapLogin << m_url.userName();

        QString walletPassword = QString();
        if (m_hostPreferences->walletSupport()) {
            walletPassword = readWalletPassword(useLdapLogin);
        }
        if (!walletPassword.isNull()) {
            m_url.setPassword(walletPassword);
        } else {
            KPasswordDialog dialog(this);
            dialog.setPrompt(i18n("Access to the system requires a password."));
            if (dialog.exec() == KPasswordDialog::Accepted) {
                m_url.setPassword(dialog.password());

                if (m_hostPreferences->walletSupport()) {
                    saveWalletPassword(dialog.password(), useLdapLogin);
                }
            }
        }
    }

    // Check the version of FreeRDP so we can use pre-1.1 switches if needed
    QProcess *xfreeRDPVersionCheck = new QProcess(this);
    xfreeRDPVersionCheck->start(QStringLiteral("xfreerdp"), QStringList(QStringLiteral("--version")));
    xfreeRDPVersionCheck->waitForFinished();
    QString versionOutput = QString::fromUtf8(xfreeRDPVersionCheck->readAllStandardOutput().constData());
    xfreeRDPVersionCheck->deleteLater();

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
    m_containerWidget->setFixedWidth(width);
    m_containerWidget->setFixedHeight(height);
    setMaximumSize(width, height);
    if (versionOutput.contains(QLatin1String(" 1.0"))) {
        qCDebug(KRDC) << "Use FreeRDP 1.0 compatible arguments";

        arguments << QStringLiteral("-g") << QString::number(width) + QLatin1Char('x') + QString::number(height);

        arguments << QStringLiteral("-k") << keymapToXfreerdp(m_hostPreferences->keyboardLayout());

        if (!m_url.userName().isEmpty()) {
            // if username contains a domain, it needs to be set with another parameter
            if (m_url.userName().contains(QLatin1Char('\\'))) {
                const QStringList splittedName = m_url.userName().split(QLatin1Char('\\'));
                arguments << QStringLiteral("-d") << splittedName.at(0);
                arguments << QStringLiteral("-u") << splittedName.at(1);
            } else {
                arguments << QStringLiteral("-u") << m_url.userName();
            }
        } else {
            arguments << QStringLiteral("-u") << QStringLiteral("");
        }

        arguments << QStringLiteral("-D");  // request the window has no decorations
        arguments << QStringLiteral("-X") << QString::number(m_container->winId());
        arguments << QStringLiteral("-a") << QString::number((m_hostPreferences->colorDepth() + 1) * 8);

        switch (m_hostPreferences->sound()) {
        case 1:
            arguments << QStringLiteral("-o");
            break;
        case 0:
            arguments << QStringLiteral("--plugin") << QStringLiteral("rdpsnd");
            break;
        case 2:
        default:
            break;
        }

        if (!m_hostPreferences->shareMedia().isEmpty()) {
            QStringList shareMedia;
            shareMedia << QStringLiteral("--plugin") << QStringLiteral("rdpdr")
                       << QStringLiteral("--data") << QStringLiteral("disk:media:")
                       + m_hostPreferences->shareMedia() << QStringLiteral("--");
            arguments += shareMedia;
        }

        QString performance;
        switch (m_hostPreferences->performance()) {
        case 0:
            performance = QLatin1Char('m');
            break;
        case 1:
            performance = QLatin1Char('b');
            break;
        case 2:
            performance = QLatin1Char('l');
            break;
        default:
            break;
        }

        arguments << QStringLiteral("-x") << performance;

        if (m_hostPreferences->console()) {
            arguments << QStringLiteral("-0");
        }

        if (m_hostPreferences->remoteFX()) {
            arguments << QStringLiteral("--rfx");
        }

        if (!m_hostPreferences->extraOptions().isEmpty()) {
            const QStringList additionalArguments = KShell::splitArgs(m_hostPreferences->extraOptions());
            arguments += additionalArguments;
        }

        // krdc has no support for certificate management yet; it would not be possbile to connect to any host:
        // "The host key for example.com has changed" ...
        // "Add correct host key in ~/.freerdp/known_hosts to get rid of this message."
        arguments << QStringLiteral("--ignore-certificate");

        // clipboard sharing is activated in KRDC; user can disable it at runtime
        arguments << QStringLiteral("--plugin") << QStringLiteral("cliprdr");

        arguments << QStringLiteral("-t") << QString::number(m_port);
        arguments << m_host;

        qCDebug(KRDC) << "Starting xfreerdp with arguments: " << arguments.join(QStringLiteral(" "));

        arguments.removeLast(); // host must be last, remove and re-add it after the password
        if (!m_url.password().isNull())
            arguments << QStringLiteral("-p") << m_url.password();
        arguments << m_host;

    } else {
        qCDebug(KRDC) << "Use FreeRDP 1.1+ compatible arguments";

        arguments << QStringLiteral("-decorations");
        arguments << QStringLiteral("/w:") + QString::number(width);
        arguments << QStringLiteral("/h:") + QString::number(height);

        arguments << QStringLiteral("/kbd:") + keymapToXfreerdp(m_hostPreferences->keyboardLayout());

        if (!m_url.userName().isEmpty()) {
            // if username contains a domain, it needs to be set with another parameter
            if (m_url.userName().contains(QLatin1Char('\\'))) {
                const QStringList splittedName = m_url.userName().split(QLatin1Char('\\'));
                arguments << QStringLiteral("/d:") + splittedName.at(0);
                arguments << QStringLiteral("/u:") + splittedName.at(1);
            } else {
                arguments << QStringLiteral("/u:") + m_url.userName();
            }
        } else {
            arguments << QStringLiteral("/u:");
        }

        arguments << QStringLiteral("/parent-window:") + QString::number(m_container->winId());
        arguments << QStringLiteral("/bpp:") + QString::number((m_hostPreferences->colorDepth() + 1) * 8);
        arguments << QStringLiteral("/audio-mode:") + QString::number(m_hostPreferences->sound());

        if (!m_hostPreferences->shareMedia().isEmpty()) {
            QStringList shareMedia;
            shareMedia << QStringLiteral("/drive:media,") + m_hostPreferences->shareMedia();
            arguments += shareMedia;
        }

        QString performance;
        switch (m_hostPreferences->performance()) {
        case 0:
            performance = QStringLiteral("modem");
            break;
        case 1:
            performance = QStringLiteral("broadband");
            break;
        case 2:
            performance = QStringLiteral("lan");
            break;
        default:
            break;
        }

        arguments << QStringLiteral("/network:") + performance;

        if (m_hostPreferences->console()) {
            arguments << QStringLiteral("/admin");
        }

        if (m_hostPreferences->remoteFX()) {
            arguments << QStringLiteral("/rfx");
        }

        if (!m_hostPreferences->extraOptions().isEmpty()) {
            const QStringList additionalArguments = KShell::splitArgs(m_hostPreferences->extraOptions());
            arguments += additionalArguments;
        }

        // krdc has no support for certificate management yet; it would not be possbile to connect to any host:
        // "The host key for example.com has changed" ...
        // "Add correct host key in ~/.freerdp/known_hosts to get rid of this message."
        arguments << QStringLiteral("/cert-ignore");

        // clipboard sharing is activated in KRDC; user can disable it at runtime
        arguments << QStringLiteral("+clipboard");

        arguments << QStringLiteral("/port:") + QString::number(m_port);
        arguments << QStringLiteral("/v:") + m_host;

        qCDebug(KRDC) << "Starting xfreerdp with arguments: " << arguments.join(QStringLiteral(" "));

        //avoid printing the password in debug
        if (!m_url.password().isNull()) {
            arguments << QStringLiteral("/p:") + m_url.password();
        }
    }

    setStatus(Connecting);

    connect(m_process, SIGNAL(error(QProcess::ProcessError)), SLOT(processError(QProcess::ProcessError)));
    connect(m_process, SIGNAL(readyReadStandardError()), SLOT(receivedStandardError()));
    connect(m_process, SIGNAL(readyReadStandardOutput()), SLOT(receivedStandardOutput()));
    connect(m_process, SIGNAL(finished(int, QProcess::ExitStatus)), SLOT(connectionClosed()));
    connect(m_process, SIGNAL(started()), SLOT(connectionOpened()));

    m_process->start(QStringLiteral("xfreerdp"), arguments);

    return true;
}

HostPreferences* RdpView::hostPreferences()
{
    return m_hostPreferences;
}

void RdpView::switchFullscreen(bool on)
{
    if (on == true) {
        m_container->setKeyboardGrabEnabled(true);
    }
}

void RdpView::connectionOpened()
{
    qCDebug(KRDC) << "Connection opened";
    const QSize size = QSize(m_container->width(), m_container->height());
    qCDebug(KRDC) << "Size hint: " << size.width() << " " << size.height();
    setStatus(Connected);
    setFixedSize(size);
    resize(size);
    m_containerWidget->setFixedSize(size);
    emit framebufferSizeChanged(size.width(), size.height());
    emit connected();
    setFocus();
}

QPixmap RdpView::takeScreenshot()
{
    return QPixmap::grabWindow(m_container->winId());
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
    qCDebug(KRDC) << error;
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
    const QString output = QString::fromUtf8(m_process->readAllStandardError().constData());
    qCDebug(KRDC) << output;
    QString line;
    int i = 0;
    while (!(line = output.section(QLatin1Char('\n'), i, i)).isEmpty()) {

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
    const QString output = QString::fromUtf8(m_process->readAllStandardOutput().constData());
    qCDebug(KRDC) << output;
    QStringList splittedOutput = output.split(QLatin1Char('\n'));
    Q_FOREACH (const QString &line, splittedOutput) {
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

        } else if (line.contains(QLatin1String("Authentication failure, check credentials"))) {
            KMessageBox::error(0, i18n("Authentication failure, check credentials."),
                               i18n("Connection Failure"));
            connectionError();
            return;

        // looks like some generic xfreerdp error message, handle it if nothing was handled:
        // "Error: protocol security negotiation failure"
        } else if (line.contains(QLatin1String("Error: protocol security negotiation failure")) || // xfreerdp 1.0
            line.contains(QLatin1String("Error: protocol security negotiation or connection failure"))) { // xfreerdp 1.2
            KMessageBox::error(0, i18n("Connection attempt to host failed. Security negotiation or connection failure."),
                               i18n("Connection Failure"));
            connectionError();
            return;
        }
    }
}

void RdpView::setGrabAllKeys(bool grabAllKeys)
{
    Q_UNUSED(grabAllKeys);
    // do nothing.. grabKeyboard seems not to be supported in QX11EmbedContainer
}

QString RdpView::keymapToXfreerdp(const QString &keyboadLayout)
{
    if (keymapToXfreerdpHash.isEmpty()) {
        keymapToXfreerdpHash = initKeymapToXfreerdp();
    }
    return keymapToXfreerdpHash[keyboadLayout];
}

// list of xfreerdp --kbd-list
// this is a mapping for rdesktop comptibilty (old settings will still work)
// needs to be completed (when not in message freeze; needs new localization)
QHash<QString, QString> RdpView::initKeymapToXfreerdp()
{
    QHash<QString, QString> keymapToXfreerdpHash;

    // Keyboard Layouts
    keymapToXfreerdpHash[QStringLiteral("ar")] = QStringLiteral("0x00000401");  // Arabic (101)
    // keymapToXfreerdpHash[""] = "0x00000402";  // Bulgarian
    // keymapToXfreerdpHash[""] = "0x00000404";  // Chinese (Traditional) - US Keyboard
    keymapToXfreerdpHash[QStringLiteral("cs")] = QStringLiteral("0x00000405");  // Czech
    keymapToXfreerdpHash[QStringLiteral("da")] = QStringLiteral("0x00000406");  // Danish
    keymapToXfreerdpHash[QStringLiteral("fo")] = QStringLiteral("0x00000406");  // Danish, Faroese; legacy for rdesktop
    keymapToXfreerdpHash[QStringLiteral("de")] = QStringLiteral("0x00000407");  // German
    // keymapToXfreerdpHash[""] = "0x00000408";  // Greek
    keymapToXfreerdpHash[QStringLiteral("en-us")] = QStringLiteral("0x00000409");  // US
    keymapToXfreerdpHash[QStringLiteral("es")] = QStringLiteral("0x0000040A");  // Spanish
    keymapToXfreerdpHash[QStringLiteral("fi")] = QStringLiteral("0x0000040B");  // Finnish
    keymapToXfreerdpHash[QStringLiteral("fr")] = QStringLiteral("0x0000040C");  // French
    keymapToXfreerdpHash[QStringLiteral("he")] = QStringLiteral("0x0000040D");  // Hebrew
    keymapToXfreerdpHash[QStringLiteral("hu")] = QStringLiteral("0x0000040E");  // Hungarian
    keymapToXfreerdpHash[QStringLiteral("is")] = QStringLiteral("0x0000040F");  // Icelandic
    keymapToXfreerdpHash[QStringLiteral("it")] = QStringLiteral("0x00000410");  // Italian
    keymapToXfreerdpHash[QStringLiteral("ja")] = QStringLiteral("0x00000411");  // Japanese
    keymapToXfreerdpHash[QStringLiteral("ko")] = QStringLiteral("0x00000412");  // Korean
    keymapToXfreerdpHash[QStringLiteral("nl")] = QStringLiteral("0x00000413");  // Dutch
    keymapToXfreerdpHash[QStringLiteral("no")] = QStringLiteral("0x00000414");  // Norwegian
    keymapToXfreerdpHash[QStringLiteral("pl")] = QStringLiteral("0x00000415");  // Polish (Programmers)
    keymapToXfreerdpHash[QStringLiteral("pt-br")] = QStringLiteral("0x00000416");  // Portuguese (Brazilian ABNT)
    // keymapToXfreerdpHash[""] = "0x00000418";  // Romanian
    keymapToXfreerdpHash[QStringLiteral("ru")] = QStringLiteral("0x00000419");  // Russian
    keymapToXfreerdpHash[QStringLiteral("hr")] = QStringLiteral("0x0000041A");  // Croatian
    // keymapToXfreerdpHash[""] = "0x0000041B";  // Slovak
    // keymapToXfreerdpHash[""] = "0x0000041C";  // Albanian
    keymapToXfreerdpHash[QStringLiteral("sv")] = QStringLiteral("0x0000041D");  // Swedish
    keymapToXfreerdpHash[QStringLiteral("th")] = QStringLiteral("0x0000041E");  // Thai Kedmanee
    keymapToXfreerdpHash[QStringLiteral("tr")] = QStringLiteral("0x0000041F");  // Turkish Q
    // keymapToXfreerdpHash[""] = "0x00000420";  // Urdu
    // keymapToXfreerdpHash[""] = "0x00000422";  // Ukrainian
    // keymapToXfreerdpHash[""] = "0x00000423";  // Belarusian
    keymapToXfreerdpHash[QStringLiteral("sl")] = QStringLiteral("0x00000424");  // Slovenian
    keymapToXfreerdpHash[QStringLiteral("et")] = QStringLiteral("0x00000425");  // Estonian
    keymapToXfreerdpHash[QStringLiteral("lv")] = QStringLiteral("0x00000426");  // Latvian
    keymapToXfreerdpHash[QStringLiteral("lt")] = QStringLiteral("0x00000427");  // Lithuanian IBM
    // keymapToXfreerdpHash[""] = "0x00000429";  // Farsi
    // keymapToXfreerdpHash[""] = "0x0000042A";  // Vietnamese
    // keymapToXfreerdpHash[""] = "0x0000042B";  // Armenian Eastern
    // keymapToXfreerdpHash[""] = "0x0000042C";  // Azeri Latin
    keymapToXfreerdpHash[QStringLiteral("mk")] = QStringLiteral("0x0000042F");  // FYRO Macedonian
    // keymapToXfreerdpHash[""] = "0x00000437";  // Georgian
    // keymapToXfreerdpHash[""] = "0x00000438";  // Faeroese
    // keymapToXfreerdpHash[""] = "0x00000439";  // Devanagari - INSCRIPT
    // keymapToXfreerdpHash[""] = "0x0000043A";  // Maltese 47-key
    // keymapToXfreerdpHash[""] = "0x0000043B";  // Norwegian with Sami
    // keymapToXfreerdpHash[""] = "0x0000043F";  // Kazakh
    // keymapToXfreerdpHash[""] = "0x00000440";  // Kyrgyz Cyrillic
    // keymapToXfreerdpHash[""] = "0x00000444";  // Tatar
    // keymapToXfreerdpHash[""] = "0x00000445";  // Bengali
    // keymapToXfreerdpHash[""] = "0x00000446";  // Punjabi
    // keymapToXfreerdpHash[""] = "0x00000447";  // Gujarati
    // keymapToXfreerdpHash[""] = "0x00000449";  // Tamil
    // keymapToXfreerdpHash[""] = "0x0000044A";  // Telugu
    // keymapToXfreerdpHash[""] = "0x0000044B";  // Kannada
    // keymapToXfreerdpHash[""] = "0x0000044C";  // Malayalam
    // keymapToXfreerdpHash[""] = "0x0000044E";  // Marathi
    // keymapToXfreerdpHash[""] = "0x00000450";  // Mongolian Cyrillic
    // keymapToXfreerdpHash[""] = "0x00000452";  // United Kingdom Extended
    // keymapToXfreerdpHash[""] = "0x0000045A";  // Syriac
    // keymapToXfreerdpHash[""] = "0x00000461";  // Nepali
    // keymapToXfreerdpHash[""] = "0x00000463";  // Pashto
    // keymapToXfreerdpHash[""] = "0x00000465";  // Divehi Phonetic
    // keymapToXfreerdpHash[""] = "0x0000046E";  // Luxembourgish
    // keymapToXfreerdpHash[""] = "0x00000481";  // Maori
    // keymapToXfreerdpHash[""] = "0x00000804";  // Chinese (Simplified) - US Keyboard
    keymapToXfreerdpHash[QStringLiteral("de-ch")] = QStringLiteral("0x00000807");  // Swiss German
    keymapToXfreerdpHash[QStringLiteral("en-gb")] = QStringLiteral("0x00000809");  // United Kingdom
    // keymapToXfreerdpHash[""] = "0x0000080A";  // Latin American
    keymapToXfreerdpHash[QStringLiteral("fr-be")] = QStringLiteral("0x0000080C");  // Belgian French
    keymapToXfreerdpHash[QStringLiteral("nl-be")] = QStringLiteral("0x00000813");  // Belgian (Period)
    keymapToXfreerdpHash[QStringLiteral("pt")] = QStringLiteral("0x00000816");  // Portuguese
    // keymapToXfreerdpHash[""] = "0x0000081A";  // Serbian (Latin)
    // keymapToXfreerdpHash[""] = "0x0000082C";  // Azeri Cyrillic
    // keymapToXfreerdpHash[""] = "0x0000083B";  // Swedish with Sami
    // keymapToXfreerdpHash[""] = "0x00000843";  // Uzbek Cyrillic
    // keymapToXfreerdpHash[""] = "0x0000085D";  // Inuktitut Latin
    // keymapToXfreerdpHash[""] = "0x00000C0C";  // Canadian French (legacy)
    // keymapToXfreerdpHash[""] = "0x00000C1A";  // Serbian (Cyrillic)
    keymapToXfreerdpHash[QStringLiteral("fr-ca")] = QStringLiteral("0x00001009");  // Canadian French
    keymapToXfreerdpHash[QStringLiteral("fr-ch")] = QStringLiteral("0x0000100C");  // Swiss French
    // keymapToXfreerdpHash[""] = "0x0000141A";  // Bosnian
    // keymapToXfreerdpHash[""] = "0x00001809";  // Irish
    // keymapToXfreerdpHash[""] = "0x0000201A";  // Bosnian Cyrillic

    // Keyboard Layout Variants
    // keymapToXfreerdpHash[""] = "0x00010401";  // Arabic (102)
    // keymapToXfreerdpHash[""] = "0x00010402";  // Bulgarian (Latin)
    // keymapToXfreerdpHash[""] = "0x00010405";  // Czech (QWERTY)
    // keymapToXfreerdpHash[""] = "0x00010407";  // German (IBM)
    // keymapToXfreerdpHash[""] = "0x00010408";  // Greek (220)
    keymapToXfreerdpHash[QStringLiteral("en-dv")] = QStringLiteral("0x00010409");  // United States-Dvorak
    // keymapToXfreerdpHash[""] = "0x0001040A";  // Spanish Variation
    // keymapToXfreerdpHash[""] = "0x0001040E";  // Hungarian 101-key
    // keymapToXfreerdpHash[""] = "0x00010410";  // Italian (142)
    // keymapToXfreerdpHash[""] = "0x00010415";  // Polish (214)
    // keymapToXfreerdpHash[""] = "0x00010416";  // Portuguese (Brazilian ABNT2)
    // keymapToXfreerdpHash[""] = "0x00010419";  // Russian (Typewriter)
    // keymapToXfreerdpHash[""] = "0x0001041B";  // Slovak (QWERTY)
    // keymapToXfreerdpHash[""] = "0x0001041E";  // Thai Pattachote
    // keymapToXfreerdpHash[""] = "0x0001041F";  // Turkish F
    // keymapToXfreerdpHash[""] = "0x00010426";  // Latvian (QWERTY)
    // keymapToXfreerdpHash[""] = "0x00010427";  // Lithuanian
    // keymapToXfreerdpHash[""] = "0x0001042B";  // Armenian Western
    // keymapToXfreerdpHash[""] = "0x00010439";  // Hindi Traditional
    // keymapToXfreerdpHash[""] = "0x0001043A";  // Maltese 48-key
    // keymapToXfreerdpHash[""] = "0x0001043B";  // Sami Extended Norway
    // keymapToXfreerdpHash[""] = "0x00010445";  // Bengali (Inscript)
    // keymapToXfreerdpHash[""] = "0x0001045A";  // Syriac Phonetic
    // keymapToXfreerdpHash[""] = "0x00010465";  // Divehi Typewriter
    // keymapToXfreerdpHash[""] = "0x0001080C";  // Belgian (Comma)
    // keymapToXfreerdpHash[""] = "0x0001083B";  // Finnish with Sami
    // keymapToXfreerdpHash[""] = "0x00011009";  // Canadian Multilingual Standard
    // keymapToXfreerdpHash[""] = "0x00011809";  // Gaelic
    // keymapToXfreerdpHash[""] = "0x00020401";  // Arabic (102) AZERTY
    // keymapToXfreerdpHash[""] = "0x00020405";  // Czech Programmers
    // keymapToXfreerdpHash[""] = "0x00020408";  // Greek (319)
    // keymapToXfreerdpHash[""] = "0x00020409";  // United States-International
    // keymapToXfreerdpHash[""] = "0x0002041E";  // Thai Kedmanee (non-ShiftLock)
    // keymapToXfreerdpHash[""] = "0x0002083B";  // Sami Extended Finland-Sweden
    // keymapToXfreerdpHash[""] = "0x00030408";  // Greek (220) Latin
    // keymapToXfreerdpHash[""] = "0x00030409";  // United States-Dvorak for left hand
    // keymapToXfreerdpHash[""] = "0x0003041E";  // Thai Pattachote (non-ShiftLock)
    // keymapToXfreerdpHash[""] = "0x00040408";  // Greek (319) Latin
    // keymapToXfreerdpHash[""] = "0x00040409";  // United States-Dvorak for right hand
    // keymapToXfreerdpHash[""] = "0x00050408";  // Greek Latin
    // keymapToXfreerdpHash[""] = "0x00050409";  // US English Table for IBM Arabic 238_L
    // keymapToXfreerdpHash[""] = "0x00060408";  // Greek Polytonic
    // keymapToXfreerdpHash[""] = "0xB0000407";  // German Neo

    // Keyboard Input Method Editors (IMEs)
    // keymapToXfreerdpHash[""] = "0xE0010404";  // Chinese (Traditional) - Phonetic
    // keymapToXfreerdpHash[""] = "0xE0010411";  // Japanese Input System (MS-IME2002)
    // keymapToXfreerdpHash[""] = "0xE0010412";  // Korean Input System (IME 2000)
    // keymapToXfreerdpHash[""] = "0xE0010804";  // Chinese (Simplified) - QuanPin
    // keymapToXfreerdpHash[""] = "0xE0020404";  // Chinese (Traditional) - ChangJie
    // keymapToXfreerdpHash[""] = "0xE0020804";  // Chinese (Simplified) - ShuangPin
    // keymapToXfreerdpHash[""] = "0xE0030404";  // Chinese (Traditional) - Quick
    // keymapToXfreerdpHash[""] = "0xE0030804";  // Chinese (Simplified) - ZhengMa
    // keymapToXfreerdpHash[""] = "0xE0040404";  // Chinese (Traditional) - Big5 Code
    // keymapToXfreerdpHash[""] = "0xE0050404";  // Chinese (Traditional) - Array
    // keymapToXfreerdpHash[""] = "0xE0050804";  // Chinese (Simplified) - NeiMa
    // keymapToXfreerdpHash[""] = "0xE0060404";  // Chinese (Traditional) - DaYi
    // keymapToXfreerdpHash[""] = "0xE0070404";  // Chinese (Traditional) - Unicode
    // keymapToXfreerdpHash[""] = "0xE0080404";  // Chinese (Traditional) - New Phonetic
    // keymapToXfreerdpHash[""] = "0xE0090404";  // Chinese (Traditional) - New ChangJie
    // keymapToXfreerdpHash[""] = "0xE00E0804";  // Chinese (Traditional) - Microsoft Pinyin IME 3.0
    // keymapToXfreerdpHash[""] = "0xE00F0404";  // Chinese (Traditional) - Alphanumeric

    return keymapToXfreerdpHash;
}

#include "rdpview.moc"
