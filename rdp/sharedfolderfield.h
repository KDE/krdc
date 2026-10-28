/*
    SPDX-FileCopyrightText: 2026 Tony Murray <murraytony@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef SHAREDFOLDERFIELD_H
#define SHAREDFOLDERFIELD_H

#include <QComboBox>

/**
 * Picks the folder that an RDP session shares: none, the removable drives, a standard folder,
 * or any other folder. The folder is empty when nothing is shared, and otherwise
 * RemovableDrives or the path of a folder.
 */
class SharedFolderField : public QComboBox
{
    Q_OBJECT
    Q_PROPERTY(QString folder READ folder WRITE setFolder NOTIFY folderChanged USER true)

public:
    /** The folder that stands for the removable drives, including ones plugged in later. */
    static inline const QString RemovableDrives = QStringLiteral("DynamicDrives");

    explicit SharedFolderField(QWidget *parent = nullptr);

    QString folder() const;
    void setFolder(const QString &folder);

Q_SIGNALS:
    void folderChanged(const QString &folder);

private:
    int otherFolderIndex() const;
    void showFolderAt(int index);
    void chooseOtherFolder();

    QString m_folder;
    // The entry that shows m_folder; "Other Folder…" is only shown while its dialog is open.
    int m_folderIndex = 0;
    // The entry of a folder that is not offered, once there is one.
    int m_otherFolderEntry = -1;
};

#endif
