/****************************************************************************
**
** Copyright (C) 2008 David Gross <gdavid.devel@gmail.com>
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

#include "nxhostpreferences.h"

#include "settings.h"

#include <KDebug>

#include <QDesktopWidget>
#include <QFile>
#include <QByteArray>
#include <QFileDialog>
#include <QTextEdit>

static QStringList keymaps = (QStringList()
    << "ar"
    << "cs"
    << "da"
    << "de"
    << "de-ch"
    << "en-dv"
    << "en-gb"
    << "en-us"
    << "es"
    << "et"
    << "fi"
    << "fo"
    << "fr"
    << "fr-be"
    << "fr-ca"
    << "fr-ch"
    << "he"
    << "hr"
    << "hu"
    << "is"
    << "it"
    << "ja"
    << "ko"
    << "lt"
    << "lv"
    << "mk"
    << "nl"
    << "nl-be"
    << "no"
    << "pl"
    << "pt"
    << "pt-br"
    << "ru"
    << "sl"
    << "sv"
    << "th"
    << "tr"
);

static const int defaultKeymap = 7; // en-us

inline int keymap2int(const QString &keymap)
{
    int index = keymaps.lastIndexOf(keymap);
    return (index == -1) ? defaultKeymap : index;
}

inline QString int2keymap(int layout)
{
    if (layout >= 0 && layout < keymaps.count())
        return keymaps.at(layout);
    else
        return keymaps.at(defaultKeymap);
}

static QStringList desktopTypes = (QStringList()
    << "unix-kde" 
    << "unix-gnome"
    << "unix-cde" 
    << "unix-xdm"
);

static const int defaultDesktopType = 0;

inline int desktopType2int(const QString &desktopType)
{
    int index = desktopTypes.lastIndexOf(desktopType);
    return (index == -1) ? defaultDesktopType : index;
}

inline QString int2desktopType(int index)
{
    if(index >=0 && index < desktopTypes.count())
        return desktopTypes.at(index);
    else
        return desktopTypes.at(defaultDesktopType);
}

NxHostPreferences::NxHostPreferences(const QString &url, bool forceShow, QObject *parent)
  : HostPreferences(url, parent),
    m_height(600),
    m_width(800),
    m_desktopType("unix-kde"),
    m_keyboardLayout("en-us"),
    m_privateKey("default")
{
    if (hostConfigured()) {
        if (showConfigAgain() || forceShow) {
            kDebug(5013) << "Show config dialog again";
            showDialog();
        } else
            return; // no changes, no need to save
    } else {
        kDebug(5013) << "No config found, create new";
        if (Settings::showPreferencesForNewConnections())
            showDialog();
    }

    saveConfig();
}

NxHostPreferences::~NxHostPreferences()
{
}

void NxHostPreferences::showDialog()
{
    nxPage = new QWidget();
    nxUi.setupUi(nxPage);

    KDialog *dialog = createDialog(nxPage);

    nxUi.kcfg_Height->setValue(height());
    nxUi.kcfg_Width->setValue(width());
    nxUi.kcfg_DesktopType->setCurrentIndex(desktopType2int(desktopType()));
    nxUi.kcfg_KeyboardLayout->setCurrentIndex(keymap2int(keyboardLayout()));

    if(privateKey() == "default") {
        nxUi.checkboxDefaultPrivateKey->setChecked(true);
        nxUi.groupboxPrivateKey->setEnabled(false);
	setDefaultPrivateKey(Qt::Checked);
    } else {
        nxUi.checkboxDefaultPrivateKey->setChecked(false);
        nxUi.groupboxPrivateKey->setEnabled(true);
	setDefaultPrivateKey(Qt::Unchecked);
    }

    connect(nxUi.resolutionComboBox, SIGNAL(currentIndexChanged(int)), SLOT(updateWidthHeight(int)));
    connect(nxUi.checkboxDefaultPrivateKey, SIGNAL(stateChanged(int)), SLOT(setDefaultPrivateKey(int)));
    connect(nxUi.buttonImportPrivateKey, SIGNAL(pressed()), SLOT(chooseKeyFile()));
    connect(nxUi.kcfg_PrivateKey, SIGNAL(textChanged()), SLOT(updatePrivateKey()));

    QString resolutionString = QString::number(width()) + 'x' + QString::number(height());
    int resolutionIndex = nxUi.resolutionComboBox->findText(resolutionString, Qt::MatchContains);
    nxUi.resolutionComboBox->setCurrentIndex((resolutionIndex == -1) ? 5 : resolutionIndex);

    if (dialog->exec() == KDialog::Accepted) {
        kDebug(5013) << "NxHostPreferences config dialog accepted";

        setHeight(nxUi.kcfg_Height->value());
        setWidth(nxUi.kcfg_Width->value());
        setDesktopType(int2desktopType(nxUi.kcfg_DesktopType->currentIndex()));
        setKeyboardLayout(int2keymap(nxUi.kcfg_KeyboardLayout->currentIndex()));
    }
}

void NxHostPreferences::updateWidthHeight(int index)
{
    switch (index) {
    case 0:
        nxUi.kcfg_Height->setValue(480);
        nxUi.kcfg_Width->setValue(640);
        break;
    case 1:
        nxUi.kcfg_Height->setValue(600);
        nxUi.kcfg_Width->setValue(800);
        break;
    case 2:
        nxUi.kcfg_Height->setValue(768);
        nxUi.kcfg_Width->setValue(1024);
        break;
    case 3:
        nxUi.kcfg_Height->setValue(1024);
        nxUi.kcfg_Width->setValue(1280);
        break;
    case 4:
        nxUi.kcfg_Height->setValue(1200);
        nxUi.kcfg_Width->setValue(1600);
        break;
    case 5: {
        QDesktopWidget *desktop = QApplication::desktop();
        int currentScreen = desktop->screenNumber(nxUi.kcfg_Height);
        nxUi.kcfg_Height->setValue(desktop->screenGeometry(currentScreen).height());
        nxUi.kcfg_Width->setValue(desktop->screenGeometry(currentScreen).width());
        break;
    }
    case 6:
    default:
        break;
    }

    bool enabled = (index == 6) ? true : false;

    nxUi.kcfg_Height->setEnabled(enabled);
    nxUi.kcfg_Width->setEnabled(enabled);
    nxUi.heightLabel->setEnabled(enabled);
    nxUi.widthLabel->setEnabled(enabled);
}

void NxHostPreferences::setDefaultPrivateKey(int state)
{
    if(state == Qt::Checked) {
        setPrivateKey("default");
        nxUi.groupboxPrivateKey->setEnabled(false);
    } else if(state == Qt::Unchecked) {
        setPrivateKey("");
        nxUi.groupboxPrivateKey->setEnabled(true);
    }
}

void NxHostPreferences::updatePrivateKey() 
{
    m_privateKey = nxUi.kcfg_PrivateKey->toPlainText();
}

void NxHostPreferences::chooseKeyFile()
{
    QString fileName = QFileDialog::getOpenFileName(nxPage, i18n("Open DSA Key File"), QString("~/"), i18n("Key File (*.key);;All files (*.*)"));

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    QByteArray key;
    while (!file.atEnd()) {
        key += file.readLine();
    }

    nxUi.kcfg_PrivateKey->setPlainText(QString(key));
    setPrivateKey(QString(key));
}

void NxHostPreferences::readProtocolSpecificConfig()
{
    kDebug(5013) << "NxHostPreferences::readProtocolSpecificConfig";

    if (m_element.firstChildElement("height") != QDomElement())
        setHeight(m_element.firstChildElement("height").text().toInt());
    else
        setHeight(Settings::height());

    if (m_element.firstChildElement("width") != QDomElement())
        setWidth(m_element.firstChildElement("width").text().toInt());
    else
        setWidth(Settings::width());

    if (m_element.firstChildElement("desktopType") != QDomElement())
        setDesktopType(int2desktopType(m_element.firstChildElement("desktopType").text().toInt()));
    else
        setDesktopType(int2desktopType(Settings::desktopType()));

    if (m_element.firstChildElement("keyboardLayout") != QDomElement())
        setKeyboardLayout(int2keymap(m_element.firstChildElement("keyboardLayout").text().toInt()));
    else
        setKeyboardLayout(int2keymap(Settings::keyboardLayout()));

    if (m_element.firstChildElement("privateKey") != QDomElement())
        setPrivateKey(m_element.firstChildElement("privateKey").text());
    else
        setPrivateKey(Settings::privateKey());
}

void NxHostPreferences::saveProtocolSpecificConfig()
{
    kDebug(5013) << "NxHostPreferences::saveProtocolSpecificConfig";

    updateElement("height", QString::number(height()));
    updateElement("width", QString::number(width()));
    updateElement("desktopType", QString::number(desktopType2int(desktopType())));
    updateElement("keyboardLayout", QString::number(keymap2int(keyboardLayout())));
    updateElement("privateKey", privateKey());
}

void NxHostPreferences::setHeight(int height)
{
    if (height >= 0)
        m_height = height;
}

int NxHostPreferences::height()
{
    return m_height;
}

void NxHostPreferences::setWidth(int width)
{
    if (width >= 0)
        m_width = width;
}

int NxHostPreferences::width()
{
    return m_width;
}

void NxHostPreferences::setDesktopType(const QString &desktopType)
{
    if (!desktopType.isNull())
        m_desktopType = desktopType;
}

QString NxHostPreferences::desktopType() const
{
    return m_desktopType;
}

void NxHostPreferences::setKeyboardLayout(const QString &keyboardLayout)
{
    if (!keyboardLayout.isNull())
        m_keyboardLayout = keyboardLayout;
}

QString NxHostPreferences::keyboardLayout() const
{
    return m_keyboardLayout;
}

void NxHostPreferences::setPrivateKey(const QString &privateKey)
{
    if(!privateKey.isNull())
	    m_privateKey = privateKey;
}

QString NxHostPreferences::privateKey() const
{
    return m_privateKey;
}

#include "nxhostpreferences.moc"
