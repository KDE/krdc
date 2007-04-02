/* This file is part of the KDE project
   Copyright (C) 2002-2003 Tim Jansen <tim@tjansen.de>
   Copyright (C) 2003-2004 Nadeem Hasan <nhasan@kde.org>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or ( at your option ) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include <config-krdc.h>

#include <QRegExp>
#include <QTimer>

#include <kcombobox.h>
#include <kdebug.h>
#include <klineedit.h>
#include <k3listview.h>
#include <klocale.h>
#include <kmessagebox.h>
#include <kpushbutton.h>
#include <QWhatsThis>

#include "hostpreferences.h"
#include "maindialogwidget.h"

static const QString DEFAULT_SCOPE = "default";
static const QString DNSSD_SCOPE = "DNS-SD";

class UrlListViewItem : public K3ListViewItem
{
  public:
    UrlListViewItem( Q3ListView *v, const QString &url, const QString &host,
        const QString &protocol, const QString &type, const QString &userid,
        const QString &fullname, const QString &desc,
        const QString &serviceid )
      : K3ListViewItem( v, host, i18n( "unknown" ), host, protocol ),
        m_url( url ), m_serviceid( serviceid )
    {
      if ( !type.isNull() )
      {
        //User connects to somebody else's desktop, used for krfb
        if ( type.toLower() == "shared" )
          setText( 1, i18n( "Shared Desktop" ) );
        //User connects to desktop that exists only on the network
        else if ( type.toLower() == "private" )
          setText( 1, i18n( "Standalone Desktop" ) );
      }
      if ( !desc.isNull() )
        setText( 0, desc );
      if ( ( !userid.isEmpty() ) && ( !fullname.isEmpty() ) )
        setText( 0, QString( "%1 (%2)" ).arg( fullname ).arg( userid ) );
      else if ( !userid.isNull() )
        setText( 0, userid );
      else if ( !fullname.isNull() )
        setText( 0, fullname );
   }

  QString url()
  {
    return m_url;
  }
  const QString& serviceid() const
  {
    return m_serviceid;
  }

  protected:
    QString m_url;
    QString m_serviceid;
};

MainDialogWidget::MainDialogWidget( QWidget *parent )
  : QWidget( parent ), m_scanning( false ), m_locator_dnssd_rfb(0),m_locator_dnssd_vnc(0)
{
    setupUi(this);
    // These should be folded back into the .ui, if we keep it.
    m_browseButton->setText( i18n( "Browse >>" ) );
    m_rescanButton->setText( i18n( "Rescan" ) );
    m_activeNoteLabel->setWordWrap( true );


    // signals and slots connections
    connect(m_browseButton, SIGNAL(clicked()), this, SLOT(toggleBrowsingArea()));
    connect(m_rescanButton, SIGNAL(clicked()), this, SLOT(rescan()));
    connect(m_serverInput, SIGNAL(textChanged(const QString&)),
	    this, SLOT(hostChanged(const QString&)));
    connect(m_searchInput, SIGNAL(returnPressed()), this, SLOT(rescan()));
    connect(m_activeNoteLabel, SIGNAL(linkActivated(const QString&)),
	    this, SLOT(exampleWhatsThis(const QString&)));

  HostPreferences *hp = HostPreferences::instance();
  QStringList list;

  list  = hp->serverCompletions();
  m_serverInput->completionObject()->setItems( list );
  list = hp->serverHistory();
  m_serverInput->setHistoryItems( list );

  m_searchInput->setTrapReturnKey( true );

  connect( m_browsingView,
      SIGNAL( selectionChanged( Q3ListViewItem * ) ),
      SLOT( itemSelected( Q3ListViewItem * ) ) );
  connect( m_browsingView,
      SIGNAL( doubleClicked( Q3ListViewItem *, const QPoint &, int ) ),
      SLOT( itemDoubleClicked( Q3ListViewItem * ) ) );
  connect( m_scopeCombo,
      SIGNAL( activated( const QString & ) ),
      SLOT( scopeSelected( const QString & ) ) );
  connect( m_serverInput,
      SIGNAL( returnPressed( const QString & ) ),
      SLOT( rescan() ) );

  bool showBrowse = hp->showBrowsingPanel();
  enableBrowsingArea( showBrowse );

  adjustSize();
}

/*
 *  Sets the strings of the subwidgets using the current
 *  language.
 */
void MainDialogWidget::languageChange()
{
    retranslateUi(this);
}

void MainDialogWidget::save()
{
  HostPreferences *hp = HostPreferences::instance();
  QStringList list;

  m_serverInput->addToHistory( m_serverInput->currentText() );
  list = m_serverInput->completionObject()->items();
  hp->setServerCompletions( list );
  list = m_serverInput->historyItems();
  hp->setServerHistory( list );

  hp->setShowBrowsingPanel( m_browsingPanel->isVisible() );
}

