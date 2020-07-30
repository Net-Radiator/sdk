/**
 * @file heartbeats.cpp
 * @brief Classes for heartbeating Sync configuration and status
 *
 * (c) 2013 by Mega Limited, Auckland, New Zealand
 *
 * This file is part of the MEGA SDK - Client Access Engine.
 *
 * Applications using the MEGA API must present a valid application key
 * and comply with the the rules set forth in the Terms of Service.
 *
 * The MEGA SDK is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * @copyright Simplified (2-clause) BSD License.
 *
 * You should have received a copy of the license along with this
 * program.
 */

#include "mega/heartbeats.h"
#include "mega/command.h"
#include "assert.h"

namespace mega {

HeartBeatBackupInfo::HeartBeatBackupInfo(handle backupId)
    : mBackupId(backupId)
{
}

handle HeartBeatBackupInfo::backupId() const
{
    return mBackupId;
}

Command *HeartBeatBackupInfo::runningCommand() const
{
    return mRunningCommand;
}

void HeartBeatBackupInfo::setRunningCommand(Command *runningCommand)
{
    mRunningCommand = runningCommand;
}

int HeartBeatBackupInfo::status() const
{
    return mStatus;
}

double HeartBeatBackupInfo::progress() const
{
    return 0.;
}

uint32_t HeartBeatBackupInfo::pendingUps() const
{
    return mPendingUps;
}

uint32_t HeartBeatBackupInfo::pendingDowns() const
{
    return mPendingDowns;
}

void HeartBeatBackupInfo::setPendingDowns(uint32_t pendingDowns)
{
    if (mPendingDowns != pendingDowns)
    {
        mPendingDowns = pendingDowns;
        updateLastActionTime();
    }
}

void HeartBeatBackupInfo::setPendingUps(uint32_t pendingUps)
{
    if (mPendingUps != pendingUps)
    {
        mPendingUps = pendingUps;
        updateLastActionTime();
    }
}

m_time_t HeartBeatBackupInfo::lastAction() const
{
    return mLastAction;
}

mega::MegaHandle HeartBeatBackupInfo::lastItemUpdated() const
{
    return mLastItemUpdated;
}

void HeartBeatBackupInfo::setBackupId(const handle &backupId)
{
    mBackupId = backupId;
}

void HeartBeatBackupInfo::setLastSyncedItem(const mega::MegaHandle &lastSyncedItem)
{
    if (mLastItemUpdated != lastSyncedItem)
    {
        mLastItemUpdated = lastSyncedItem;
        updateLastActionTime();
    }
}

void HeartBeatBackupInfo::setStatus(const int &status)
{
    if (mStatus != status)
    {
        mStatus = status;
        updateLastActionTime();
    }
}

void HeartBeatBackupInfo::setLastAction(const m_time_t &lastAction)
{
    mLastAction = lastAction;
}

void HeartBeatBackupInfo::updateLastActionTime()
{
    setLastAction(m_time(nullptr));
}

void HeartBeatBackupInfo::setLastBeat(const m_time_t &lastBeat)
{
    mLastBeat = lastBeat;
}

m_time_t HeartBeatBackupInfo::lastBeat() const
{
    return mLastBeat;
}

void HeartBeatBackupInfo::onCommandToBeDeleted(Command *command)
{
    if (mRunningCommand == command)
    {
        mRunningCommand = nullptr;
    }
}


////////// HeartBeatTransferProgressedInfo ////////

HeartBeatTransferProgressedInfo::HeartBeatTransferProgressedInfo(handle backupId)
    : HeartBeatBackupInfo(backupId)
{
}

void HeartBeatTransferProgressedInfo::updateTransferInfo(MegaTransfer *transfer)
{
    auto it = mPendingTransfers.find(transfer->getTag());
    if (it == mPendingTransfers.end())
    {
        it = mPendingTransfers.insert(std::make_pair(transfer->getTag(), ::mega::make_unique<PendingTransferInfo>())).first;
    }

    const unique_ptr<PendingTransferInfo> &pending = it->second;

    auto total = mTotalBytes;
    auto transferred = mTransferredBytes;

    // reduce globals by the last known data
    total -= pending->mTotalBytes;
    transferred -= pending->mTransferredBytes;

    // update values with those of the transfer
    pending->mTotalBytes = transfer->getTotalBytes();
    pending->mTransferredBytes = transfer->getTransferredBytes();

    // reflect those in the globals
    total += pending->mTotalBytes;
    transferred += pending->mTransferredBytes;

    setTotalBytes(total);
    setTransferredBytes(transferred);
}

void HeartBeatTransferProgressedInfo::removePendingTransfer(MegaTransfer *transfer)
{
    const auto it = mPendingTransfers.find(transfer->getTag());
    if (it == mPendingTransfers.end())
    {
        assert(false && "removing a non included transfer");
        return;
    }

    unique_ptr<PendingTransferInfo> &pending = it->second;

    // add the transfer data to a list of finished. To reduce the totals with its values
    // when we consider progress is complete
    mFinishedTransfers.push_back(std::move(pending));

    mPendingTransfers.erase(transfer->getTag());

    if (mPendingTransfers.empty())
    {
        clearFinshedTransfers(); // asume the sync is up-to-date: clear totals
        assert(!mPendingUps && !mPendingDowns);
        mTotalBytes = 0;
        mTransferredBytes = 0;
    }
}

void HeartBeatTransferProgressedInfo::clearFinshedTransfers()
{
    for (const auto &pending : mFinishedTransfers)
    {
        // reduce globals by the last known data
        mTotalBytes -= pending->mTotalBytes;
        mTransferredBytes -= pending->mTransferredBytes;
    }

    mFinishedTransfers.clear();
}


void HeartBeatTransferProgressedInfo::setTransferredBytes(long long value)
{
    if (mTransferredBytes != value)
    {
        mTransferredBytes = value;
        updateLastActionTime();
    }
}

void HeartBeatTransferProgressedInfo::setTotalBytes(long long value)
{
    if (mTotalBytes != value)
    {
        mTotalBytes = value;
        updateLastActionTime();
    }
}

double HeartBeatTransferProgressedInfo::progress() const
{
    return std::max(0., std::min(1., static_cast<double>(mTransferredBytes) / static_cast<double>(mTotalBytes)));
}

////////////// HeartBeatSyncInfo ////////////////
HeartBeatSyncInfo::HeartBeatSyncInfo(int tag, handle backupId)
    : HeartBeatTransferProgressedInfo(backupId), mSyncTag(tag)
{
    mStatus = HeartBeatSyncInfo::Status::UNKNOWN;
}

int HeartBeatSyncInfo::syncTag() const
{
    return mSyncTag;
}

void HeartBeatSyncInfo::updateStatus(MegaClient *client)
{
    HeartBeatSyncInfo::Status status = HeartBeatSyncInfo::Status::INACTIVE;

    int tag = syncTag();

    for (sync_list::iterator it = client->syncs.begin(); it != client->syncs.end(); it++)
    {
        Sync *sync = (*it);
        if (sync->tag == tag)
        {
            switch(sync->localroot->ts)
            {
            case TREESTATE_SYNCED:
                status = HeartBeatSyncInfo::Status::UPTODATE;
                break;
            case TREESTATE_PENDING:
                status = HeartBeatSyncInfo::Status::PENDING;
                break;
            case TREESTATE_SYNCING:
                status = HeartBeatSyncInfo::Status::SYNCING;
                break;
            default:
                status = HeartBeatSyncInfo::Status::UNKNOWN;
                break;
            }
        }
    }

    setStatus(status);
}



////////////// BackupInfo ////////////////

MegaBackupInfo::MegaBackupInfo(BackupType type, string localFolder, string name, handle megaHandle, int state, int substate, std::string extra, handle backupId)
    : mType(type), mLocalFolder(localFolder), mName(name), mMegaHandle(megaHandle),
      mState(state), mSubState(substate), mExtra(extra), mBackupId(backupId)
{

}

BackupType MegaBackupInfo::type() const
{
    return mType;
}

handle MegaBackupInfo::backupId() const
{
    return mBackupId;
}

string MegaBackupInfo::localFolder() const
{
    return mLocalFolder;
}

string MegaBackupInfo::name() const
{
    return mName;
}

handle MegaBackupInfo::megaHandle() const
{
    return mMegaHandle;
}

int MegaBackupInfo::state() const
{
    return mState;
}

int MegaBackupInfo::subState() const
{
    return mSubState;
}

string MegaBackupInfo::extra() const
{
    return mExtra;
}

void MegaBackupInfo::setBackupId(const handle &backupId)
{
    mBackupId = backupId;
}

MegaBackupInfoSync::MegaBackupInfoSync(MegaClient *client, const MegaSync &sync, handle backupid)
    : MegaBackupInfo(getSyncType(client, sync), sync.getLocalFolder(), sync.getName(), sync.getMegaHandle()
                 , getSyncState(sync), getSyncSubstatus(sync), getSyncExtraData(sync), backupid)
{


}
int MegaBackupInfoSync::getSyncState(const MegaSync &sync)
{
    if (sync.isTemporaryDisabled())
    {
        return State::TEMPORARY_DISABLED;
    }
    else if (sync.isActive())
    {
        //TODO: consider use case: paused, if transfers are paused!?
        return State::ACTIVE;
    }
    else if (!sync.isEnabled())
    {
        return State::DISABLED;
    }
    else
    {
        return State::FAILED;
    }
}

BackupType MegaBackupInfoSync::getSyncType(MegaClient *client, const MegaSync &sync)
{

    int syncTag = sync.getTag();
    auto config = client->syncConfigs->get(syncTag);
    assert(config);

    if (config)
    {

        switch (config->getType())
        {
        case SyncConfig::Type::TYPE_UP:
                return BackupType::UP_SYNC;
        case SyncConfig::Type::TYPE_DOWN:
                return BackupType::DOWN_SYNC;
        case SyncConfig::Type::TYPE_TWOWAY:
                return BackupType::TWO_WAY;
        default:
                return BackupType::INVALID;
        }
    }
    return BackupType::INVALID;

}

int MegaBackupInfoSync::getSyncSubstatus(const MegaSync &sync)
{
    return sync.getError();
}

string MegaBackupInfoSync::getSyncExtraData(const MegaSync &sync)
{
    return string();
}

////////////// MegaBackupMonitor ////////////////
MegaBackupMonitor::MegaBackupMonitor(MegaClient *client)
    : mClient(client)
{
}

void MegaBackupMonitor::onSyncBackupRegistered(int syncTag, handle backupId)
{
    bool needsAdding = true;

    // remove the tag from the set of in-flight sync registrations
    mPendingSyncPuts.erase(syncTag);

    if (ISUNDEF(backupId))
    {
        LOG_warn << "Received invalid id for sync with tag: " << syncTag;
        needsAdding = false;
    }

    if (!ISUNDEF(backupId) && mHeartBeatedSyncs.find(syncTag) != mHeartBeatedSyncs.end())
    {
        mHeartBeatedSyncs[syncTag]->setBackupId(backupId);
        needsAdding = false;
    }

    if (needsAdding)
    {
        for (const auto &hBPair : mHeartBeatedSyncs)
        {
            if (hBPair.second->backupId() == backupId)
            {
                needsAdding = false;
                break;
            }
        }

        if (needsAdding)
        {
            //create new HeartBeatSyncInfo
            mHeartBeatedSyncs[syncTag] = std::make_shared<HeartBeatSyncInfo>(syncTag, backupId);
        }
    }

    // store the id in the sync configuration
    if (!ISUNDEF(backupId))
    {
        mClient->updateSyncBackupId(syncTag, backupId);
    }

    // handle pending updates
    const auto &pendingSyncPair = mPendingSyncUpdates.find(syncTag);
    if (pendingSyncPair != mPendingSyncUpdates.end())
    {
        if (!ISUNDEF(backupId))
        {
            MegaBackupInfo &info = *pendingSyncPair->second.get();
            info.setBackupId(backupId);
            updateBackupInfo(info);
        }
        else
        {
            LOG_warn << "discarding heartbeat update for pending sync: no valid id received for sync: " << pendingSyncPair->second->localFolder();

        }
        mPendingSyncUpdates.erase(pendingSyncPair);
    }
}

void MegaBackupMonitor::digestPutResult(handle backupId)
{
    // get the tag from queue of pending puts
    assert(mPendingBackupPutCallbacks.size());
    auto putResultCallback = mPendingBackupPutCallbacks.front();
    mPendingBackupPutCallbacks.pop_front();


    //call corresponding callback
    putResultCallback(backupId);
}

void MegaBackupMonitor::updateBackupInfo(const MegaBackupInfo &info)
{
    string localFolderEncrypted(mClient->cypherTLVTextWithMasterKey("lf", info.localFolder()) );
    string deviceIDEncrypted(mClient->cypherTLVTextWithMasterKey("de", mClient->getDeviceid()) );
    string nameEncrypted(mClient->cypherTLVTextWithMasterKey("na", info.name()) );

    mClient->reqs.add(new CommandBackupPut(mClient, info.backupId(), info.type(), info.megaHandle(),
                                           localFolderEncrypted.c_str(),
                                           deviceIDEncrypted.c_str(), nameEncrypted.c_str(),
                                           info.state(), info.subState(), info.extra().c_str()
                                           ));
}


void MegaBackupMonitor::registerBackupInfo(const MegaBackupInfo &info)
{
    string localFolderEncrypted(mClient->cypherTLVTextWithMasterKey("lf", info.localFolder()) );
    string deviceIDEncrypted(mClient->cypherTLVTextWithMasterKey("de", mClient->getDeviceid()) );
    string nameEncrypted(mClient->cypherTLVTextWithMasterKey("na", info.name()) );

    mClient->reqs.add(new CommandBackupPut(mClient, info.type(), info.megaHandle(),
                                           localFolderEncrypted.c_str(),
                                           deviceIDEncrypted.c_str(), nameEncrypted.c_str(),
                                           info.state(), info.subState(), info.extra().c_str()
                                           ));
}



void MegaBackupMonitor::updateOrRegisterSync(MegaSync *sync)
{
    if (!sync)
    {
        return;
    }
    int syncTag = sync->getTag();
    auto config = mClient->syncConfigs->get(syncTag);
    handle backupId = config ? config->getBackupId() : UNDEF;

    std::unique_ptr<MegaBackupInfo> info = ::mega::make_unique<MegaBackupInfoSync>(mClient, *sync, backupId);

    if (info->backupId() == UNDEF) // not registered (or pending registration)
    {
        if (mPendingSyncPuts.find(syncTag) == mPendingSyncPuts.end()) //new backup, register required
        {
            registerBackupInfo(*info.get());

            // queue callback to process the backupId when received
            mPendingSyncPuts.insert(syncTag);
            mPendingBackupPutCallbacks.push_back([this, syncTag](handle backupId)
            {
                onSyncBackupRegistered(syncTag, backupId);
            });
        }
        else // registration in-flight: backupId not received yet, let's queue the update
        {
            LOG_debug << " Queuing sync update, register is on progress for sync: " << info->localFolder();
            mPendingSyncUpdates[syncTag].reset(info.release()); // we replace any previous pending updates
        }
    }
    else //update
    {
        updateBackupInfo(*info); //queue update comand

        auto hBPair = mHeartBeatedSyncs.find(syncTag);
        if (hBPair == mHeartBeatedSyncs.end()) //not in our map: backupId read from cache
        {
            //create new HeartBeatSyncInfo
            mHeartBeatedSyncs.insert(std::make_pair(syncTag, std::make_shared<HeartBeatSyncInfo>(syncTag, backupId)));
        }
    }
}

void MegaBackupMonitor::onSyncAdded(MegaApi* /*api*/, MegaSync *sync, int /*additionState*/)
{
    updateOrRegisterSync(sync);
}

void MegaBackupMonitor::onSyncStateChanged(MegaApi* /*api*/, MegaSync *sync)
{
    updateOrRegisterSync(sync);
}

std::shared_ptr<HeartBeatSyncInfo> MegaBackupMonitor::getHeartBeatBackupInfoByTransfer(MegaTransfer *transfer)
{
    if (!transfer->isSyncTransfer())
    {
        return nullptr;
    }

    int syncTag = 0;

    // use map to get the syncTag directly if there was one
    auto mTSPair = mTransferToSyncMap.find(transfer->getTag());
    if (mTSPair != mTransferToSyncMap.end())
    {
        syncTag = mTSPair->second;
    }

    if (!syncTag) //first time
    {
        Node *n = mClient->nodebyhandle(transfer->getType() == MegaTransfer::TYPE_UPLOAD ? transfer->getParentHandle() : transfer->getNodeHandle());
        while (n)
        {
            if (n && n->localnode && n->localnode->sync)
            {
                syncTag = n->localnode->sync->tag;
                mTransferToSyncMap[transfer->getTag()] = syncTag;
                break;
            }
            LOG_warn << "Heartbeat could not get sync tag direclty from transfer handle. Going up";
            n = n->parent;
        }
    }

    if (syncTag)
    {
        auto hBPair = mHeartBeatedSyncs.find(syncTag);
        if (hBPair != mHeartBeatedSyncs.end()) //transfer info arrived before having created the HeartBeatSyncInfo object
        {
            return hBPair->second;
        }
        else
        {
            //create new HeartBeatSyncInfo
            return mHeartBeatedSyncs.insert(std::make_pair(syncTag, std::make_shared<HeartBeatSyncInfo>(syncTag, UNDEF))).first->second;
        }
    }

    return nullptr;
}

void MegaBackupMonitor::onTransferStart(MegaApi *api, MegaTransfer *transfer)
{
    auto hbs = getHeartBeatBackupInfoByTransfer(transfer);
    if (hbs)
    {
        if (transfer->getType() == MegaTransfer::TYPE_UPLOAD)
        {
            hbs->setPendingUps(hbs->pendingUps() + 1);
        }
        else
        {
            hbs->setPendingDowns(hbs->pendingDowns() + 1);
        }
        hbs->updateTransferInfo(transfer);
    }
}

void MegaBackupMonitor::onTransferUpdate(MegaApi *api, MegaTransfer *transfer)
{
    auto hbs = getHeartBeatBackupInfoByTransfer(transfer);
    if (hbs)
    {
        hbs->updateTransferInfo(transfer);
    }
}

void MegaBackupMonitor::onTransferFinish(MegaApi *api, MegaTransfer *transfer, MegaError *error)
{
    auto hbs = getHeartBeatBackupInfoByTransfer(transfer);
    if (hbs)
    {
        if (transfer->getType() == MegaTransfer::TYPE_UPLOAD)
        {
            hbs->setPendingUps(hbs->pendingUps() - 1);
        }
        else
        {
            hbs->setPendingDowns(hbs->pendingDowns() - 1);
        }
        hbs->updateTransferInfo(transfer);

        hbs->removePendingTransfer(transfer);
        mTransferToSyncMap.erase(transfer->getTag());

        if (error->getErrorCode() == API_OK)
        {
            hbs->setLastSyncedItem(transfer->getNodeHandle());
        }
    }
}

void MegaBackupMonitor::onSyncDeleted(MegaApi *api, MegaSync *sync)
{
    auto hBPair = mHeartBeatedSyncs.find(sync->getTag());
    if (hBPair != mHeartBeatedSyncs.end())
    {
        mClient->reqs.add(new CommandBackupRemove(mClient, hBPair->second->backupId()));

        mHeartBeatedSyncs.erase(hBPair); //This is speculative: could be moved to backupremove_result
        // in case we wanted to handle possible failing cases.
        mPendingSyncUpdates.erase(sync->getTag()); // remove any pending update: also speculative.
    }
}

void MegaBackupMonitor::calculateStatus(HeartBeatBackupInfo *hbs)
{
   hbs->updateStatus(mClient);
}

void MegaBackupMonitor::beatBackupInfo(const std::shared_ptr<HeartBeatBackupInfo> &hbs)
{
    auto now = m_time(nullptr);
    auto lapsed = now - hbs->lastBeat();
    if ( (hbs->lastAction() > hbs->lastBeat()) //something happened since last reported!
         || lapsed > MAX_HEARBEAT_SECS_DELAY) // max delay happened. Beating: Sicherheitsfahrschaltung!
    {
        calculateStatus(hbs.get()); //we asume this is costly: only do it when beating

        hbs->setLastBeat(m_time(nullptr));

        auto newCommand = new CommandBackupPutHeartBeat(mClient, hbs->backupId(), hbs->status(),
                          static_cast<uint8_t>(std::lround(hbs->progress()*100.0)), hbs->pendingUps(), hbs->pendingDowns(),
                          hbs->lastAction(), hbs->lastItemUpdated());

        auto runningCommand = hbs->runningCommand();

        if (runningCommand && !runningCommand->getRead()) //replace existing command
        {
            LOG_warn << "Detected a yet unprocessed beat: replacing data with current";
            // instead of appending a new command, and potentially hammering, we just update the existing command with the updated input
            runningCommand->replaceWith(*newCommand);
        }
        else // append new command
        {
            hbs->setRunningCommand(newCommand);
            newCommand->addListener(hbs);
            mClient->reqs.add(newCommand);
        }
    }
}

void MegaBackupMonitor::beat()
{
    for (const auto &hBPair : mHeartBeatedSyncs)
    {
        beatBackupInfo(hBPair.second);
    }
}

}