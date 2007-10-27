/****************************************************************************
**
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

#include "hostpreferences.h"

#include "settings.h"

#include <KDebug>
#include <KLocale>
#include <KStandardDirs>
#include <KTitleWidget>

#include <QCheckBox>
#include <QFile>
#include <QVBoxLayout>

HostPreferences::HostPreferences(const QString &url, QObject *parent)
        : QObject(parent),
        m_url(url),
        m_showConfigAgain(true),
        m_walletSupport(true)
{
    if (m_url.endsWith('/')) // check case when user enters an ending slash -> remove it
        m_url.truncate(m_url.length() - 1);

    m_filename = KStandardDirs::locateLocal("appdata", "hostpreferences.xml");

    QFile file(m_filename);

    if (!m_doc.setContent(&file)) {
        kWarning(5010) << "Error reading " << m_filename;

        // no xml file found, create a new one
        QDomDocument domDocument("krdc");
        QDomProcessingInstruction process = domDocument.createProcessingInstruction(
                                                "xml", "version=\"1.0\" encoding=\"UTF-8\"");
        domDocument.appendChild(process);

        QDomElement root = domDocument.createElement("krdc");
        root.setAttribute("version", "1.0");
        domDocument.appendChild(root);

        if (!file.open(QFile::WriteOnly | QFile::Truncate))
            kWarning(5010) << "Error creating " << m_filename;

        QTextStream out(&file);
        domDocument.save(out, 4);

        file.close();

        m_doc.setContent(&file);
    }
}

HostPreferences::~HostPreferences()
{
}

void HostPreferences::updateElement(const QString &name, const QString &value)
{
    QDomElement oldElement = m_element.firstChildElement(name);

    if (oldElement == QDomElement()) {
        oldElement = m_doc.createElement(name);
        m_element.appendChild(oldElement);
    }

    QDomElement newElement = m_doc.createElement(name);
    QDomText newText = m_doc.createTextNode(value);
    newElement.appendChild(newText);
    m_element.replaceChild(newElement, oldElement);
}

bool HostPreferences::saveConfig()
{
    saveProtocolSpecificConfig();

    if (showAgainCheckBox) // check if the checkbox has been created
        setShowConfigAgain(showAgainCheckBox->isChecked());

    if (walletSupportCheckBox)
        setWalletSupport(walletSupportCheckBox->isChecked());

    updateElement("showConfigAgain", m_showConfigAgain ? "true" : "false");
    updateElement("walletSupport", m_walletSupport ? "true" : "false");

    QDomElement root = m_doc.documentElement();
    QDomElement oldElement = QDomElement();
    for (QDomNode n = root.firstChild(); !n.isNull(); n = n.nextSibling()) {
        if (n.toElement().hasAttribute("url") && n.toElement().attribute("url") == m_url) {
            oldElement = n.toElement();
        }
    }

    if (oldElement == QDomElement()) // host not existing, create new one
        m_doc.appendChild(m_element);
    else
        m_doc.replaceChild(m_element, oldElement);

    QFile file(m_filename);
    if (!file.open(QFile::WriteOnly | QFile::Text)) {
        kWarning(5010) << "Cannot write " << m_filename << ". " << file.errorString() << endl;
        return false;
    }

    QTextStream out(&file);
    m_doc.save(out, 4);
    return true;
}

bool HostPreferences::hostConfigured()
{
    QDomElement root = m_doc.documentElement();
    for (QDomNode n = root.firstChild(); !n.isNull(); n = n.nextSibling()) {
        if (n.toElement().hasAttribute("url") && n.toElement().attribute("url") == m_url) {
            kDebug(5010) << "Found: " << m_url;
            m_element = n.toElement();

            readConfig();

            return true;
        }
    }

    // host not configured yet, create a new host element
    m_element = m_doc.createElement("host");
    m_doc.documentElement().appendChild(m_element);
    m_element.setTagName("host");
    m_element.setAttribute("url", m_url);

    readConfig();

    return false;
}

void HostPreferences::readConfig()
{
    readProtocolSpecificConfig();

    setShowConfigAgain(m_element.firstChildElement("showConfigAgain").text() != "false");
    if (m_element.firstChildElement("walletSupport") != QDomElement())
        setWalletSupport(m_element.firstChildElement("walletSupport").text() != "false");
    else
        setWalletSupport(Settings::walletSupport());
}

void HostPreferences::setShowConfigAgain(bool show)
{
    m_showConfigAgain = show;
}

bool HostPreferences::showConfigAgain()
{
    return m_showConfigAgain;
}

void HostPreferences::setWalletSupport(bool walletSupport)
{
    m_walletSupport = walletSupport;
}

bool HostPreferences::walletSupport()
{
    return m_walletSupport;
}

KDialog *HostPreferences::createDialog(QWidget *widget)
{
    KDialog *dialog = new KDialog;
    dialog->setCaption(i18n("Host Configuration"));

    QWidget *mainWidget = new QWidget(dialog);
    QVBoxLayout *layout = new QVBoxLayout(mainWidget);

    KTitleWidget *titleWidget = new KTitleWidget(dialog);
    titleWidget->setText(i18n("Host Configuration"));
    titleWidget->setPixmap(KIcon("krdc"));

    showAgainCheckBox = new QCheckBox(mainWidget);
    showAgainCheckBox->setText(i18n("Show this dialog again for this host"));
    showAgainCheckBox->setChecked(showConfigAgain());

    walletSupportCheckBox = new QCheckBox(mainWidget);
    walletSupportCheckBox->setText(i18n("Remember password (KWallet)"));
    walletSupportCheckBox->setChecked(walletSupport());

    layout->addWidget(titleWidget);
    layout->addWidget(widget);
    layout->addWidget(showAgainCheckBox);
    layout->addWidget(walletSupportCheckBox);
    layout->addStretch(1);
    mainWidget->setLayout(layout);

    dialog->setMainWidget(mainWidget);

    return dialog;
}

#include "hostpreferences.moc"
