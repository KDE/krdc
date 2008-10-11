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
#include <kfiledialog.h>

#include <QDesktopWidget>
#include <QFile>
#include <QByteArray>
#include <QTextEdit>

static const QStringList keymaps = (QStringList()
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
    const int index = keymaps.lastIndexOf(keymap);
    return (index == -1) ? defaultKeymap : index;
}

inline QString int2keymap(int layout)
{
    if (layout >= 0 && layout < keymaps.count())
        return keymaps.at(layout);
    else
        return keymaps.at(defaultKeymap);
}

static const QStringList desktopTypes = (QStringList()
    << "unix-kde"
    << "unix-gnome"
    << "unix-cde"
    << "unix-xdm"
);

static const int defaultDesktopType = 0;

inline int desktopType2int(const QString &desktopType)
{
    const int index = desktopTypes.lastIndexOf(desktopType);
    return (index == -1) ? defaultDesktopType : index;
}

inline QString int2desktopType(int index)
{
    if (index >= 0 && index < desktopTypes.count())
        return desktopTypes.at(index);
    else
        return desktopTypes.at(defaultDesktopType);
}

NxHostPreferences::NxHostPreferences(KConfigGroup configGroup, QObject *parent)
        : HostPreferences(configGroup, parent)
{
}

NxHostPreferences::~NxHostPreferences()
{
}

QWidget* NxHostPreferences::createProtocolSpecificConfigPage()
{
    nxPage = new QWidget();
    nxUi.setupUi(nxPage);

    nxUi.kcfg_NxHeight->setValue(height());
    nxUi.kcfg_NxWidth->setValue(width());
    nxUi.kcfg_NxDesktopType->setCurrentIndex(desktopType2int(desktopType()));
    nxUi.kcfg_NxKeyboardLayout->setCurrentIndex(keymap2int(keyboardLayout()));

    if (privateKey() == "default") {
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
    connect(nxUi.kcfg_NxPrivateKey, SIGNAL(textChanged()), SLOT(updatePrivateKey()));

    const QString resolutionString = QString::number(width()) + 'x' + QString::number(height());
    const int resolutionIndex = nxUi.resolutionComboBox->findText(resolutionString, Qt::MatchContains);
    nxUi.resolutionComboBox->setCurrentIndex((resolutionIndex == -1) ? 5 : resolutionIndex);

    return nxPage;
}

void NxHostPreferences::updateWidthHeight(int index)
{
    switch (index) {
    case 0:
        nxUi.kcfg_NxHeight->setValue(480);
        nxUi.kcfg_NxWidth->setValue(640);
        break;
    case 1:
        nxUi.kcfg_NxHeight->setValue(600);
        nxUi.kcfg_NxWidth->setValue(800);
        break;
    case 2:
        nxUi.kcfg_NxHeight->setValue(768);
        nxUi.kcfg_NxWidth->setValue(1024);
        break;
    case 3:
        nxUi.kcfg_NxHeight->setValue(1024);
        nxUi.kcfg_NxWidth->setValue(1280);
        break;
    case 4:
        nxUi.kcfg_NxHeight->setValue(1200);
        nxUi.kcfg_NxWidth->setValue(1600);
        break;
    case 5: {
        QDesktopWidget *desktop = QApplication::desktop();
        int currentScreen = desktop->screenNumber(nxUi.kcfg_NxHeight);
        nxUi.kcfg_NxHeight->setValue(desktop->screenGeometry(currentScreen).height());
        nxUi.kcfg_NxWidth->setValue(desktop->screenGeometry(currentScreen).width());
        break;
    }
    case 6:
    default:
        break;
    }

    bool enabled = (index == 6) ? true : false;

    nxUi.kcfg_NxHeight->setEnabled(enabled);
    nxUi.kcfg_NxWidth->setEnabled(enabled);
    nxUi.heightLabel->setEnabled(enabled);
    nxUi.widthLabel->setEnabled(enabled);
}

void NxHostPreferences::setDefaultPrivateKey(int state)
{
    if (state == Qt::Checked) {
        setPrivateKey("default");
        nxUi.groupboxPrivateKey->setEnabled(false);
    } else if (state == Qt::Unchecked) {
        setPrivateKey("");
        nxUi.groupboxPrivateKey->setEnabled(true);
    }
}

void NxHostPreferences::updatePrivateKey()
{
    m_privateKey = nxUi.kcfg_NxPrivateKey->toPlainText();
}

void NxHostPreferences::chooseKeyFile()
{
    const QString fileName = KFileDialog::getOpenFileName(KUrl(QDir::homePath()),
                                                          "*.key|" + i18n("Key Files (*.key)"),
                                                          nxPage, i18n("Open DSA Key File"));

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    QByteArray key;
    while (!file.atEnd())
        key += file.readLine();

    nxUi.kcfg_NxPrivateKey->setPlainText(QString(key));
    setPrivateKey(QString(key));
}

void NxHostPreferences::acceptConfig()
{
    HostPreferences::acceptConfig();

    setHeight(nxUi.kcfg_NxHeight->value());
    setWidth(nxUi.kcfg_NxWidth->value());
    setDesktopType(int2desktopType(nxUi.kcfg_NxDesktopType->currentIndex()));
    setKeyboardLayout(int2keymap(nxUi.kcfg_NxKeyboardLayout->currentIndex()));
}

void NxHostPreferences::setHeight(int height)
{
    if (height >= 0)
        m_configGroup.writeEntry("height", height);
}

int NxHostPreferences::height()
{
    return m_configGroup.readEntry("height", Settings::nxHeight());
}

void NxHostPreferences::setWidth(int width)
{
    if (width >= 0)
        m_configGroup.writeEntry("width", width);
}

int NxHostPreferences::width()
{
    return m_configGroup.readEntry("width", Settings::nxWidth());
}

void NxHostPreferences::setDesktopType(const QString &desktopType)
{
    if (!desktopType.isNull())
        m_configGroup.writeEntry("desktopType", desktopType2int(desktopType));
}

QString NxHostPreferences::desktopType() const
{
    return int2desktopType(m_configGroup.readEntry("desktopType", Settings::nxDesktopType()));
}

void NxHostPreferences::setKeyboardLayout(const QString &keyboardLayout)
{
    if (!keyboardLayout.isNull())
        m_configGroup.writeEntry("keyboardLayout", keymap2int(keyboardLayout));
}

QString NxHostPreferences::keyboardLayout() const
{
    return int2keymap(m_configGroup.readEntry("keyboardLayout", Settings::nxKeyboardLayout()));
}

void NxHostPreferences::setPrivateKey(const QString &privateKey)
{
    if (!privateKey.isNull())
        m_configGroup.writeEntry("privateKey", privateKey);
}

QString NxHostPreferences::privateKey() const
{
    return m_configGroup.readEntry("privateKey", Settings::nxPrivateKey());
}

#include "nxhostpreferences.moc"
