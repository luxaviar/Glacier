#include "Behaviour/DemoHud.h"

#include <algorithm>
#include <format>
#include <string>
#include <imgui.h>

#include "Behaviour/PlayerController.h"
#include "Behaviour/ThirdPersonCamera.h"
#include "App.h"
#include "Core/GameObject.h"
#include "Core/Transform.h"
#include "Input/Input.h"
#include "Lux/Lux.h"
#include "Math/Util.h"
#include "Render/PerfStats.h"
#include "Render/Renderer.h"

namespace glacier {

LUX_IMPL(DemoHud, DemoHud)
LUX_CTOR(DemoHud)
LUX_FUNC(DemoHud, SetPlayer)
LUX_FUNC(DemoHud, SetCamera)
LUX_PROP_FUNC(DemoHud, visible)
LUX_IMPL_END

namespace {

//the hint is drawn by hand, so its spacing is spelled out here
constexpr float kMargin = 12.0f;
constexpr float kPadding = 10.0f;
constexpr float kLineSpacing = 4.0f;
constexpr float kColumnGap = 12.0f;
//the atlas of ImGui is small next to a whole window, so the hint is drawn bigger
constexpr float kTextScale = 1.25f;

constexpr ImU32 kBackground = IM_COL32(10, 12, 16, 150);
constexpr ImU32 kBorder = IM_COL32(255, 255, 255, 45);
constexpr ImU32 kKeyColor = IM_COL32(255, 214, 120, 255);
constexpr ImU32 kTextColor = IM_COL32(230, 230, 230, 255);
constexpr ImU32 kStatusColor = IM_COL32(150, 205, 255, 255);
constexpr ImU32 kStatsColor = IM_COL32(170, 170, 175, 255);
constexpr ImU32 kShadowColor = IM_COL32(0, 0, 0, 200);

//above this speed the character is running rather than walking: it sits between
//the speeds the clips of the pack were measured at, see LocomotionBlend
constexpr float kRunningSpeed = (kWalkSpeed + kRunSpeed) * 0.5f;
//below this speed the character counts as standing, as it does in the controller
constexpr float kMovingSpeed = 0.05f;

struct HintLine {
    const char* keys;
    const char* text;
};

const HintLine kHints[] = {
    { "W / S", "walk away from the view, walk towards it" },
    { "A / D", "walk across the view" },
    { "Shift", "run instead of walking" },
    { "Space", "jump" },
    { "Right mouse", "hold it and move the mouse to swing the view" },
    { "Wheel", "pull the camera in, push it out" },
    { "P", "pause the world" },
    { "H", "hide this hint" },
    { "F2", "write a screenshot of the scene" },
};

constexpr const char* kTitle = "Y Bot demo";

}

void DemoHud::DrawOverlay() {
    //H belongs to the hint and to nothing else, and it is read here rather than
    //in Update because the hint keeps drawing while the world is paused, and so
    //must the key that hides it
    if (Input::GetJustKeyDownState().H) {
        visible_ = !visible_;
    }

    if (!visible_) {
        return;
    }

    //what the character is doing, read from the controller of the scene
    std::string status;
    if (!player_) {
        status = "no character";
    }
    else {
        float speed = player_->speed();
        const char* motion = player_->airborne() ? "in the air"
            : (player_->turning() ? "turning where it stands"
                : (speed > kRunningSpeed ? "running"
                    : (speed > kMovingSpeed ? "walking" : "standing")));

        //the facing of the character is a yaw around +Z, shown the way a compass
        //would be
        float facing = math::WrapAngle(player_->yaw()) * math::kRad2Deg;
        if (facing < 0.0f) {
            facing += 360.0f;
        }

        if (camera_) {
            //the character and not this component: where an object of a scene
            //happens to be attached is not something the hint may assume
            float distance = player_->transform().position().Distance(camera_->transform().position());
            status = std::format("{}: {:.2f} m/s, facing {:.0f} deg, {:.2f} m from the camera",
                motion, speed, facing, distance);
        }
        else {
            status = std::format("{}: {:.2f} m/s, facing {:.0f} deg", motion, speed, facing);
        }
    }

    //the world is held still but this is still being drawn, and a character that
    //does not answer to the keys needs to say why
    if (App::Self() && App::Self()->paused()) {
        status = "paused - " + status;
    }

    auto* font = ImGui::GetFont();
    const float font_size = ImGui::GetFontSize() * kTextScale;

    auto text_width = [font, font_size](const char* text) {
        return text ? font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, text).x : 0.0f;
    };

    //the keys are a column of their own, so every line of the hint starts in the
    //same place
    float key_column = 0.0f;
    float width = text_width(kTitle);
    float text_column = 0.0f;
    for (const auto& hint : kHints) {
        key_column = std::max(key_column, text_width(hint.keys));
        text_column = std::max(text_column, text_width(hint.text));
    }
    width = std::max(width, key_column + kColumnGap + text_column);
    width = std::max(width, text_width(status.c_str()));

    const float line_height = font_size + kLineSpacing;
    const int line_count = (int)(sizeof(kHints) / sizeof(kHints[0])) + 2;
    const float height = line_height * (float)line_count;

    //ImGui keeps the strip of the main menu bar out of the work area of the
    //viewport, so the hint sits under the bar instead of behind it. It is
    //centred as well: the panels of the editor own both sides of the top of the
    //window - the hierarchy on the left, the inspector on the right - and the
    //hint is meant to be read over the middle of the scene
    auto work_pos = ImGui::GetMainViewport()->WorkPos;
    auto work_size = ImGui::GetMainViewport()->WorkSize;
    ImVec2 origin(work_pos.x + (work_size.x - width) * 0.5f, work_pos.y + kMargin);
    ImVec2 top_left(origin.x - kPadding, origin.y - kPadding);
    ImVec2 bottom_right(origin.x + width + kPadding, origin.y + height + kPadding);

    auto* draw_list = ImGui::GetForegroundDrawList();
    draw_list->AddRectFilled(top_left, bottom_right, kBackground, 5.0f);
    draw_list->AddRect(top_left, bottom_right, kBorder, 5.0f);

    auto draw_text = [&](const ImVec2& pos, ImU32 color, const char* text) {
        //a shadow keeps the line readable over a bright scene
        draw_list->AddText(font, font_size, ImVec2(pos.x + 1.0f, pos.y + 1.0f), kShadowColor, text);
        draw_list->AddText(font, font_size, pos, color, text);
    };

    float y = origin.y;
    draw_text(ImVec2(origin.x, y), kTextColor, kTitle);
    y += line_height;

    for (const auto& hint : kHints) {
        draw_text(ImVec2(origin.x, y), kKeyColor, hint.keys);
        draw_text(ImVec2(origin.x + key_column + kColumnGap, y), kTextColor, hint.text);
        y += line_height;
    }

    draw_text(ImVec2(origin.x, y), kStatusColor, status.c_str());
    y += line_height;
}

}
