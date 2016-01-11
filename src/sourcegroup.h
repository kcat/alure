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

public:
    ALSourceGroup(ALContext *context);
    virtual ~ALSourceGroup () { }

    ALfloat getAppliedGain() const { return mGain * mParentProps.mGain; }
    ALfloat getAppliedPitch() const { return mPitch * mParentProps.mPitch; }

    virtual void addSource(Source *source) final;
    virtual void removeSource(Source *source) final;

    virtual void addSources(const Vector<Source*> &sources) final;
    virtual void removeSources(const Vector<Source*> &sources) final;

    virtual void addSubGroup(SourceGroup *group) final;
    virtual void removeSubGroup(SourceGroup *group) final;

    virtual Vector<Source*> getSources() final;

    virtual Vector<SourceGroup*> getSubGroups() final;

    virtual void setGain(ALfloat gain) final;
    virtual ALfloat getGain() const final { return mGain; }

    virtual void setPitch(ALfloat pitch) final;
    virtual ALfloat getPitch() const final { return mPitch; }

    virtual void release() final;
};

} // namespace alure2

#endif /* SOURCEGROUP_H */
