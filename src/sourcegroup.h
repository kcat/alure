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

class ALSourceGroup : public SourceGroup, SourceGroupProps {
    ALContext *const mContext;

    Vector<ALSource*> mSources;
    Vector<ALSourceGroup*> mSubGroups;

    SourceGroupProps mParentProps;
    ALSourceGroup *mParent;

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
    ALSourceGroup(ALContext *context) : mContext(context), mParent(nullptr)
    { }
    // Avoid a warning about deleting an object with virtual functions but no
    // virtual destructor.
    virtual ~ALSourceGroup() { }

    ALfloat getAppliedGain() const { return mGain * mParentProps.mGain; }
    ALfloat getAppliedPitch() const { return mPitch * mParentProps.mPitch; }

    void addSource(Source *source) override final;
    void removeSource(Source *source) override final;

    void addSources(const Vector<Source*> &sources) override final;
    void removeSources(const Vector<Source*> &sources) override final;

    void addSubGroup(SourceGroup *group) override final;
    void removeSubGroup(SourceGroup *group) override final;

    Vector<Source*> getSources() override final;

    Vector<SourceGroup*> getSubGroups() override final;

    void setGain(ALfloat gain) override final;
    ALfloat getGain() const override final { return mGain; }

    void setPitch(ALfloat pitch) override final;
    ALfloat getPitch() const override final { return mPitch; }

    void pauseAll() const override final;
    void resumeAll() const override final;

    void stopAll() const override final;

    void release() override final;
};

} // namespace alure2

#endif /* SOURCEGROUP_H */
