
#include "sourcegroup.h"

#include <algorithm>

#include "source.h"

namespace alure
{

ALSourceGroup::ALSourceGroup()
  : mGain(1.0f)
{
}

void ALSourceGroup::release()
{
    for(ALSource *source : mSources)
        /*source->unsetGroup()*/;
}

std::vector<Source*> ALSourceGroup::getSources()
{
    std::vector<Source*> ret;
    std::copy(mSources.begin(), mSources.end(), std::back_inserter(ret));
    return ret;
}

void ALSourceGroup::addSource(Source *source)
{
    ALSource *alsrc = cast<ALSource*>(source);
    if(!alsrc) throw std::runtime_error("Source is not valid");

    auto iter = std::lower_bound(mSources.begin(), mSources.end(), alsrc);
    if(iter != mSources.end() && *iter == alsrc) return;

    mSources.insert(iter, alsrc);
    //alsrc->setGroup(this);
}

void ALSourceGroup::addSources(const std::vector<Source*> &sources)
{
    if(sources.empty())
        return;

    std::vector<ALSource*> alsrcs;
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
        //alsrc->setGroup(this);
    }
}


void ALSourceGroup::removeSource(Source *source)
{
    auto iter = std::lower_bound(mSources.begin(), mSources.end(), source);
    if(iter != mSources.end() && *iter == source)
    {
        //(*iter)->unsetGroup();
        mSources.erase(iter);
    }
}

void ALSourceGroup::removeSources(const std::vector<Source*> &sources)
{
    for(Source *source : sources)
    {
        auto iter = std::lower_bound(mSources.begin(), mSources.end(), source);
        if(iter != mSources.end() && *iter == source)
        {
            //(*iter)->unsetGroup();
            mSources.erase(iter);
        }
    }
}


void ALSourceGroup::setGain(ALfloat gain)
{
    if(!(gain >= 0.0f))
        throw std::runtime_error("Gain out of range");
    /*CheckContext(mContext);
    for(ALSource *alsrc : mSources)
        alsrc->groupGainUpdate(gain);*/
    mGain = gain;
}


} // namespace alure
