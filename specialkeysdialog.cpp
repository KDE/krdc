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

#include "specialkeysdialog.h"

#include <KLocale>

#include <QKeyEvent>
#include <QLabel>

SpecialKeysDialog::SpecialKeysDialog(QWidget *parent, RemoteView *remoteView)
        : KDialog(parent),
        m_remoteView(remoteView)
{
    setCaption(i18n("Special Keys"));
    setButtons(0);

    QLabel *descriptionLabel = new QLabel(i18n("<html>Enter a special key or a key combination to sent the the remote desktop."
                                          "<br /><br />This function allows you to send a key combination like Ctrl+Alt+Del to the remote computer."
                                          "<br /><br />When you have finished entering the special keys, close this window.</html>"), this);
    descriptionLabel->setWordWrap(true);
    descriptionLabel->setTextFormat(Qt::RichText);

    setMainWidget(descriptionLabel);
    setFocusPolicy(Qt::StrongFocus);
    setFocus();
}

SpecialKeysDialog::~SpecialKeysDialog()
{
    releaseKeyboard();
}

void SpecialKeysDialog::keyPressEvent(QKeyEvent *event)
{
//     kDebug(5010) << "key press" << event->key();

    m_remoteView->keyEvent(event);

    event->accept();
}

void SpecialKeysDialog::keyReleaseEvent(QKeyEvent *event)
{
//     kDebug(5010) << "key release" << event->key();

    m_remoteView->keyEvent(event);

    event->accept();
}

void SpecialKeysDialog::focusInEvent(QFocusEvent *event)
{
    Q_UNUSED(event);
    grabKeyboard();
}

void SpecialKeysDialog::focusOutEvent(QFocusEvent *event)
{
    Q_UNUSED(event);
    releaseKeyboard();
}

#include "specialkeysdialog.moc"
