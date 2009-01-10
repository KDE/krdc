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

#include <kdemacros.h>

#include <KDialog>

#include <QDomDocument>

#include <QSize>

class QCheckBox;
class QWidget;
class KConfig;
class KConfigGroup;

class KDE_EXPORT HostPreferences : public QObject
{
    Q_OBJECT

public:
    ~HostPreferences();

    KConfigGroup configGroup();
    
    bool walletSupport();
    
    /** Whether scaling is enabled when session is full screen */
    bool fullscreenScale();
    void setFullscreenScale(bool scale);
    
    /** Whether scaling is enabled when session is not full screen */
    bool windowedScale();
    void setWindowedScale(bool scale);
    
    bool grabAllKeys();
    void setGrabAllKeys(bool grab);
    
    bool showLocalCursor();
    void setShowLocalCursor(bool show);
    
    bool viewOnly();
    void setViewOnly(bool view);
    
    /** 
     * Show the configuration dialog if needed, ie. if the user did 
     * check "show this dialog again for this host".
     * Returns true if user pressed ok. 
     */
    bool showDialogIfNeeded();
    
    /** Show the configuration dialog */
    bool showDialog();

protected:
    HostPreferences(KConfigGroup configGroup, QObject *parent);

    virtual QWidget* createProtocolSpecificConfigPage() = 0;
    
    /** Called when the user validates the config dialog. */
    virtual void acceptConfig();
    
    bool hostConfigured();
    bool showConfigAgain();

    KConfigGroup m_configGroup;

private:
    void setShowConfigAgain(bool show);
    void setWalletSupport(bool walletSupport);

    bool m_hostConfigured;

    QCheckBox *showAgainCheckBox;
    QCheckBox *walletSupportCheckBox;
};

#endif
