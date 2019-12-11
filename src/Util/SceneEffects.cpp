/*!
  @file
  @author Shin'ichiro Nakaoka
*/

#include "SceneEffects.h"
#include "SceneNodeClassRegistry.h"

using namespace std;
using namespace cnoid;


SgFog::SgFog(int classId)
    : SgPreprocessed(classId)
{
    color_.setOnes();
    visibilityRange_ = 0.0f;
}


SgFog::SgFog()
    : SgFog(findClassId<SgFog>())
{

}


SgFog::SgFog(const SgFog& org)
    : SgPreprocessed(org)
{
    color_ = org.color_;
    visibilityRange_ = org.visibilityRange_;
}


Referenced* SgFog::doClone(CloneMap*) const
{
    return new SgFog(*this);
}


SgOutlineGroup::SgOutlineGroup(int classId)
    : SgGroup(classId)
{
    lineWidth_ = 1.0;
    color_ << 1.0, 0.0, 0.0;
}


SgOutlineGroup::SgOutlineGroup()
    : SgOutlineGroup(findClassId<SgOutlineGroup>())
{

}


Referenced* SgOutlineGroup::doClone(CloneMap*) const
{
    return new SgOutlineGroup(*this);
}


SgLightweightRenderingGroup::SgLightweightRenderingGroup()
    : SgGroup(findClassId<SgLightweightRenderingGroup>())
{

}


Referenced* SgLightweightRenderingGroup::doClone(CloneMap*) const
{
    return new SgLightweightRenderingGroup(*this);
}


namespace {

struct NodeTypeRegistration {
    NodeTypeRegistration() {
        SceneNodeClassRegistry::instance()
            .registerClass<SgFog, SgPreprocessed>()
            .registerClass<SgOutlineGroup, SgGroup>()
            .registerClass<SgLightweightRenderingGroup, SgGroup>();
    }
} registration;

}
