
#include "sourcegroup.h"

#include <algorithm>

#include "source.h"
#include "context.h"

namespace alure
{

ALSourceGroup::ALSourceGroup(ALContext *context)
  : mContext(context), mParent(nullptr)
{
}


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


bool ALSourceGroup::findInSubGroups(ALSourceGroup *group)
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


void ALSourceGroup::update(ALfloat gain, ALfloat pitch)
{
    gain *= mGain;
    pitch *= mPitch;
    for(ALSource *alsrc : mSources)
        alsrc->groupPropUpdate(gain, pitch);
    for(ALSourceGroup *group : mSubGroups)
        group->update(gain, pitch);
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


void ALSourceGroup::addSubGroup(SourceGroup *group)
{
    ALSourceGroup *algrp = cast<ALSourceGroup*>(group);
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

void ALSourceGroup::removeSubGroup(SourceGroup *group)
{
    auto iter = std::lower_bound(mSubGroups.begin(), mSubGroups.end(), group);
    if(iter != mSubGroups.end() && *iter == group)
    {
        Batcher batcher = mContext->getBatcher();
        (*iter)->unsetParentGroup();
        mSubGroups.erase(iter);
    }
}


Vector<Source*> ALSourceGroup::getSources()
{
    Vector<Source*> ret;
    std::copy(mSources.begin(), mSources.end(), std::back_inserter(ret));
    return ret;
}

alure::Vector<SourceGroup*> ALSourceGroup::getSubGroups()
{
    Vector<SourceGroup*> ret;
    std::copy(mSubGroups.begin(), mSubGroups.end(), std::back_inserter(ret));
    return ret;
}


void ALSourceGroup::setGain(ALfloat gain)
{
    if(!(gain >= 0.0f))
        throw std::runtime_error("Gain out of range");
    CheckContext(mContext);
    mGain = gain;
    Batcher batcher = mContext->getBatcher();
    if(!mParent)
        update(1.0f, 1.0f);
    else
    {
        SourceGroupProps props;
        mParent->applyPropTree(props);
        update(props.mGain, props.mPitch);
    }
}

void ALSourceGroup::setPitch(ALfloat pitch)
{
    if(!(pitch > 0.0f))
        throw std::runtime_error("Pitch out of range");
    CheckContext(mContext);
    mPitch = pitch;
    Batcher batcher = mContext->getBatcher();
    if(!mParent)
        update(1.0f, 1.0f);
    else
    {
        SourceGroupProps props;
        mParent->applyPropTree(props);
        update(props.mGain, props.mPitch);
    }
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
    delete this;
}

} // namespace alure
