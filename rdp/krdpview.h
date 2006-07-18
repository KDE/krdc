/*
     krdpview.h, declaration of the KRdpView class
     Copyright (C) 2002 Arend van Beelen jr.

     This program is free software; you can redistribute it and/or modify it under the terms of the
     GNU General Public License as published by the Free Software Foundation; either version 2 of
     the License, or (at your option) any later version.

     This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
     without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See
     the GNU General Public License for more details.

     You should have received a copy of the GNU General Public License along with this program; if
     not, write to the Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston,
     MA 02111-1307 USA

     For any questions, comments or whatever, you may mail me at: arend@auton.nl
*/

#ifndef KRDPVIEW_H
#define KRDPVIEW_H

#include <qxembed.h>

#include "hostpreferences.h"
#include "kremoteview.h"

#define TCP_PORT_RDP 3389
#define RDP_LOGON_NORMAL 0x33

class KProcess;
class KRdpView;

class RdpContainer : public QXEmbed
{
	Q_OBJECT

	friend class KRdpView;

	public:
		RdpContainer(QWidget *parent = 0, const char *name = 0, WFlags f = 0);
		~RdpContainer();

	signals:
		void newEmbeddedWindow(WId window);

	protected:
		virtual void windowChanged(WId window);
		virtual bool x11Event(XEvent *e);

	private:
		bool m_viewOnly;                   // if set: ignore all input
};

class KRdpView : public KRemoteView
{
	Q_OBJECT

	public:
		// constructor and destructor
		KRdpView(QWidget *parent = 0, const char *name = 0,
		         const QString &host = QString::null, int port = TCP_PORT_RDP,
		         const QString &user = QString::null, const QString &password = QString::null,
		         int flags = RDP_LOGON_NORMAL, const QString &domain = QString::null,
		         const QString &shell = QString::null, const QString &directory = QString::null);
		virtual ~KRdpView();

		// functions regarding the window
		virtual QSize framebufferSize();         // returns the size of the remote view
		QSize sizeHint();                        // returns the suggested size
		virtual bool viewOnly();

		// functions regarding the connection
		virtual void startQuitting();            // start closing the connection
		virtual bool isQuitting();               // are we currently closing the connection?
		virtual QString host();                  // return the host we're connected to
		virtual int port();                      // return the port number we're connected on
		virtual bool start();                    // open a connection

		static bool editPreferences( HostPrefPtr );

	public slots:
		virtual void switchFullscreen(bool on);
		virtual void pressKey(XEvent *k);        // send a generated key to the server
		virtual void setViewOnly(bool s);

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
		RdpContainer *m_container;         // container for the rdesktop window
		KProcess *m_process;               // rdesktop process

	private slots:
		void connectionOpened(WId window); // called if rdesktop started
		void connectionClosed();           // called if rdesktop quits
		void processDied(KProcess *);      // called if rdesktop dies
		void receivedStderr(KProcess *proc, char *buffer, int buflen);
		                                   // catches rdesktop debug output
};

#endif
