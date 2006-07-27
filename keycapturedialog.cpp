/*
   Copyright (C) 2002-2003 Tim Jansen <tim@tjansen.de>
   Copyright (C) 2004 Nadeem Hasan <nhasan@kde.org>
*/
/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

//
// based on key capture code from kdelibs/kdeui/kshortcutdialog.cpp
//

#include "keycapturedialog.h"
#include "keycapturewidget.h"

#include <QLabel>
#include <QVBoxLayout>
#include <QFrame>

#include <klocale.h>

#define XK_XKB_KEYS
#define XK_MISCELLANY
#include <X11/Xlib.h>	// For x11Event()
#include <X11/keysymdef.h> // For XK_...

#ifdef KeyPress
const int XFocusOut = FocusOut;
const int XFocusIn = FocusIn;
const int XKeyPress = KeyPress;
const int XKeyRelease = KeyRelease;
#undef KeyRelease
#undef KeyPress
#undef FocusOut
#undef FocusIn
#endif


KeyCaptureDialog::KeyCaptureDialog(QWidget *parent, const char *name)
  : KDialog( parent ), m_grabbed(false) {
  setObjectName( name );
  setModal( true );
  setCaption( i18n( "Enter Key Combination" ) );
  setButtons( Cancel );
  setDefaultButton( Cancel );
  showButtonSeparator( true );

  QFrame *main = new QFrame();
  setMainWidget( main );

  QVBoxLayout *layout = new QVBoxLayout( main );
  layout->setSpacing( KDialog::spacingHint() );
  layout->setMargin( 0 );
  m_captureWidget = new KeyCaptureWidget( main, "m_captureWidget" );
  layout->addWidget( m_captureWidget );
  layout->addStretch();
}

KeyCaptureDialog::~KeyCaptureDialog() {
	if (m_grabbed)
		releaseKeyboard();
}

void KeyCaptureDialog::execute() {
	m_captureWidget->keyLabel->setText("");
	exec();
	if (m_grabbed)
		releaseKeyboard();
}

bool KeyCaptureDialog::x11Event(XEvent *pEvent)
{
	switch( pEvent->type ) {
		case XKeyPress:
		case XKeyRelease:
			x11EventKeyPress( pEvent );
			return true;
		case XFocusIn:
			if (!m_grabbed)
				grabKeyboard();
			return true;
		case XFocusOut:
			if (m_grabbed)
				releaseKeyboard();
			return true;
		default:
			break;
	}
	return QWidget::x11Event( pEvent );
}

void KeyCaptureDialog::x11EventKeyPress( XEvent *pEvent )
{
#warning PortMe
#if 0
	// taken from kshortcutdialog.h
	KKeyNative keyNative( pEvent );
	uint keyModX = keyNative.mod(), keySymX = keyNative.sym();
	if ((keySymX == XK_Escape) && !keyModX) {
		accept();
		return;
	}

	switch( keySymX ) {
		// Don't allow setting a modifier key as an accelerator.
		// Also, don't release the focus yet.  We'll wait until
		//  we get a 'normal' key.
		case XK_Shift_L:   case XK_Shift_R:   keyModX = KKeyNative::modXShift(); break;
		case XK_Control_L: case XK_Control_R: keyModX = KKeyNative::modXCtrl(); break;
		case XK_Alt_L:     case XK_Alt_R:     keyModX = KKeyNative::modXAlt(); break;
		// FIXME: check whether the Meta or Super key are for the Win modifier
		case XK_Meta_L:    case XK_Meta_R:
		case XK_Super_L:   case XK_Super_R:   keyModX = KKeyNative::modXWin(); break;
		case XK_Hyper_L:   case XK_Hyper_R:
		case XK_Mode_switch:
		case XK_Num_Lock:
		case XK_Caps_Lock:
			break;
		default:
			if( pEvent->type == XKeyPress && keyNative.sym() ) {
				emit keyPressed(pEvent);
				reject();
			}
			return;
	}

	// If we are editing the first key in the sequence,
	//  display modifier keys which are held down
	if( pEvent->type == XKeyPress )
		keyModX |= pEvent->xkey.state;
	else
		keyModX = pEvent->xkey.state & ~keyModX;

	QString keyModStr;
	if( keyModX & KKeyNative::modXWin() )	keyModStr += KKey::modFlagLabel(KKey::WIN) + '+';
	if( keyModX & KKeyNative::modXAlt() )	keyModStr += KKey::modFlagLabel(KKey::ALT) + '+';
	if( keyModX & KKeyNative::modXCtrl() )	keyModStr += KKey::modFlagLabel(KKey::CTRL) + '+';
	if( keyModX & KKeyNative::modXShift() )	keyModStr += KKey::modFlagLabel(KKey::SHIFT) + '+';

	// Display currently selected modifiers, or redisplay old key.
	m_captureWidget->keyLabel->setText( keyModStr );
#endif
}

#include "keycapturedialog.moc"

