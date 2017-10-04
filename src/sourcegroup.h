#ifndef SOURCEGROUP_H
#define SOURCEGROUP_H

#include "main.h"


namespace alure
{

class ALContext;
class ALSource;

struct SourceGroupProps {
    ALfloat mGain;
    ALfloat mPitch;

    SourceGroupProps() : mGain(1.0f), mPitch(1.0f) { }
};

class ALSourceGroup : SourceGroupProps {
    ALContext *const mContext;

    Vector<ALSource*> mSources;
    Vector<ALSourceGroup*> mSubGroups;

    SourceGroupProps mParentProps;
    ALSourceGroup *mParent;

    const String mName;

    void applyPropTree(SourceGroupProps &props) const
    {
        props.mGain *= mGain;
        props.mPitch *= mPitch;
        if(mParent)
            mParent->applyPropTree(props);
    }

    void update(ALfloat gain, ALfloat pitch);

    void setParentGroup(ALSourceGroup *group);
    void unsetParentGroup();

    void eraseSubGroup(ALSourceGroup *group);

    bool findInSubGroups(ALSourceGroup *group) const;

    void collectPlayingSourceIds(Vector<ALuint> &sourceids) const;
    void updatePausedStatus() const;

    void collectPausedSourceIds(Vector<ALuint> &sourceids) const;
    void updatePlayingStatus() const;

    void collectSourceIds(Vector<ALuint> &sourceids) const;
    void updateStoppedStatus() const;

public:
    ALSourceGroup(ALContext *context, String name)
      : mContext(context), mParent(nullptr), mName(std::move(name))
    { }

    ALfloat getAppliedGain() const { return mGain * mParentProps.mGain; }
    ALfloat getAppliedPitch() const { return mPitch * mParentProps.mPitch; }

    void addSource(Source source);
    void removeSource(Source source);

    void addSources(const Vector<Source> &sources);
    void removeSources(const Vector<Source> &sources);

    void addSubGroup(SourceGroup group);
    void removeSubGroup(SourceGroup group);

    Vector<Source> getSources() const;

    Vector<SourceGroup> getSubGroups() const;

    void setGain(ALfloat gain);
    ALfloat getGain() const { return mGain; }

    void setPitch(ALfloat pitch);
    ALfloat getPitch() const { return mPitch; }

    void pauseAll() const;
    void resumeAll() const;

    void stopAll() const;

    const String &getName() const { return mName; }

    void release();
};

} // namespace alure2

#endif /* SOURCEGROUP_H */
