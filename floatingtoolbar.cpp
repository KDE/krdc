/****************************************************************************
**
** Copyright (C) 2007 Urs Wolfer <uwolfer @ kde.org>
** Parts of this file have been take from okular:
** Copyright (C) 2004-2005 Enrico Ros <eros.kde@email.it>
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

#include "floatingtoolbar.h"

#include <KDebug>

#include <QApplication>
#include <QBitmap>
#include <QMouseEvent>
#include <QLinkedList>
#include <QPainter>
#include <QPaintEvent>
#include <QTimer>
#include <QToolButton>
#include <QWheelEvent>

#include <math.h>

static const int iconSize = 22;
static const int buttonSize = 28;
static const int toolBarGridSize = 28;
static const int toolBarRBMargin = 2;
static const qreal toolBarOpacity = 0.8;
static const int visiblePixelWhenAutoHidden = 3;
static const int autoHideTimeout = 2000;

class FloatingToolBarPrivate
{
public:
    FloatingToolBarPrivate(FloatingToolBar *qq)
            : q(qq) {
    }

    // rebuild contents and reposition then widget
    void buildToolBar();
    void reposition();
    // compute the visible and hidden positions along current side
    QPoint getInnerPoint() const;
    QPoint getOuterPoint() const;

    FloatingToolBar *q;

    QWidget *anchorWidget;
    FloatingToolBar::Side anchorSide;

    QTimer *animTimer;
    QTimer *autoHideTimer;
    QPoint currentPosition;
    QPoint endPosition;
    bool hiding;
    bool toDelete;
    bool visible;
    bool sticky;
    qreal opacity;

    QPixmap backgroundPixmap;
    QLinkedList<QToolButton*> buttons;
};

FloatingToolBar::FloatingToolBar(QWidget *parent, QWidget *anchorWidget)
        : QWidget(parent), d(new FloatingToolBarPrivate(this))
{
    setMouseTracking(true);

    d->anchorWidget = anchorWidget;
    d->anchorSide = Left;
    d->hiding = false;
    d->toDelete = false;
    d->visible = false;
    d->sticky = false;
    d->opacity = toolBarOpacity;

    d->animTimer = new QTimer(this);
    connect(d->animTimer, SIGNAL(timeout()), this, SLOT(slotAnimate()));

    d->autoHideTimer = new QTimer(this);
    connect(d->autoHideTimer, SIGNAL(timeout()), this, SLOT(hide()));
    d->autoHideTimer->start(autoHideTimeout);

    // apply a filter to get notified when anchor changes geometry
    d->anchorWidget->installEventFilter(this);
}

FloatingToolBar::~FloatingToolBar()
{
    delete d;
}

void FloatingToolBar::addAction(QAction *action)
{
    QToolButton *button = new QToolButton(this);
    button->setDefaultAction(action);
    button->setCheckable(true);
    button->setAutoRaise(true);
    button->resize(buttonSize, buttonSize);
    button->setIconSize(QSize(iconSize, iconSize));

    d->buttons.append(button);

    button->setMouseTracking(true);
    button->installEventFilter(this);

    // rebuild toolbar shape and contents
    d->reposition();
}

void FloatingToolBar::setSide(Side side)
{
    d->anchorSide = side;

    d->reposition();
}

void FloatingToolBar::setSticky(bool sticky)
{
    d->sticky = sticky;

    if (sticky)
        d->autoHideTimer->stop();
    else
        d->autoHideTimer->start(autoHideTimeout);
}

void FloatingToolBar::showAndAnimate()
{
    d->hiding = false;

    show();

    // start scrolling in
    d->animTimer->start(20);
}

void FloatingToolBar::hideAndDestroy()
{
    // set parameters for sliding out
    d->hiding = true;
    d->toDelete = true;
    d->endPosition = d->getOuterPoint();

    // start scrolling out
    d->animTimer->start(20);
}

void FloatingToolBar::hide()
{
    if (d->visible) {
        QPoint diff;
        switch (d->anchorSide) {
        case Left:
            diff = QPoint(visiblePixelWhenAutoHidden, 0);
            break;
        case Right:
            diff = QPoint(-visiblePixelWhenAutoHidden, 0);
            break;
        case Top:
            diff = QPoint(0, visiblePixelWhenAutoHidden);
            break;
        case Bottom:
            diff = QPoint(0, -visiblePixelWhenAutoHidden);
            break;
        }
        d->hiding = true;
        d->endPosition = d->getOuterPoint() + diff;
    }

    // start scrolling out
    d->animTimer->start(20);
}

bool FloatingToolBar::eventFilter(QObject *obj, QEvent *e)
{
    // if anchorWidget changed geometry reposition toolbar
    if (obj == d->anchorWidget && e->type() == QEvent::Resize) {
        d->animTimer->stop();
        if (d->hiding && d->toDelete)
            deleteLater();
        else
            d->reposition();
    }

    // keep toolbar visible when mouse is on it
    if (e->type() == QEvent::MouseMove && d->autoHideTimer->isActive())
        d->autoHideTimer->start(autoHideTimeout);

    return false;
}

void FloatingToolBar::paintEvent(QPaintEvent *e)
{
    // paint the internal pixmap over the widget
    QPainter p(this);
    p.setOpacity(d->opacity);
    p.drawImage(e->rect().topLeft(), d->backgroundPixmap.toImage(), e->rect());
}

void FloatingToolBar::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton)
        setCursor(Qt::SizeAllCursor);
}

void FloatingToolBar::mouseMoveEvent(QMouseEvent *e)
{
    // keep toolbar visible when mouse is on it
    if (d->autoHideTimer->isActive())
        d->autoHideTimer->start(autoHideTimeout);

    // show the toolbar again when it is auto-hidden
    if (!d->visible && !d->animTimer->isActive()) {
        d->hiding = false;
        show();
        d->endPosition = QPoint();
        d->reposition();

        d->animTimer->start(20);

        return;
    }

    if ((QApplication::mouseButtons() & Qt::LeftButton) != Qt::LeftButton)
        return;

    // compute the nearest side to attach the widget to
    QPoint parentPos = mapToParent(e->pos());
    float nX = (float)parentPos.x() / (float)d->anchorWidget->width();
    float nY = (float)parentPos.y() / (float)d->anchorWidget->height();
    if (nX > 0.3 && nX < 0.7 && nY > 0.3 && nY < 0.7)
        return;
    bool LT = nX < (1.0 - nY);
    bool LB = nX < (nY);
    Side side = LT ? (LB ? Left : Top) : (LB ? Bottom : Right);

    // check if side changed
    if (side == d->anchorSide)
        return;

    d->anchorSide = side;
    d->reposition();
    emit orientationChanged((int)side);
}

void FloatingToolBar::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton)
        setCursor(Qt::ArrowCursor);
}

void FloatingToolBar::wheelEvent(QWheelEvent *e)
{
    e->accept();

    qreal diff = e->delta() / 100.0 / 15.0;
//     kDebug(5010) << diff;
    if (((d->opacity <= 1) && (diff > 0)) || ((d->opacity >= 0) && (diff < 0)))
        d->opacity += diff;

    update();
}

void FloatingToolBarPrivate::buildToolBar()
{
    int buttonsNumber = buttons.count();
    int parentWidth = anchorWidget->width();
    int parentHeight = anchorWidget->height();
    int myCols = 1;
    int myRows = 1;

    // 1. find out columns and rows we're going to use
    bool topLeft = anchorSide == FloatingToolBar::Left || anchorSide == FloatingToolBar::Top;
    bool vertical = anchorSide == FloatingToolBar::Left || anchorSide == FloatingToolBar::Right;
    if (vertical) {
        myCols = 1 + (buttonsNumber * toolBarGridSize) /
                 (parentHeight - toolBarGridSize);
        myRows = (int)ceil((float)buttonsNumber / (float)myCols);
    } else {
        myRows = 1 + (buttonsNumber * toolBarGridSize) /
                 (parentWidth - toolBarGridSize);
        myCols = (int)ceil((float)buttonsNumber / (float)myRows);
    }

    // 2. compute widget size (from rows/cols)
    int myWidth = myCols * toolBarGridSize;
    int myHeight = myRows * toolBarGridSize;
    int xOffset = (toolBarGridSize - buttonSize) / 2;
    int yOffset = (toolBarGridSize - buttonSize) / 2;

    if (vertical) {
        myHeight += 16;
        myWidth += 4;
        yOffset += 12;
        if (anchorSide == FloatingToolBar::Right)
            xOffset += 4;
    } else {
        myWidth += 16;
        myHeight += 4;
        xOffset += 12;
        if (anchorSide == FloatingToolBar::Bottom)
            yOffset += 4;
    }

    bool prevUpdates = q->updatesEnabled();
    q->setUpdatesEnabled(false);

    // 3. resize pixmap, mask and widget
    QBitmap mask(myWidth + 1, myHeight + 1);
    backgroundPixmap = QPixmap(myWidth + 1, myHeight + 1);
    backgroundPixmap.fill(Qt::transparent);
    q->resize(myWidth + 1, myHeight + 1);

    // 4. create and set transparency mask
    QPainter maskPainter(&mask);
    mask.fill(Qt::white);
    maskPainter.setBrush(Qt::black);
    if (vertical)
        maskPainter.drawRoundRect(topLeft ? -10 : 0, 0, myWidth + 10, myHeight, 2000 / (myWidth + 10), 2000 / myHeight);
    else
        maskPainter.drawRoundRect(0, topLeft ? -10 : 0, myWidth, myHeight + 10, 2000 / myWidth, 2000 / (myHeight + 10));
    maskPainter.end();
    q->setMask(mask);

    // 5. draw background
    QPainter bufferPainter(&backgroundPixmap);
    bufferPainter.translate(0.5, 0.5);
    QPalette pal = q->palette();
    // 5.1. draw horizontal/vertical gradient
    QLinearGradient grad;
    switch (anchorSide) {
    case FloatingToolBar::Left:
        grad = QLinearGradient(0, 1, myWidth + 1, 1);
        break;
    case FloatingToolBar::Right:
        grad = QLinearGradient(myWidth + 1, 1, 0, 1);
        break;
    case FloatingToolBar::Top:
        grad = QLinearGradient(1, 0, 1, myHeight + 1);
        break;
    case FloatingToolBar::Bottom:
        grad = QLinearGradient(1, myHeight + 1, 0, 1);
        break;
    }
    grad.setColorAt(0, pal.color(QPalette::Active, QPalette::Button));
    grad.setColorAt(1, pal.color(QPalette::Active, QPalette::Light));
    bufferPainter.setBrush(QBrush(grad));
    // 5.2. draw rounded border
    bufferPainter.setPen( pal.color(QPalette::Active, QPalette::Dark).lighter(40));
    bufferPainter.setRenderHints(QPainter::Antialiasing);
    if (vertical)
        bufferPainter.drawRoundRect(topLeft ? -10 : 0, 0, myWidth + 10, myHeight, 2000 / (myWidth + 10), 2000 / myHeight);
    else
        bufferPainter.drawRoundRect(0, topLeft ? -10 : 0, myWidth, myHeight + 10, 2000 / myWidth, 2000 / (myHeight + 10));
    // 5.3. draw handle
    bufferPainter.translate(-0.5, -0.5);
    bufferPainter.setPen(pal.color(QPalette::Active, QPalette::Mid));
    if (vertical) {
        int dx = anchorSide == FloatingToolBar::Left ? 2 : 4;
        bufferPainter.drawLine(dx, 6, dx + myWidth - 8, 6);
        bufferPainter.drawLine(dx, 9, dx + myWidth - 8, 9);
        bufferPainter.setPen(pal.color(QPalette::Active, QPalette::Light));
        bufferPainter.drawLine(dx + 1, 7, dx + myWidth - 7, 7);
        bufferPainter.drawLine(dx + 1, 10, dx + myWidth - 7, 10);
    } else {
        int dy = anchorSide == FloatingToolBar::Top ? 2 : 4;
        bufferPainter.drawLine(6, dy, 6, dy + myHeight - 8);
        bufferPainter.drawLine(9, dy, 9, dy + myHeight - 8);
        bufferPainter.setPen(pal.color(QPalette::Active, QPalette::Light));
        bufferPainter.drawLine(7, dy + 1, 7, dy + myHeight - 7);
        bufferPainter.drawLine(10, dy + 1, 10, dy + myHeight - 7);
    }

    // 6. reposition buttons (in rows/col grid)
    int gridX = 0;
    int gridY = 0;
    QLinkedList<QToolButton*>::const_iterator it = buttons.begin(), end = buttons.end();
    for (; it != end; ++it) {
        QToolButton *button = *it;
        button->move(gridX * toolBarGridSize + xOffset,
                     gridY * toolBarGridSize + yOffset);
        button->show();
        if (++gridX == myCols) {
            gridX = 0;
            gridY++;
        }
    }

    q->setUpdatesEnabled(prevUpdates);
}

void FloatingToolBarPrivate::reposition()
{
    // note: hiding widget here will gives better gfx, but ends drag operation
    // rebuild widget and move it to its final place
    buildToolBar();
    if (!visible) {
        currentPosition = getOuterPoint();
        endPosition = getInnerPoint();
    } else {
        currentPosition = getInnerPoint();
        endPosition = getOuterPoint();
    }
    q->move(currentPosition);

    // repaint all buttons (to update background)
    QLinkedList<QToolButton*>::const_iterator it = buttons.begin(), end = buttons.end();
    for (; it != end; ++it)
        (*it)->update();
}

QPoint FloatingToolBarPrivate::getInnerPoint() const
{
    // returns the final position of the widget
    if (anchorSide == FloatingToolBar::Left)
        return QPoint(0, (anchorWidget->height() - q->height()) / 2);
    if (anchorSide == FloatingToolBar::Top)
        return QPoint((anchorWidget->width() - q->width()) / 2, 0);
    if (anchorSide == FloatingToolBar::Right)
        return QPoint(anchorWidget->width() - q->width() + toolBarRBMargin, (anchorWidget->height() - q->height()) / 2);
    return QPoint((anchorWidget->width() - q->width()) / 2, anchorWidget->height() - q->height() + toolBarRBMargin);
}

QPoint FloatingToolBarPrivate::getOuterPoint() const
{
    // returns the point from which the transition starts
    if (anchorSide == FloatingToolBar::Left)
        return QPoint(-q->width(), (anchorWidget->height() - q->height()) / 2);
    if (anchorSide == FloatingToolBar::Top)
        return QPoint((anchorWidget->width() - q->width()) / 2, -q->height());
    if (anchorSide == FloatingToolBar::Right)
        return QPoint(anchorWidget->width() + toolBarRBMargin, (anchorWidget->height() - q->height()) / 2);
    return QPoint((anchorWidget->width() - q->width()) / 2, anchorWidget->height() + toolBarRBMargin);
}

void FloatingToolBar::slotAnimate()
{
    // move currentPosition towards endPosition
    int dX = d->endPosition.x() - d->currentPosition.x();
    int dY = d->endPosition.y() - d->currentPosition.y();
    dX = dX / 6 + qMax(-1, qMin(1, dX));
    dY = dY / 6 + qMax(-1, qMin(1, dY));
    d->currentPosition.setX(d->currentPosition.x() + dX);
    d->currentPosition.setY(d->currentPosition.y() + dY);

    move(d->currentPosition);

    // handle arrival to the end
    if (d->currentPosition == d->endPosition) {
        d->animTimer->stop();
        if (d->hiding) {
            d->visible = false;
            if (d->toDelete)
                deleteLater();
        } else
            d->visible = true;
    }
}

#include "floatingtoolbar.moc"
