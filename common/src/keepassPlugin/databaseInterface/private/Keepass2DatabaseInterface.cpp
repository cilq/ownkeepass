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
#include "Keepass2DatabaseInterface.h"
#include "../KdbListModel.h"
#include "../KdbGroup.h"
#include "crypto/Crypto.h"
#include "format/KeePass2Reader.h"
#include "keys/PasswordKey.h"
#include "keys/FileKey.h"
#include "core/Group.h"


using namespace kpxPrivate;
using namespace kpxPublic;


Keepass2DatabaseInterface::Keepass2DatabaseInterface(QObject *parent)
    : QObject(parent),
      m_Database(NULL),
      m_setting_showUserNamePasswordsInListView(false),
      m_setting_sortAlphabeticallyInListView(true),
      m_rootGroupId(0)
{
    initDatabase();
}

Keepass2DatabaseInterface::~Keepass2DatabaseInterface()
{
    qDebug("Destructor Keepass2DatabaseInterface");
    delete m_Database;
}

void Keepass2DatabaseInterface::initDatabase()
{
    // init crypto algorithms
    if (!Crypto::init()) {
        // Fatal error while testing the cryptographic functions
// TODO add error handling
    }

}

void Keepass2DatabaseInterface::slot_openDatabase(QString filePath, QString password, QString keyfile, bool readonly)
{
    // check if filePath is readable or read-writable
    QFile file(filePath);
    if (readonly) {
        if (!file.open(QIODevice::ReadOnly)) {
            qDebug() << "ERROR: Keepass 2 database is not readable!";
// TODO: return signal with error type
            return;
        }
    } else {
        if (!file.open(QIODevice::ReadWrite)) {
            qDebug() << "ERROR: Keepass 2 databasse is not read-writeable!";
// TODO: return signal with error type
            return;
        }
    }

    CompositeKey masterKey;
    masterKey.addKey(PasswordKey(password));
    if (!keyfile.isEmpty()) {
        FileKey key;
        QString errorMsg;
        if (!key.load(keyfile, &errorMsg)) {
            qDebug() << "ERROR: Cannot open key file for Keepass 2 database. " << errorMsg;
//            MessageBox::warning(this, tr("Error"), tr("Can't open key file").append(":\n").append(errorMsg));
// TODO: return signal with error type
            return;
        }
        masterKey.addKey(key);
    }

    if (m_Database) {
        delete m_Database;
    }

    KeePass2Reader reader;
    m_Database = reader.readDatabase(&file, masterKey);

// TODO check if .lock file exists and ask user if he wants to open the database in read only mode or discard and open in read/write mode
// TODO create .lock file if it does not exist yet

    // database was opened successfully
    emit databaseOpened();

    // load used encryption and KeyTransfRounds and sent to KdbDatabase object so that it is shown in UI database settings page
    emit databaseCryptAlgorithmChanged(0); // Keepass2 only supports Rijndael_Cipher = 0
    emit databaseKeyTransfRoundsChanged(m_Database->transformRounds());

    qDebug() << "Keepass 2 database successfully opened!";
}

void Keepass2DatabaseInterface::slot_closeDatabase()
{
}

void Keepass2DatabaseInterface::slot_createNewDatabase(QString filePath, QString password, QString keyfile, int cryptAlgorithm, int keyTransfRounds)
{
}

void Keepass2DatabaseInterface::slot_changePassKey(QString password, QString keyFile)
{
}

void Keepass2DatabaseInterface::slot_loadMasterGroups(bool registerListModel)
{
    Q_ASSERT(m_Database);

    Uuid listModelId = Uuid(); // root group has list model ID 0

    QList<Group*> masterGroups = m_Database->rootGroup()->children();
    for (int i = 0; i < masterGroups.count(); i++) {
        Group* masterGroup = masterGroups.at(i);
        qDebug() << "Mastergroup " << i << ": " << masterGroup->name();
        qDebug() << "Expanded: " << masterGroup->isExpanded();

        int numberOfSubgroups = masterGroup->children().count();
        int numberOfEntries = masterGroup->entries().count();

        Uuid masterGroupId = masterGroup->uuid();
        qDebug() << "Uuid: " << masterGroupId.toByteArray();
        qDebug() << "toHex: " << masterGroupId.toHex();
        if (registerListModel) {
            // save modelId and master group only if needed
            // i.e. save model list id for master group page and don't do it for list models used in dialogs
            m_groups_modelId.insertMulti((const Uuid &)listModelId, (const Uuid &)masterGroupId);
        }
        emit appendItemToListModel(masterGroup->name(),                            // group name
                                   QString("Subgroups: %1 | Entries: %2")
                                   .arg(numberOfSubgroups)
                                   .arg(numberOfEntries),                          // subtitle
                                   masterGroupId.toHex(),                          // item id
                                   (int)GROUP,                                          // item type
                                   0,                                              // item level (0 = root, 1 = first level, etc.
                                   listModelId.toHex());                           // list model of root group
    }

    QList<Entry*> masterEntries = m_Database->rootGroup()->entries();
    for (int i = 0; i < masterEntries.count(); i++) {
        Entry* entry = masterEntries.at(i);
        Uuid itemId = entry->uuid();
        // only append to list model if item ID is valid
        emit appendItemToListModel(entry->title(),                                 // group name
                                   getUserAndPassword(entry),                      // subtitle
                                   itemId.toHex(),                                 // item id
                                   (int)ENTRY,                                          // item type
                                   0,                                              // item level (not used here)
                                   listModelId.toHex());                           // list model gets groupId as its unique ID (here 0 because of root group)
        // save modelId and entry
        m_entries_modelId.insertMulti(listModelId, itemId);
    }
    emit masterGroupsLoaded(RE_OK);
}

