#pragma once

#include "component.h"
#include "linkable.h"
#include "identifiable.h"
#include "objectmanager.h"

namespace glacier {

class BehaviourManager;

class Behaviour : public Component,
    public Identifiable<Behaviour>,
    public Linkable<BehaviourManager, Behaviour> 
{
public:
    friend class BehaviourManager;

    Behaviour() noexcept;
    //virtual void OnStart() {}
    virtual void Update(float dt) {};
    virtual void LateUpdate(float dt) {};

private:
    //bool start_ = false;
};

class BehaviourManager : public BaseManager<BehaviourManager, Behaviour> {
public:
    void Update(float dt);
    void LateUpdate(float dt);
};

}
