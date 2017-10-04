
#include "sourcegroup.h"

#include <algorithm>

#include "source.h"
#include "context.h"

namespace alure
{

void ALSourceGroup::eraseSubGroup(ALSourceGroup *group)
{
    auto iter = std::lower_bound(mSubGroups.begin(), mSubGroups.end(), group);
    if(iter != mSubGroups.end() && *iter == group) mSubGroups.erase(iter);
}


void ALSourceGroup::setParentGroup(ALSourceGroup *group)
{
    if(mParent)
        mParent->eraseSubGroup(this);
    mParent = group;
    SourceGroupProps props;
    mParent->applyPropTree(props);
    update(props.mGain, props.mPitch);
}

void ALSourceGroup::unsetParentGroup()
{
    mParent = nullptr;
    update(1.0f, 1.0f);
}

void ALSourceGroup::update(ALfloat gain, ALfloat pitch)
{
    mParentProps.mGain = gain;
    mParentProps.mPitch = pitch;

    gain *= mGain;
    pitch *= mPitch;
    for(ALSource *alsrc : mSources)
        alsrc->groupPropUpdate(gain, pitch);
    for(ALSourceGroup *group : mSubGroups)
        group->update(gain, pitch);
}


bool ALSourceGroup::findInSubGroups(ALSourceGroup *group) const
{
    auto iter = std::lower_bound(mSubGroups.begin(), mSubGroups.end(), group);
    if(iter != mSubGroups.end() && *iter == group) return true;

    for(ALSourceGroup *group : mSubGroups)
    {
        if(group->findInSubGroups(group))
            return true;
    }
    return false;
}


void ALSourceGroup::addSource(Source *source)
{
    ALSource *alsrc = cast<ALSource*>(source);
    if(!alsrc) throw std::runtime_error("Source is not valid");
    CheckContext(mContext);

    auto iter = std::lower_bound(mSources.begin(), mSources.end(), alsrc);
    if(iter != mSources.end() && *iter == alsrc) return;

    mSources.insert(iter, alsrc);
    alsrc->setGroup(this);
}

void ALSourceGroup::removeSource(Source *source)
{
    CheckContext(mContext);
    auto iter = std::lower_bound(mSources.begin(), mSources.end(), source);
    if(iter != mSources.end() && *iter == source)
    {
        (*iter)->unsetGroup();
        mSources.erase(iter);
    }
}


void ALSourceGroup::addSources(const Vector<Source*> &sources)
{
    CheckContext(mContext);
    if(sources.empty())
        return;

    Vector<ALSource*> alsrcs;
    alsrcs.reserve(sources.size());

    for(Source *source : sources)
    {
        alsrcs.push_back(cast<ALSource*>(source));
        if(!alsrcs.back()) throw std::runtime_error("Source is not valid");
    }

    Batcher batcher = mContext->getBatcher();
    for(ALSource *alsrc : alsrcs)
    {
        auto iter = std::lower_bound(mSources.begin(), mSources.end(), alsrc);
        if(iter != mSources.end() && *iter == alsrc) continue;

        mSources.insert(iter, alsrc);
        alsrc->setGroup(this);
    }
}

void ALSourceGroup::removeSources(const Vector<Source*> &sources)
{
    Batcher batcher = mContext->getBatcher();
    for(Source *source : sources)
    {
        auto iter = std::lower_bound(mSources.begin(), mSources.end(), source);
        if(iter != mSources.end() && *iter == source)
        {
            (*iter)->unsetGroup();
            mSources.erase(iter);
        }
    }
}


void ALSourceGroup::addSubGroup(SourceGroup group)
{
    ALSourceGroup *algrp = group.pImpl;
    if(!algrp) throw std::runtime_error("SourceGroup is not valid");
    CheckContext(mContext);

    auto iter = std::lower_bound(mSubGroups.begin(), mSubGroups.end(), algrp);
    if(iter != mSubGroups.end() && *iter == algrp) return;

    if(this == algrp || algrp->findInSubGroups(this))
        throw std::runtime_error("Attempted circular group chain");

    mSubGroups.insert(iter, algrp);
    Batcher batcher = mContext->getBatcher();
    algrp->setParentGroup(this);
}

void ALSourceGroup::removeSubGroup(SourceGroup group)
{
    auto iter = std::lower_bound(mSubGroups.begin(), mSubGroups.end(), group.pImpl);
    if(iter != mSubGroups.end() && *iter == group.pImpl)
    {
        Batcher batcher = mContext->getBatcher();
        (*iter)->unsetParentGroup();
        mSubGroups.erase(iter);
    }
}


Vector<Source*> ALSourceGroup::getSources() const
{
    Vector<Source*> ret;
    ret.reserve(mSources.size());
    std::copy(mSources.begin(), mSources.end(), std::back_inserter(ret));
    return ret;
}

alure::Vector<SourceGroup> ALSourceGroup::getSubGroups() const
{
    Vector<SourceGroup> ret;
    ret.reserve(mSubGroups.size());
    for(ALSourceGroup *grp : mSubGroups)
        ret.emplace_back(SourceGroup(grp));
    return ret;
}


void ALSourceGroup::setGain(ALfloat gain)
{
    if(!(gain >= 0.0f))
        throw std::runtime_error("Gain out of range");
    CheckContext(mContext);
    mGain = gain;
    gain *= mParentProps.mGain;
    ALfloat pitch = mPitch * mParentProps.mPitch;
    Batcher batcher = mContext->getBatcher();
    for(ALSource *alsrc : mSources)
        alsrc->groupPropUpdate(gain, pitch);
    for(ALSourceGroup *group : mSubGroups)
        group->update(gain, pitch);
}

void ALSourceGroup::setPitch(ALfloat pitch)
{
    if(!(pitch > 0.0f))
        throw std::runtime_error("Pitch out of range");
    CheckContext(mContext);
    mPitch = pitch;
    ALfloat gain = mGain * mParentProps.mGain;
    pitch *= mParentProps.mPitch;
    Batcher batcher = mContext->getBatcher();
    for(ALSource *alsrc : mSources)
        alsrc->groupPropUpdate(gain, pitch);
    for(ALSourceGroup *group : mSubGroups)
        group->update(gain, pitch);
}


void ALSourceGroup::collectPlayingSourceIds(Vector<ALuint> &sourceids) const
{
    for(ALSource *alsrc : mSources)
    {
        if(alsrc->isPlaying())
            sourceids.push_back(alsrc->getId());
    }
    for(ALSourceGroup *group : mSubGroups)
        group->collectPlayingSourceIds(sourceids);
}

void ALSourceGroup::updatePausedStatus() const
{
    for(ALSource *alsrc : mSources)
        alsrc->checkPaused();
    for(ALSourceGroup *group : mSubGroups)
        group->updatePausedStatus();
}

void ALSourceGroup::pauseAll() const
{
    CheckContext(mContext);
    auto lock = mContext->getSourceStreamLock();

    Vector<ALuint> sourceids;
    sourceids.reserve(16);
    collectPlayingSourceIds(sourceids);
    if(!sourceids.empty())
    {
        alSourcePausev(sourceids.size(), sourceids.data());
        updatePausedStatus();
    }
    lock.unlock();
}


void ALSourceGroup::collectPausedSourceIds(Vector<ALuint> &sourceids) const
{
    for(ALSource *alsrc : mSources)
    {
        if(alsrc->isPaused())
            sourceids.push_back(alsrc->getId());
    }
    for(ALSourceGroup *group : mSubGroups)
        group->collectPausedSourceIds(sourceids);
}

void ALSourceGroup::updatePlayingStatus() const
{
    for(ALSource *alsrc : mSources)
        alsrc->unsetPaused();
    for(ALSourceGroup *group : mSubGroups)
        group->updatePlayingStatus();
}

void ALSourceGroup::resumeAll() const
{
    CheckContext(mContext);
    auto lock = mContext->getSourceStreamLock();

    Vector<ALuint> sourceids;
    sourceids.reserve(16);
    collectPausedSourceIds(sourceids);
    if(!sourceids.empty())
    {
        alSourcePlayv(sourceids.size(), sourceids.data());
        updatePlayingStatus();
    }
    lock.unlock();
}


void ALSourceGroup::collectSourceIds(Vector<ALuint> &sourceids) const
{
    for(ALSource *alsrc : mSources)
    {
        if(ALuint id = alsrc->getId())
            sourceids.push_back(id);
    }
    for(ALSourceGroup *group : mSubGroups)
        group->collectSourceIds(sourceids);
}

void ALSourceGroup::updateStoppedStatus() const
{
    for(ALSource *alsrc : mSources)
    {
        alsrc->makeStopped();
        mContext->send(&MessageHandler::sourceForceStopped, alsrc);
    }
    for(ALSourceGroup *group : mSubGroups)
        group->updateStoppedStatus();
}

void ALSourceGroup::stopAll() const
{
    CheckContext(mContext);
    auto lock = mContext->getSourceStreamLock();

    Vector<ALuint> sourceids;
    sourceids.reserve(16);
    collectSourceIds(sourceids);
    if(!sourceids.empty())
    {
        alSourceRewindv(sourceids.size(), sourceids.data());
        updateStoppedStatus();
    }
    lock.unlock();
}


void ALSourceGroup::release()
{
    CheckContext(mContext);
    Batcher batcher = mContext->getBatcher();
    for(ALSource *source : mSources)
        source->unsetGroup();
    mSources.clear();
    for(ALSourceGroup *group : mSubGroups)
        group->unsetParentGroup();
    mSubGroups.clear();
    if(mParent)
        mParent->eraseSubGroup(this);
    mParent = nullptr;

    mContext->freeSourceGroup(this);
}


DECL_THUNK0(const String&, SourceGroup, getName, const)
DECL_THUNK1(void, SourceGroup, addSource,, Source*)
DECL_THUNK1(void, SourceGroup, removeSource,, Source*)
DECL_THUNK1(void, SourceGroup, addSources,, const Vector<Source*>&)
DECL_THUNK1(void, SourceGroup, removeSources,, const Vector<Source*>&)
DECL_THUNK1(void, SourceGroup, addSubGroup,, SourceGroup)
DECL_THUNK1(void, SourceGroup, removeSubGroup,, SourceGroup)
DECL_THUNK0(Vector<Source*>, SourceGroup, getSources, const)
DECL_THUNK0(Vector<SourceGroup>, SourceGroup, getSubGroups, const)
DECL_THUNK1(void, SourceGroup, setGain,, ALfloat)
DECL_THUNK0(ALfloat, SourceGroup, getGain, const)
DECL_THUNK1(void, SourceGroup, setPitch,, ALfloat)
DECL_THUNK0(ALfloat, SourceGroup, getPitch, const)
DECL_THUNK0(void, SourceGroup, pauseAll, const)
DECL_THUNK0(void, SourceGroup, resumeAll, const)
DECL_THUNK0(void, SourceGroup, stopAll, const)
void SourceGroup::release()
{
    pImpl->release();
    pImpl = nullptr;
}

} // namespace alure
