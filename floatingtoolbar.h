/*
    SPDX-FileCopyrightText: 2007-2008 Urs Wolfer <uwolfer@kde.org>
    Parts of this file have been take from okular:
    SPDX-FileCopyrightText: 2004-2005 Enrico Ros <eros.kde@email.it>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef FLOATINGTOOLBAR_H
#define FLOATINGTOOLBAR_H

#include <QToolBar>

/**
 * @short A toolbar widget that slides in from a side.
 *
 * This is a shaped widget that slides in from a side of the 'anchor widget'
 * it's attached to. It can be dragged and docked on {left,top,right,bottom}
 * sides and contains actions.
 */
class FloatingToolBar : public QToolBar
{
    Q_OBJECT
public:
    FloatingToolBar(QWidget *parent, QWidget *anchorWidget);
    ~FloatingToolBar() override;

    enum Side {
        Left = 0,
        Top = 1,
        Right = 2,
        Bottom = 3,
    };
    Q_ENUM(Side)

    void addAction(QAction *action);
    void setSide(Side side);

Q_SIGNALS:
    void orientationChanged(int side);

public Q_SLOTS:
    void setSticky(bool sticky);
    void showAndAnimate();
    void hideAndDestroy();

protected:
    bool eventFilter(QObject *o, QEvent *e) override;
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void wheelEvent(QWheelEvent *e) override;

private:
    class FloatingToolBarPrivate *d;

private Q_SLOTS:
    void animate();
    void hide();
};

#endif
