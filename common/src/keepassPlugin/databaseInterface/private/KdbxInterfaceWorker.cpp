/***************************************************************************
**
** Copyright (C) 2015 Marko Koschak (marko.koschak@tisno.de)
** All rights reserved.
**
** This file is part of ownKeepass.
**
** ownKeepass is free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation, either version 2 of the License, or
** (at your option) any later version.
**
** ownKeepass is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with ownKeepass.  If not, see <http://www.gnu.org/licenses/>.
**
***************************************************************************/

#include <QDebug>

#include "KdbxInterfaceWorker.h"

using namespace keepassx2Private;
using namespace kpxPublic;

KdbxInterfaceWorker::KdbxInterfaceWorker(QObject *parent)
    : QObject(parent),
      m_kdbxDatabase(NULL),
      m_setting_showUserNamePasswordsInListView(false)
{
    initKdbDatabase();
}

KdbxInterfaceWorker::~KdbxInterfaceWorker()
{
    qDebug("Destructor KdbInterfaceWorker");
    delete m_kdbxDatabase;
}

void KdbxInterfaceWorker::initKdbDatabase()
{
}
