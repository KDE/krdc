/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "hostprofiles.h"

/*
 *  Constructs a HostProfiles widget as a child of 'parent'
 */
HostProfiles::HostProfiles(QWidget* parent)
    : QWidget(parent)
{
    setupUi(this);

    connect(removeHostButton, SIGNAL(clicked()), this, SLOT(removeHost()));
    connect(removeAllButton, SIGNAL(clicked()), this, SLOT(removeAllHosts()));
    connect(hostListView, SIGNAL(selectionChanged()), this, SLOT(selectionChanged()));
}

HostProfiles::~HostProfiles()
{
}

void HostProfiles::removeHost()
{
  HostPreferences *hps = HostPreferences::instance();

  Q3ListViewItemIterator it(hostListView);
  while (it.current())
  {
    Q3ListViewItem *vi = it.current();
    if (vi->isSelected())
    {
      HostPrefPtr hp = hps->getHostPref(vi->text(0), vi->text(1));
      if (hp)
        deletedHosts += hp;
      delete vi;
    }
    else
      ++it;
  }
  removeAllButton->setEnabled(hostListView->childCount() > 0);
}

void HostProfiles::removeAllHosts()
{
  HostPreferences *hps = HostPreferences::instance();

  Q3ListViewItemIterator it(hostListView);
  while (it.current())
  {
    Q3ListViewItem *vi = it.current();
    HostPrefPtr hp = hps->getHostPref(vi->text(0), vi->text(1));
    if (hp)
      deletedHosts += hp;
    ++it;
  }
  hostListView->clear();
  removeAllButton->setEnabled(false);
}


void HostProfiles::selectionChanged()
{
  Q3ListViewItemIterator it(hostListView);
  while (it.current())
  {
    if (it.current()->isSelected())
    {
      removeHostButton->setEnabled(true);
      return;
    }
    ++it;
  }
  removeHostButton->setEnabled(false);
}


void HostProfiles::load()
{
  HostPreferences *hps = HostPreferences::instance();

  HostPrefPtrList hplist = hps->getAllHostPrefs();
  HostPrefPtrList::iterator it = hplist.begin();
  while ( it != hplist.end() )
  {
    HostPref *hp = *it;
    new K3ListViewItem( hostListView, hp->host(), hp->type(),
        hp->prefDescription() );
    ++it;
  }
}


void HostProfiles::save()
{
  HostPreferences *hps = HostPreferences::instance();

  HostPrefPtrList::iterator it = deletedHosts.begin();
  while (it != deletedHosts.end())
  {
    hps->removeHostPref(*it);
    it++;
  }

  hps->sync();
}

#include "hostprofiles.moc"