void Keepass2DatabaseInterface::slot_loadGroupsAndEntries(QString groupId)
{
    Q_ASSERT(m_Database);
    // load sub groups and entries
    Uuid groupUuid = qString2Uuid(groupId);
    Group* group = m_Database->resolveGroup(groupUuid);
    QList<Group*> subGroups = group->children();
/*
    if (m_setting_sortAlphabeticallyInListView) {
        subGroups = m_kdb3Database->sortedGroups();
    } else {
        subGroups = m_kdb3Database->groups();
    }
*/

    for (int i = 0; i < subGroups.count(); i++) {
        Group* subGroup = subGroups.at(i);
        int numberOfSubgroups = subGroup->children().count();
        int numberOfEntries = subGroup->entries().count();
        Uuid itemId = subGroup->uuid();
        emit appendItemToListModel(subGroup->name(),                               // group name
                                   QString("Subgroups: %1 | Entries: %2")
                                   .arg(numberOfSubgroups).arg(numberOfEntries),   // subtitle
                                   itemId.toHex(),                                 // item id
                                   (int)GROUP,                                     // item type
                                   0,                                              // item level (not used here)
                                   groupId);                                       // list model gets groupId as its unique ID
        // save modelId and group
        m_groups_modelId.insertMulti(groupUuid, itemId);
    }

    QList<Entry*> entries = group->entries();
/*
    if (m_setting_sortAlphabeticallyInListView) {
        entries = m_kdb3Database->entriesSortedStd(group);
    } else {
        entries = m_kdb3Database->entries(group);
    }
*/
    for (int i = 0; i < entries.count(); i++) {
        Entry* entry = entries.at(i);
        Uuid itemId = entry->uuid();
        emit appendItemToListModel(entry->title(),                                 // group name
                                   getUserAndPassword(entry),                      // subtitle
                                   itemId.toHex(),                                 // item id
                                   (int)ENTRY,                                     // item type
                                   0,                                              // item level (not used here)
                                   groupId);                                       // list model gets groupId as its unique ID
        // save modelId and entry
        m_entries_modelId.insertMulti(groupUuid, itemId);
    }
    emit groupsAndEntriesLoaded(RE_OK);
}

void Keepass2DatabaseInterface::slot_loadEntry(QString entryId)
{
}

void Keepass2DatabaseInterface::slot_loadGroup(QString groupId)
{
}

void Keepass2DatabaseInterface::slot_saveGroup(QString groupId, QString title)
{
}

void Keepass2DatabaseInterface::slot_unregisterListModel(QString modelId)
{
    // delete all groups and entries which are associated with given modelId
    m_groups_modelId.remove(qString2Uuid(modelId));
    m_entries_modelId.remove(qString2Uuid(modelId));
}

void Keepass2DatabaseInterface::slot_createNewGroup(QString title, quint32 iconId, QString parentGroupId)
{
}

void Keepass2DatabaseInterface::slot_saveEntry(QString entryId,
                                        QString title,
                                        QString url,
                                        QString username,
                                        QString password,
                                        QString comment)
{
    Q_ASSERT(m_Database);
}

void Keepass2DatabaseInterface::slot_createNewEntry(QString title,
                                             QString url,
                                             QString username,
                                             QString password,
                                             QString comment,
                                             QString parentGroupId)
{
}

void Keepass2DatabaseInterface::slot_deleteGroup(QString groupId)
{
}

//void Keepass2DatabaseInterface::updateGrandParentGroupInListModel(IGroupHandle* parentGroup)
//{
//}

void Keepass2DatabaseInterface::slot_deleteEntry(QString entryId)
{
}

void Keepass2DatabaseInterface::slot_moveEntry(QString entryId, QString newGroupId)
{
}

void Keepass2DatabaseInterface::slot_moveGroup(QString groupId, QString newParentGroupId)
{
}

void Keepass2DatabaseInterface::slot_searchEntries(QString searchString, QString rootGroupId)
{
}

inline QString Keepass2DatabaseInterface::getUserAndPassword(Entry* entry)
{
    if (m_setting_showUserNamePasswordsInListView) {
        QString username = entry->username();
        QString password = entry->password();
        if (username.length() == 0 && password.length() == 0) {
            return QString("");
        } else {
            return QString("%1 | %2").arg(username).arg(password);
        }
    } else {
        return QString("");
    }
}

/******************************************************************************
\brief Convert QString to Uuid

This function converts a 16 character long QString into a Uuid.

\param QString value to be converted to Uuid
\emit RE_ERR_QSTRING_TO_UUID if the conversion was not successful because the
      QString value was not exactly 16 characters long
\return Uuid representation of the QString content or
        an empty Uuid if an error occured during conversion
+******************************************************************************/
inline Uuid Keepass2DatabaseInterface::qString2Uuid(QString value)
{
    QByteArray baValue = QByteArray::fromHex(value.toLatin1());
    if (baValue.size() == Uuid::Length) {
        return Uuid(baValue);
    } else {
        emit errorOccured(RE_ERR_QString_TO_UUID, value);
        return Uuid();
    }
}

void Keepass2DatabaseInterface::slot_changeKeyTransfRounds(int value)
{
}

void Keepass2DatabaseInterface::slot_changeCryptAlgorithm(int value)
{
}
