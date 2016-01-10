
#include "sourcegroup.h"

#include <algorithm>

#include "source.h"
#include "context.h"

namespace alure
{

ALSourceGroup::ALSourceGroup(ALContext *context)
  : mContext(context), mGain(1.0f)
{
}

Vector<Source*> ALSourceGroup::getSources()
{
    Vector<Source*> ret;
    std::copy(mSources.begin(), mSources.end(), std::back_inserter(ret));
    return ret;
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

    for(ALSource *alsrc : alsrcs)
    {
        auto iter = std::lower_bound(mSources.begin(), mSources.end(), alsrc);
        if(iter != mSources.end() && *iter == alsrc) continue;

        mSources.insert(iter, alsrc);
        alsrc->setGroup(this);
    }
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

void ALSourceGroup::removeSources(const Vector<Source*> &sources)
{
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


void ALSourceGroup::setGain(ALfloat gain)
{
    if(!(gain >= 0.0f))
        throw std::runtime_error("Gain out of range");
    CheckContext(mContext);
    for(ALSource *alsrc : mSources)
        alsrc->groupGainUpdate(gain);
    mGain = gain;
}


void ALSourceGroup::release()
{
    CheckContext(mContext);
    for(ALSource *source : mSources)
        source->unsetGroup();
    mContext->freeSourceGroup(this);
    delete this;
}

} // namespace alure
