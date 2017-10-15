
#include "sourcegroup.h"

#include <algorithm>

#include "source.h"
#include "context.h"

namespace alure
{

void SourceGroupImpl::eraseSubGroup(SourceGroupImpl *group)
{
    auto iter = std::lower_bound(mSubGroups.begin(), mSubGroups.end(), group);
    if(iter != mSubGroups.end() && *iter == group) mSubGroups.erase(iter);
}


void SourceGroupImpl::setParentGroup(SourceGroupImpl *group)
{
    if(mParent)
        mParent->eraseSubGroup(this);
    mParent = group;
    SourceGroupProps props;
    mParent->applyPropTree(props);
    update(props.mGain, props.mPitch);
}

void SourceGroupImpl::unsetParentGroup()
{
    mParent = nullptr;
    update(1.0f, 1.0f);
}

void SourceGroupImpl::update(ALfloat gain, ALfloat pitch)
{
    mParentProps.mGain = gain;
    mParentProps.mPitch = pitch;

    gain *= mGain;
    pitch *= mPitch;
    for(SourceImpl *alsrc : mSources)
        alsrc->groupPropUpdate(gain, pitch);
    for(SourceGroupImpl *group : mSubGroups)
        group->update(gain, pitch);
}


bool SourceGroupImpl::findInSubGroups(SourceGroupImpl *group) const
{
    auto iter = std::lower_bound(mSubGroups.begin(), mSubGroups.end(), group);
    if(iter != mSubGroups.end() && *iter == group) return true;

    for(SourceGroupImpl *group : mSubGroups)
    {
        if(group->findInSubGroups(group))
            return true;
    }
    return false;
}


void SourceGroupImpl::addSource(Source source)
{
    SourceImpl *alsrc = source.getHandle();
    if(!alsrc) throw std::runtime_error("Source is not valid");
    CheckContext(mContext);

    auto iter = std::lower_bound(mSources.begin(), mSources.end(), alsrc);
    if(iter != mSources.end() && *iter == alsrc) return;

    mSources.insert(iter, alsrc);
    alsrc->setGroup(this);
}

void SourceGroupImpl::removeSource(Source source)
{
    CheckContext(mContext);
    auto iter = std::lower_bound(mSources.begin(), mSources.end(), source.getHandle());
    if(iter != mSources.end() && *iter == source.getHandle())
    {
        (*iter)->unsetGroup();
        mSources.erase(iter);
    }
}


void SourceGroupImpl::addSources(ArrayView<Source> sources)
{
    CheckContext(mContext);
    if(sources.empty())
        return;

    Vector<SourceImpl*> alsrcs;
    alsrcs.reserve(sources.size());

    for(Source source : sources)
    {
        alsrcs.push_back(source.getHandle());
        if(!alsrcs.back()) throw std::runtime_error("Source is not valid");
    }

    Batcher batcher = mContext->getBatcher();
    for(SourceImpl *alsrc : alsrcs)
    {
        auto iter = std::lower_bound(mSources.begin(), mSources.end(), alsrc);
        if(iter != mSources.end() && *iter == alsrc) continue;

        mSources.insert(iter, alsrc);
        alsrc->setGroup(this);
    }
}

void SourceGroupImpl::removeSources(ArrayView<Source> sources)
{
    Batcher batcher = mContext->getBatcher();
    for(Source source : sources)
    {
        auto iter = std::lower_bound(mSources.begin(), mSources.end(), source.getHandle());
        if(iter != mSources.end() && *iter == source.getHandle())
        {
            (*iter)->unsetGroup();
            mSources.erase(iter);
        }
    }
}


void SourceGroupImpl::addSubGroup(SourceGroup group)
{
    SourceGroupImpl *algrp = group.getHandle();
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

void SourceGroupImpl::removeSubGroup(SourceGroup group)
{
    auto iter = std::lower_bound(mSubGroups.begin(), mSubGroups.end(), group.getHandle());
    if(iter != mSubGroups.end() && *iter == group.getHandle())
    {
        Batcher batcher = mContext->getBatcher();
        (*iter)->unsetParentGroup();
        mSubGroups.erase(iter);
    }
}


Vector<Source> SourceGroupImpl::getSources() const
{
    Vector<Source> ret;
    ret.reserve(mSources.size());
    for(SourceImpl *src : mSources)
        ret.emplace_back(Source(src));
    return ret;
}

Vector<SourceGroup> SourceGroupImpl::getSubGroups() const
{
    Vector<SourceGroup> ret;
    ret.reserve(mSubGroups.size());
    for(SourceGroupImpl *grp : mSubGroups)
        ret.emplace_back(SourceGroup(grp));
    return ret;
}


void SourceGroupImpl::setGain(ALfloat gain)
{
    if(!(gain >= 0.0f))
        throw std::runtime_error("Gain out of range");
    CheckContext(mContext);
    mGain = gain;
    gain *= mParentProps.mGain;
    ALfloat pitch = mPitch * mParentProps.mPitch;
    Batcher batcher = mContext->getBatcher();
    for(SourceImpl *alsrc : mSources)
        alsrc->groupPropUpdate(gain, pitch);
    for(SourceGroupImpl *group : mSubGroups)
        group->update(gain, pitch);
}

void SourceGroupImpl::setPitch(ALfloat pitch)
{
    if(!(pitch > 0.0f))
        throw std::runtime_error("Pitch out of range");
    CheckContext(mContext);
    mPitch = pitch;
    ALfloat gain = mGain * mParentProps.mGain;
    pitch *= mParentProps.mPitch;
    Batcher batcher = mContext->getBatcher();
    for(SourceImpl *alsrc : mSources)
        alsrc->groupPropUpdate(gain, pitch);
    for(SourceGroupImpl *group : mSubGroups)
        group->update(gain, pitch);
}


void SourceGroupImpl::collectPlayingSourceIds(Vector<ALuint> &sourceids) const
{
    for(SourceImpl *alsrc : mSources)
    {
        if(alsrc->isPlaying())
            sourceids.push_back(alsrc->getId());
    }
    for(SourceGroupImpl *group : mSubGroups)
        group->collectPlayingSourceIds(sourceids);
}

void SourceGroupImpl::updatePausedStatus() const
{
    for(SourceImpl *alsrc : mSources)
        alsrc->checkPaused();
    for(SourceGroupImpl *group : mSubGroups)
        group->updatePausedStatus();
}

void SourceGroupImpl::pauseAll() const
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


void SourceGroupImpl::collectPausedSourceIds(Vector<ALuint> &sourceids) const
{
    for(SourceImpl *alsrc : mSources)
    {
        if(alsrc->isPaused())
            sourceids.push_back(alsrc->getId());
    }
    for(SourceGroupImpl *group : mSubGroups)
        group->collectPausedSourceIds(sourceids);
}

void SourceGroupImpl::updatePlayingStatus() const
{
    for(SourceImpl *alsrc : mSources)
        alsrc->unsetPaused();
    for(SourceGroupImpl *group : mSubGroups)
        group->updatePlayingStatus();
}

void SourceGroupImpl::resumeAll() const
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


void SourceGroupImpl::collectSourceIds(Vector<ALuint> &sourceids) const
{
    for(SourceImpl *alsrc : mSources)
    {
        if(ALuint id = alsrc->getId())
            sourceids.push_back(id);
    }
    for(SourceGroupImpl *group : mSubGroups)
        group->collectSourceIds(sourceids);
}

void SourceGroupImpl::updateStoppedStatus() const
{
    for(SourceImpl *alsrc : mSources)
    {
        mContext->removePlayingSource(alsrc);
        alsrc->makeStopped(false);
        mContext->send(&MessageHandler::sourceForceStopped, alsrc);
    }
    for(SourceGroupImpl *group : mSubGroups)
        group->updateStoppedStatus();
}

void SourceGroupImpl::stopAll() const
{
    CheckContext(mContext);

    Vector<ALuint> sourceids;
    sourceids.reserve(16);
    collectSourceIds(sourceids);
    if(!sourceids.empty())
    {
        auto lock = mContext->getSourceStreamLock();
        alSourceRewindv(sourceids.size(), sourceids.data());
        updateStoppedStatus();
    }
}


void SourceGroupImpl::release()
{
    CheckContext(mContext);
    Batcher batcher = mContext->getBatcher();
    for(SourceImpl *source : mSources)
        source->unsetGroup();
    mSources.clear();
    for(SourceGroupImpl *group : mSubGroups)
        group->unsetParentGroup();
    mSubGroups.clear();
    if(mParent)
        mParent->eraseSubGroup(this);
    mParent = nullptr;

    mContext->freeSourceGroup(this);
}


DECL_THUNK0(const String&, SourceGroup, getName, const)
DECL_THUNK1(void, SourceGroup, addSource,, Source)
DECL_THUNK1(void, SourceGroup, removeSource,, Source)
DECL_THUNK1(void, SourceGroup, addSources,, ArrayView<Source>)
DECL_THUNK1(void, SourceGroup, removeSources,, ArrayView<Source>)
DECL_THUNK1(void, SourceGroup, addSubGroup,, SourceGroup)
DECL_THUNK1(void, SourceGroup, removeSubGroup,, SourceGroup)
DECL_THUNK0(Vector<Source>, SourceGroup, getSources, const)
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
