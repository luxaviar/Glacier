#include "World.h"
#include <algorithm>
#include "Types.h"
#include "Physics/Collision/CollisionSystem.h"
#include "Physics/Collision/Boardphase/Dynamicbvh.h"
#include "Physics/Collision/Narrowphase/ContactDetector.h"
#include "Physics/Collision/Narrowphase/Gjk.h"
#include "Physics/Collision/Narrowphase/Epa.h"
#include "Physics/Dynamic/ContactSolver.h"
#include "Physics/Dynamic/Island.h"
#include "Render/Editor/Gizmos.h"

namespace glacier {

namespace physics {

World::World()
{
    filter_.SetCollision(0, 0, false); //default layer will collide each other

    auto gjk = std::make_unique<Gjk>(50);
    auto epa = std::make_unique<Epa>(128, 0.001f);
    auto narrow = std::make_unique<ContactDetector>(std::move(gjk), std::move(epa));
    auto board = std::make_unique<DynamicBvh>(&filter_, 4096, 0.1f, 2.0f);

    collision_system_ = std::make_unique<CollisionSystem>(std::move(board), std::move(narrow));

    contact_solver_ = std::make_unique<ContactSolver>(15);
    contact_solver_->inv_interval((float)frequency_);

    island_ = std::make_unique<Island>();

    gravity(Vec3f{ 0.0f, -9.8f, 0.0f });
}

World::~World() {
    Clear();
}

void World::AddCollider(Collider* collider) {
    collision_system_->AddCollider(collider);
}

void World::RemoveCollider(Collider* collider) {
    collision_system_->RemoveCollider(collider);
    contact_solver_->ClearContact(collider);
}

void World::UpdateBody(Rigidbody* body) {
    body->IntegratePosition(inv_frequency_);
    uint32_t ver = body->transform().version();
    if (body->tx_ver_ != ver) {
        body->tx_ver_ = ver;
        collision_system_->Update(body);
    }
}

void World::Advance(float dt_us) {
    if (pause_) return;
    
    delta_time_ += dt_us;

    while (delta_time_ >= inv_frequency_) {
        delta_time_ -= inv_frequency_;
        Step();
    }
}

void World::Step() {
    if (pause_) return;

    FixedUpdate(inv_frequency_);

    enters_.clear();
    exits_.clear();
    auto& collideList = collision_system_->Step(enters_, exits_);

    contact_solver_->Step(collideList); //dynamic solver

    island_->Step(contact_solver_.get(), objects_, inv_frequency_);
    for (auto& body : objects_) {
        UpdateBody(body.data);
    }

    ProcessCallBack();

    ++frame_;
}

void World::FixedUpdate(float deltaTime) {
    for (auto& body : objects_) {
        body->FixedUpdate(deltaTime);
    }
}

void World::ProcessCallBack() {
    for (const auto& info : enters_) {
        Collider* a = info.first;
        Collider* b = info.second;
        if (a->IsActive() && b->IsActive()) {
            if (info.sensor) {
                a->OnSensorEnter(b);
                b->OnSensorEnter(a);
            } else {
                auto& ci = info.contact;
                a->OnCollisionEnter(ContactInfo(true, b, ci.pointA, ci.pointB, ci.normal, ci.penetration));
                b->OnCollisionEnter(ContactInfo(false, a, ci.pointB, ci.pointA, -ci.normal, ci.penetration));
            }
        }
    }

    for (const auto& info : exits_) {
        Collider* a = info.first;
        Collider* b = info.second;
        if (a->IsActive() && b->IsActive()) {
            if (info.sensor) {
                a->OnSensorExit(b);
                b->OnSensorExit(a);
            } else {
                auto& ci = info.contact;
                a->OnCollisionExit(ContactInfo(true, b, ci.pointA, ci.pointB, ci.normal, ci.penetration));
                b->OnCollisionExit(ContactInfo(false, a, ci.pointB, ci.pointA, -ci.normal, ci.penetration));
            }
        }
    }
}

RayHitResult World::RayCast(const Ray& ray, float max, uint32_t layer_mask, bool query_sensor) {
    return collision_system_->RayCast(ray, max, layer_mask, query_sensor);
}

void World::Clear() {
    collision_system_->Clear();
    contact_solver_->Clear();

    objects_.clear();

    frame_ = 0;
}

void World::Pause() {
    pause_ = true;
}

void World::Resume() {
    pause_ = false;
}

void World::OnDrawGizmos(bool draw_bvh) {
    collision_system_->OnDrawGizmos(draw_bvh);
    //contact_solver_->OnDrawGizmos();
}

}
}

