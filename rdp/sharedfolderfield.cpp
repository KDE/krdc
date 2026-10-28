/*
    SPDX-FileCopyrightText: 2026 Tony Murray <murraytony@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "sharedfolderfield.h"

#include <KLocalizedString>

#include <QDir>
#include <QFileDialog>
#include <QStandardPaths>

SharedFolderField::SharedFolderField(QWidget *parent)
    : QComboBox(parent)
{
    addItem(i18nc("@item:inlistbox shared folder", "Not shared"), QString());
    addItem(i18nc("@item:inlistbox shared folder", "Removable drives"), QStringLiteral("DynamicDrives"));
    // The home folder is not offered: it holds keys and passwords that a remote computer should
    // not see. A location without a folder of its own falls back to it, and is left out too.
    const auto addStandardFolder = [this](QStandardPaths::StandardLocation location, const QString &name) {
        const QString path = QStandardPaths::writableLocation(location);
        if (!path.isEmpty() && QDir(path) != QDir::home()) {
            addItem(name, path);
        }
    };
    addStandardFolder(QStandardPaths::DownloadLocation, i18nc("@item:inlistbox shared folder", "Downloads"));
    addStandardFolder(QStandardPaths::DocumentsLocation, i18nc("@item:inlistbox shared folder", "Documents"));
    insertSeparator(count());
    addItem(i18nc("@item:inlistbox shared folder", "Other Folder…"));

    // Only a choice of the user counts: adding the entry of another folder also moves the current
    // index while "Other Folder…" is chosen, which must not open the dialog again.
    connect(this, &QComboBox::activated, this, [this](int index) {
        if (index == otherFolderIndex()) {
            // The dialog opens once the combo box has closed its list.
            QMetaObject::invokeMethod(this, &SharedFolderField::chooseOtherFolder, Qt::QueuedConnection);
        } else {
            showFolderAt(index);
        }
    });
}

QString SharedFolderField::folder() const
{
    return m_folder;
}

void SharedFolderField::setFolder(const QString &folder)
{
    int index = findData(folder);
    if (index < 0) {
        // A folder that is not offered gets an entry of its own, which the next one replaces.
        const QString name = QDir(folder).dirName();
        const QString text = name.isEmpty() ? folder : i18nc("@item:inlistbox shared folder, %1 its name, %2 its path", "%1 (%2)", name, folder);
        if (m_otherFolderEntry < 0) {
            m_otherFolderEntry = otherFolderIndex() - 1;
            insertItem(m_otherFolderEntry, text, folder);
        } else {
            setItemText(m_otherFolderEntry, text);
            setItemData(m_otherFolderEntry, folder);
        }
        index = m_otherFolderEntry;
    }
    showFolderAt(index);
}

int SharedFolderField::otherFolderIndex() const
{
    return count() - 1;
}

void SharedFolderField::showFolderAt(int index)
{
    m_folderIndex = index;
    setCurrentIndex(index);
    const QString folder = itemData(index).toString();
    if (folder != m_folder) {
        m_folder = folder;
        Q_EMIT folderChanged(m_folder);
    }
}

void SharedFolderField::chooseOtherFolder()
{
    const QString start = QDir::isAbsolutePath(m_folder) ? m_folder : QDir::homePath();
    const QString chosen = QFileDialog::getExistingDirectory(this, i18nc("@title:window", "Choose Shared Folder"), start);
    if (chosen.isEmpty()) {
        showFolderAt(m_folderIndex);
    } else {
        setFolder(chosen);
    }
}
