#pragma once

#include "Core/Behaviour.h"

namespace glacier {

class PlayerController;
class ThirdPersonCamera;

//What the keys of the demo do and what the character is doing with them, drawn
//in a corner over the scene.
//
//The hint is a drawing into the frame of the editor (Behaviour::DrawOverlay)
//and not a window of ImGui, because the demo is played with the keyboard while
//the hint is up and a window of the interface takes the keyboard of the game as
//soon as it is the one in focus. Being a drawing it also cannot be grabbed by
//the mouse, so the view can still be swung through it.
//
//H hides it, which is what a viewer wants once the keys are known; F2 writes a
//screenshot of the scene, the interface of the editor and this hint are drawn
//into the back buffer afterwards and are not part of that image.
class DemoHud : public Behaviour {
public:
    //the character and the camera the line of the state is read from; either
    //may be missing, the hint then says no more than the keys
    void SetPlayer(PlayerController* player) { player_ = player; }
    PlayerController* player() const { return player_; }
    void SetCamera(ThirdPersonCamera* camera) { camera_ = camera; }
    ThirdPersonCamera* camera() const { return camera_; }

    bool visible() const { return visible_; }
    void visible(bool v) { visible_ = v; }

    void DrawOverlay() override;

private:
    PlayerController* player_ = nullptr;
    ThirdPersonCamera* camera_ = nullptr;
    bool visible_ = true;
};

}
