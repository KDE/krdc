/*
    SPDX-FileCopyrightText: 2026 Tony Murray <murraytony@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef SMARTCARDREADERFIELD_H
#define SMARTCARDREADERFIELD_H

#include <QComboBox>

class SmartcardReaderField : public QComboBox
{
    Q_OBJECT
    Q_PROPERTY(QString readerName READ readerName WRITE setReaderName NOTIFY readerNameChanged USER true)

public:
    /** Stored value for sharing every reader without a name filter. */
    static inline const QString AllReaders = QStringLiteral("*");

    explicit SmartcardReaderField(QWidget *parent = nullptr);

    QString readerName() const;
    void setReaderName(const QString &name);
    void showPopup() override;

Q_SIGNALS:
    void readerNameChanged(const QString &name);

private:
    void refreshReaders();
};

#endif
