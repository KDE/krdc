/*
    SPDX-FileCopyrightText: 2026 Tony Murray <murraytony@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "smartcardreaderfield.h"

#include <KLocalizedString>

#include <QSignalBlocker>

#include <winpr/smartcard.h>

SmartcardReaderField::SmartcardReaderField(QWidget *parent)
    : QComboBox(parent)
{
    refreshReaders();
    connect(this, &QComboBox::currentIndexChanged, this, [this]() {
        Q_EMIT readerNameChanged(readerName());
    });
}

QString SmartcardReaderField::readerName() const
{
    return currentData().toString();
}

void SmartcardReaderField::setReaderName(const QString &name)
{
    int index = findData(name);
    if (index < 0) {
        addItem(i18nc("@item:inlistbox smartcard reader, %1 is its saved name", "%1 (not detected)", name), name);
        index = count() - 1;
    }
    setCurrentIndex(index);
}

void SmartcardReaderField::showPopup()
{
    refreshReaders();
    QComboBox::showPopup();
}

void SmartcardReaderField::refreshReaders()
{
    const QString selected = readerName();
    const QSignalBlocker blocker(this);
    clear();
    addItem(i18nc("@item:inlistbox smartcard reader", "Not shared"), QString());
    addItem(i18nc("@item:inlistbox smartcard reader", "All readers"), AllReaders);

    SCARDCONTEXT context = 0;
    if (SCardEstablishContext(SCARD_SCOPE_USER, nullptr, nullptr, &context) == SCARD_S_SUCCESS) {
        char *readers = nullptr;
        DWORD length = SCARD_AUTOALLOCATE;
        if (SCardListReadersA(context, nullptr, reinterpret_cast<char *>(&readers), &length) == SCARD_S_SUCCESS) {
            const QByteArray names(readers, length);
            for (const QByteArray &name : names.split('\0')) {
                if (!name.isEmpty()) {
                    const QString reader = QString::fromLocal8Bit(name);
                    addItem(reader, reader);
                }
            }
            SCardFreeMemory(context, readers);
        }
        SCardReleaseContext(context);
    }
    setReaderName(selected);
}
