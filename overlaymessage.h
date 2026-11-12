/*
    SPDX-FileCopyrightText: 2026 Tony Murray <murraytony@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef OVERLAYMESSAGE_H
#define OVERLAYMESSAGE_H

#include <QLabel>
#include <QTimer>

/**
 * A message shown at the bottom of its parent.
 * It never takes focus or mouse input.
 */
class OverlayMessage : public QLabel
{
    Q_OBJECT

public:
    explicit OverlayMessage(QWidget *parent);

    /**
     * Shows @p text until hidden, or for @p timeout ms if not 0.
     */
    void showMessage(const QString &text, int timeout = 0);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    void reposition();

    QTimer m_hideTimer;
};

#endif
