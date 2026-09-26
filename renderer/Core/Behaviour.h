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

    //Draws on top of the scene, inside the frame of the editor. It is a
    //drawing and not a window of the interface: it takes neither the mouse nor
    //the keyboard of the game, so a scene can tell the player what the keys do
    //while it is being played (a window of ImGui takes both as soon as it is
    //the one in focus, see Input). See Behaviour/DemoHud for one.
    virtual void DrawOverlay() {}

private:
    //bool start_ = false;
};

class BehaviourManager : public BaseManager<BehaviourManager, Behaviour> {
public:
    void Update(float dt);
    void LateUpdate(float dt);
    //every behaviour of the scene that shows something over it
    void DrawOverlay();
};

}
