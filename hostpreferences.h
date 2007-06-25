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

#ifndef HOSTPREFERENCES_H
#define HOSTPREFERENCES_H

#include "remoteview.h"

#include <KDialog>

#include <QDomDocument>

class QCheckBox;

class HostPreferences : public QObject
{
    Q_OBJECT

public:
    HostPreferences(const QString &url, QObject *parent);
    ~HostPreferences();

protected:
    virtual void showDialog() = 0;
    virtual void readProtocolSpecificConfig() = 0;
    virtual void saveProtocolSpecificConfig() = 0;
    void updateElement(const QString &name, const QString &value);
    void readConfig();
    bool saveConfig();
    bool hostConfigured();
    bool showConfigAgain();
    KDialog *createDialog(QWidget *widget);

    QDomElement m_element;

private:
    void setShowConfigAgain(bool show);

    QString m_filename;
    QString m_url;
    QDomDocument m_doc;
    QCheckBox *showAgainCheckBox;
    bool m_showConfigAgain;
};

#endif
