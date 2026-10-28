/*
    SPDX-FileCopyrightText: 2026 Tony Murray <murraytony@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef SETTINGSFORM_H
#define SETTINGSFORM_H

#include <QAbstractButton>
#include <QCheckBox>
#include <QFormLayout>
#include <QHash>
#include <QLabel>
#include <QList>
#include <QTabWidget>
#include <QWidget>

/**
 * Gives the forms on each tab, and the forms outside of the tabs, one label column, so that
 * their fields line up. Forms in @p excluded are left alone.
 */
inline void alignLabelColumns(QWidget *root, const QWidget *excluded = nullptr)
{
    QHash<QWidget *, QList<QWidget *>> labelsByTab;
    const auto forms = root->findChildren<QFormLayout *>();
    for (QFormLayout *form : forms) {
        QWidget *owner = form->parentWidget();
        if (excluded && (owner == excluded || excluded->isAncestorOf(owner))) {
            continue;
        }
        QWidget *tab = root;
        for (QWidget *widget = owner; widget != root; widget = widget->parentWidget()) {
            if (qobject_cast<QTabWidget *>(widget->parentWidget()->parentWidget())) {
                tab = widget;
                break;
            }
        }
        form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
        for (int row = 0; row < form->rowCount(); ++row) {
            // A check box that labels its row keeps its size, so the form can align it.
            QLayoutItem *item = form->itemAt(row, QFormLayout::LabelRole);
            if (item && item->widget() && !qobject_cast<QAbstractButton *>(item->widget())) {
                labelsByTab[tab].append(item->widget());
            }
        }
    }

    for (const QList<QWidget *> &labels : std::as_const(labelsByTab)) {
        int width = 0;
        for (QWidget *label : labels) {
            label->setMinimumWidth(0);
            width = qMax(width, label->sizeHint().width());
        }
        for (QWidget *label : labels) {
            label->setMinimumWidth(width);
            if (auto *textLabel = qobject_cast<QLabel *>(label)) {
                textLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            }
        }
    }
}

/**
 * Shows the rows of @p fields in @p form only while @p check is checked.
 */
inline void showRowsWhileChecked(QFormLayout *form, QCheckBox *check, const QList<QWidget *> &fields)
{
    const auto showFields = [form, fields](bool show) {
        for (QWidget *field : fields) {
            form->setRowVisible(field, show);
        }
    };
    showFields(check->isChecked());
    QObject::connect(check, &QCheckBox::toggled, check, showFields);
}

#endif
