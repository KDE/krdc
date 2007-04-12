/*
     krdpview.h, declaration of the KRdpView class
     Copyright 2002 Arend van Beelen jr. <arend@auton.nl>

     This program is free software; you can redistribute it and/or modify it under the terms of the
     GNU General Public License as published by the Free Software Foundation; either version 2 of
     the License, or (at your option) any later version.

     This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
     without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See
     the GNU General Public License for more details.

     You should have received a copy of the GNU General Public License along with this program; if
     not, write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
     MA 02110-1301 USA

     For any questions, comments or whatever, you may mail me at: arend@auton.nl
*/

#ifndef KRDPVIEW_H
#define KRDPVIEW_H

#include "kremoteview.h"

#define TCP_PORT_RDP 3389
#define RDP_LOGON_NORMAL 0x33

class K3Process;
class KRdpView;
class QX11EmbedContainer;

class KRdpView : public KRemoteView
{
	Q_OBJECT

	public:
		// constructor and destructor
		KRdpView(QWidget *parent = 0,
		         const QString &host = QString(), int port = TCP_PORT_RDP,
		         const QString &user = QString(), const QString &password = QString(),
		         int flags = RDP_LOGON_NORMAL, const QString &domain = QString(),
		         const QString &shell = QString(), const QString &directory = QString(),
			 const QString &caption = QString());
		virtual ~KRdpView();

		// functions regarding the window
		virtual QSize framebufferSize();         // returns the size of the remote view
		QSize sizeHint() const;                        // returns the suggested size
		virtual bool viewOnly();

		// functions regarding the connection
		virtual void startQuitting();            // start closing the connection
		virtual bool isQuitting();               // are we currently closing the connection?
		virtual QString host();                  // return the host we're connected to
		virtual int port();                      // return the port number we're connected on
		virtual bool start();                    // open a connection

	public slots:
		virtual void switchFullscreen(bool on);
		virtual void pressKey(XEvent *k);        // send a generated key to the server
		virtual void setViewOnly(bool s);

	protected:
		bool eventFilter(QObject *obj, QEvent *event);

	private:
		// properties used for setting up the connection
		QString  m_name;       // name of the connection
		QString  m_host;       // the host to connect to
		int      m_port;       // the port on the host
		QString  m_user;       // the user to use to log in
		QString  m_password;   // the password to use
		int      m_flags;      // flags which determine how the connection is set up
		QString  m_domain;     // the domain where the host is on
		QString  m_shell;      // the shell to use
		QString  m_directory;  // the working directory on the server

		// other properties
		bool    m_quitFlag;                // if set: die
		QString m_clientVersion;           // version number returned by rdesktop
		QX11EmbedContainer *m_container;    // container for the rdesktop window
		K3Process *m_process;               // rdesktop process

		QString  m_caption;    // the caption to use on the window

		bool m_viewOnly; // if true filter out any input to the widget

	private slots:
		void connectionOpened();           // called if rdesktop started
		void connectionClosed();           // called if rdesktop quits
		void processDied(K3Process *);      // called if rdesktop dies
		void receivedStderr(K3Process *proc, char *buffer, int buflen);
		                                   // catches rdesktop debug output
};

#endif
