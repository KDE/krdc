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

#ifndef PREFERENCESDIALOG_H
#define PREFERENCESDIALOG_H

#include <KConfigDialog>

class KConfigSkeleton;
class KPluginSelector;

class PreferencesDialog : public KConfigDialog
{
    Q_OBJECT

public:
    PreferencesDialog(QWidget *parent, KConfigSkeleton *config);

protected:
    virtual bool isDefault();
    
private slots:
    void saveState();
    void loadDefaults();
    void settingsChanged();

private:
    KPluginSelector *m_pluginSelector;
    bool m_settingsChanged;
};

#endif