void MainDialogWidget::setRemoteHost( const QString &host )
{
  m_serverInput->setEditText( host );
}

QString MainDialogWidget::remoteHost()
{
  return m_serverInput->currentText();
}

void MainDialogWidget::hostChanged( const QString &text )
{
  emit hostValid(text.contains(QRegExp(":[0-9]+$")) ||
                 text.contains(QRegExp("^vnc:/.+")) ||
                 text.contains(QRegExp("^rdp:/.+")));
}

void MainDialogWidget::toggleBrowsingArea()
{
    enableBrowsingArea(!m_browsingPanel->isVisible());
}

void MainDialogWidget::exampleWhatsThis(const QString & link)
{
  if ( link == QString( "whatsthisExampleUrls" ) ) {
    QString whatsThisText( "<h3>Examples</h3>" );
    whatsThisText += "for a computer called 'megan':";
    whatsThisText += "<table>";
    whatsThisText += "<tr><td>megan:1</td><td>connect to the VNC server on 'megan' with display number 1</td></tr>";
    whatsThisText += "<tr><td>vnc:/megan:1</td><td>longer form for the same thing</td></tr>";
    whatsThisText += "<tr><td>rdp:/megan</td><td>connect to the RDP server on 'megan'</td></tr>";
    whatsThisText += "</table>";
    QWhatsThis::showText( QCursor::pos(), whatsThisText, this );
  } else {
    kDebug() << "got unexpected whatsThis link: " << link << endl;
  }
}

void MainDialogWidget::enableBrowsingArea( bool enable )
{
  int hOffset = 0;
  if (enable)
  {
    m_browsingPanel->show();
    m_browsingPanel->setMaximumSize(1000, 1000);
    m_browsingPanel->setEnabled(true);
    m_browseButton->setText(m_browseButton->text().replace(">>", "<<"));
  }
  else
  {
    hOffset = m_browsingPanel->height();
    m_browsingPanel->hide();
    m_browsingPanel->setMaximumSize(0, 0);
    m_browsingPanel->setEnabled(false);
    m_browseButton->setText(m_browseButton->text().replace("<<", ">>"));
    int h = minimumSize().height()-hOffset;
    setMinimumSize(minimumSize().width(), (h > 0) ? h : 0);
    resize(width(), height()-hOffset);

    QTimer::singleShot( 0, parentWidget(), SLOT( adjustWidgetSize() ) );
  }

  if (enable)
    rescan();
}

void MainDialogWidget::itemSelected( Q3ListViewItem *item )
{
  UrlListViewItem *u = ( UrlListViewItem* ) item;
  QRegExp rx( "^service:remotedesktop\\.kde:([^;]*)" );
  if ( rx.indexIn( u->url() ) < 0 )
    m_serverInput->setItemText( m_serverInput->currentIndex(), u->url() );
  else
    m_serverInput->setItemText( m_serverInput->currentIndex(), rx.cap( 1 ) );
}

void MainDialogWidget::itemDoubleClicked( Q3ListViewItem *item )
{
  itemSelected( item );
  emit accept();
}

void MainDialogWidget::scopeSelected( const QString &scope )
{
  QString s = scope;
  if ( s == i18n( "default" ) )
    s = DEFAULT_SCOPE;

  if ( m_scope == s )
    return;
  m_scope = s;
  rescan();
}

void MainDialogWidget::rescan()
{
  QStringList scopeList;

  if ( m_scanning )
    return;
  m_scanning = true;
  m_rescanButton->setEnabled( false );
  m_scopeCombo->setEnabled( false );
  if ( !ensureLocatorOpen() )
    return;

  m_browsingView->clear();

  if (m_locator_dnssd_rfb) {
    delete m_locator_dnssd_rfb;  // still active browsers
    m_locator_dnssd_rfb = 0;
    delete m_locator_vnc;
    m_locator_vnc=0;
  }

  if (m_scope == DNSSD_SCOPE) {
    kDebug() << "Scope is DNSSD\n";
    //FIXME: non-default domains too
    m_locator_dnssd_rfb = new DNSSD::ServiceBrowser("_rfb._tcp",true);
    m_locator_dnssd_vnc = new DNSSD::ServiceBrowser("_vnc._tcp",true);
    connect(m_locator_dnssd_rfb,SIGNAL(serviceAdded(DNSSD::RemoteService::Ptr)),
      SLOT(addedService(DNSSD::RemoteService::Ptr)));
    connect(m_locator_dnssd_rfb,SIGNAL(serviceRemoved(DNSSD::RemoteService::Ptr)),
      SLOT(removedService(DNSSD::RemoteService::Ptr)));
    connect(m_locator_dnssd_vnc,SIGNAL(serviceAdded(DNSSD::RemoteService::Ptr)),
      SLOT(addedService(DNSSD::RemoteService::Ptr)));
    connect(m_locator_dnssd_vnc,SIGNAL(serviceRemoved(DNSSD::RemoteService::Ptr)),
      SLOT(removedService(DNSSD::RemoteService::Ptr)));
    m_locator_dnssd_vnc->startBrowse();
    m_locator_dnssd_rfb->startBrowse();
    // now find scopes
    lastSignalServices(true);
  } else {
    QString filter;
    if ( !m_searchInput->text().trimmed().isEmpty() ) {
      QString ef = KServiceLocator::escapeFilter(
        m_searchInput->text().trimmed() );
      filter = "(|(|(description=*"+ef+"*)(username=*"+ef+"*))(fullname=*"+ef+"*))";
    }

    if ( !m_locator->findServices( "service:remotedesktop.kde",
                                   filter, m_scope ) ) {
      kWarning() << "Failure in findServices()" << endl;
      errorScanning();
      return;
    }
  }
}

