#ifndef MAINDIALOGWIDGET_H
#define MAINDIALOGWIDGET_H

#include "kservicelocator.h"
#include "maindialogbase.h"
#include "smartptr.h"

class MainDialogWidget : public MainDialogBase
{
  Q_OBJECT

  public:
    MainDialogWidget( QWidget *parent, const char *name );
    ~MainDialogWidget() {}

    void setRemoteHost( const QString & );
    QString remoteHost();
    void save();

  protected:
    void enableBrowsingArea( bool );
    bool ensureLocatorOpen();
    void errorScanning();
    void finishScanning();

  signals:
    void hostValid( bool b );
    void accept();

  protected slots:
    void hostChanged( const QString & );
    void toggleBrowsingArea();
    void itemSelected( QListViewItem * );
    void itemDoubleClicked( QListViewItem * );
    void scopeSelected( const QString & );
    void rescan();

    void foundService( QString url, int );
    void lastSignalServices( bool success );
    void foundScopes( QStringList scopeList );

  protected:
    QString m_scope;
    bool m_scanning;
    SmartPtr<KServiceLocator> m_locator;
};

#endif // MAINDIALOGWIDGET_H
