/*
    SPDX-FileCopyrightText: 2007 Urs Wolfer <uwolfer@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef PREFERENCESDIALOG_H
#define PREFERENCESDIALOG_H

#include <KConfigWidgets/KConfigDialog>

#include <QDialogButtonBox>

class KConfigSkeleton;
class KPluginSelector;

class PreferencesDialog : public KConfigDialog
{
    Q_OBJECT

public:
    PreferencesDialog(QWidget *parent, KConfigSkeleton *config);

protected:
    bool isDefault() override;

private Q_SLOTS:
    void saveState();
    void loadDefaults();
    void settingsChanged();

private:
    KPluginSelector *m_pluginSelector;
    bool m_settingsChanged;
    void enableButton(QDialogButtonBox::StandardButton standardButton);
};

#endif