bool MainDialogWidget::ensureLocatorOpen()
{
  if ( m_locator )
    return true;

  m_locator = new KServiceLocator();

  if ( !m_locator->available() ) {
#ifdef HAVE_SLP
    KMessageBox::error( 0,
        i18n( "Browsing the network is not possible. You probably "
              "did not install SLP support correctly." ),
        i18n( "Browsing Not Possible" ), false );
#endif
    return false;
  }

  connect( m_locator, SIGNAL( foundService( QString,int ) ),
                      SLOT( foundService( QString,int ) ) );
  connect( m_locator, SIGNAL( lastServiceSignal( bool ) ),
                      SLOT( lastSignalServices( bool ) ) );
  connect( m_locator, SIGNAL( foundScopes( QStringList ) ),
                      SLOT( foundScopes( QStringList ) ) );
   return true;
}

void MainDialogWidget::errorScanning()
{
  KMessageBox::error( 0,
      i18n( "An error occurred while scanning the network." ),
      i18n( "Error While Scanning" ), false );
  finishScanning();
}

void MainDialogWidget::finishScanning()
{
  m_rescanButton->setEnabled( true );
  m_scopeCombo->setEnabled( true );
  m_scanning = false;
}

void MainDialogWidget::foundService( QString url, int )
{
  QRegExp rx(  "^service:remotedesktop\\.kde:(\\w+)://([^;]+);(.*)$" );

  if ( rx.indexIn( url ) < 0 )
  {
    rx = QRegExp(  "^service:remotedesktop\\.kde:(\\w+)://(.*)$" );
    if ( rx.indexIn( url ) < 0 )
      return;
  }

  QMap<QString,QString> map;
  KServiceLocator::parseAttributeList( rx.cap( 3 ), map );

  new UrlListViewItem( m_browsingView, url, rx.cap( 2 ), rx.cap( 1 ),
      KServiceLocator::decodeAttributeValue( map[ "type" ] ),
      KServiceLocator::decodeAttributeValue( map[ "username" ] ),
      KServiceLocator::decodeAttributeValue( map[ "fullname" ] ),
      KServiceLocator::decodeAttributeValue( map[ "description" ] ),
      KServiceLocator::decodeAttributeValue( map[ "serviceid" ] ) );
}

void MainDialogWidget::addedService( DNSSD::RemoteService::Ptr service )
{
QString type = service->type().mid(1,3);
if (type == "rfb") type = "vnc";
QString url = type+"://"+service->hostName()+':'+QString::number(service->port());
new UrlListViewItem( m_browsingView, url, service->serviceName(),
     type.toUpper(),service->textData()["type"],
     service->textData()["u"],service->textData()["fullname"],
     service->textData()["description"],service->serviceName()+service->domain());
}

void MainDialogWidget::removedService( DNSSD::RemoteService::Ptr service )
{
  Q3ListViewItemIterator it( m_browsingView );
  while ( it.current() ) {
    if ( ((UrlListViewItem*)it.current())->serviceid() == service->serviceName()+service->domain() )
      delete it.current();
      else ++it;
  }
}


void MainDialogWidget::lastSignalServices( bool success )
{
  if ( !success )
  {
    errorScanning();
    return;
  }

  if ( !m_locator->findScopes() )
  {
    kWarning() << "Failure in findScopes()" << endl;
    errorScanning();
  }
}

void MainDialogWidget::foundScopes( QStringList scopeList )
{
  scopeList << DNSSD_SCOPE;

  int di = scopeList.indexOf( DEFAULT_SCOPE );
  if ( di >= 0 )
    scopeList[ di ] = i18n( "default" );

  int ct = scopeList.indexOf( m_scopeCombo->currentText() );
  m_scopeCombo->clear();
  m_scopeCombo->addItems( scopeList );
  if ( ct >= 0 )
    m_scopeCombo->setCurrentIndex( ct );
  finishScanning();
}

#include "maindialogwidget.moc"
