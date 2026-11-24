#include "UnityEngine/ParticleSystem_MinMaxCurve.hpp"
#include "beatsaber-hook/shared/safeptr.hpp"
#include "ChristmasEve/snow.hpp"
#include "assets.hpp"
#include "paper2_scotland2/shared/logger.hpp"
#define INFO(message, ...) Paper::Logger::fmtLogTag<Paper::LogLevel::INF>(message, "seasonalthemes" __VA_OPT__(, __VA_ARGS__))
#include "ChristmasEve/particleController.hpp"

#include "UnityEngine/Resources.hpp"
#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/Quaternion.hpp"
#include "UnityEngine/ParticleSystemRenderer.hpp"
#include "UnityEngine/Material.hpp"
#include "UnityEngine/Time.hpp"
#include "UnityEngine/MonoBehaviour.hpp"
#include "UnityEngine/BoxCollider.hpp"
#include "UnityEngine/Collider.hpp"
#include "custom-types/shared/macros.hpp"

#include "bsml/shared/Helpers/utilities.hpp"
#include <cmath>

using namespace UnityEngine;
using namespace ChristmasEveFlair;

namespace ChristmasEve::snowflakes {
    void UpdateWind();
}

DECLARE_CLASS_CODEGEN(ChristmasEve, SnowWindController, UnityEngine::MonoBehaviour) {
    DECLARE_INSTANCE_METHOD(void, Update);
};
DEFINE_TYPE(ChristmasEve, SnowWindController);

namespace ChristmasEve::snowflakes {
    namespace {
        safe_ptr<GameObject*, true> dustGo;
        safe_ptr<GameObject*, true> fineGo;
        safe_ptr<GameObject*, true> mediumGo;
        safe_ptr<GameObject*, true> snowflakesGo;
        safe_ptr<GameObject*, true> collisionFloor;
        float nextWindUpdate = 0.0f;

        // Physics colliders are required for World collision; the menu
        // floor is often visual-only. Keep this invisible static collider
        // separate from UI hierarchy and delete it on teardown.
        void EnsureSnowGround() {
            if (collisionFloor) return;
            auto* ground = GameObject::New_ctor("ChristmasEveSnowGroundCollision");
            if (!ground) { INFO("[SnowCollision] Ground proxy creation failed"); return; }
            ground->transform->position = Vector3(0.0f, -0.18f, 0.0f);
            auto* collider = ground->AddComponent<BoxCollider*>();
            if (!collider) {
                INFO("[SnowCollision] Ground BoxCollider creation failed");
                GameObject::Destroy(ground);
                return;
            }
            collider->set_size(Vector3(24.0f, 0.36f, 24.0f));
            collider->set_isTrigger(false);
            ground->layer = 0;
            collisionFloor = ground;
            INFO("[SnowCollision] Invisible static ground proxy installed y=0, span=24");
        }

        void LogSceneColliders() {
            int total = 0;
            int active = 0;
            for (auto* collider : Resources::FindObjectsOfTypeAll<Collider*>()) {
                if (!collider) continue;
                ++total;
                if (collider->gameObject && collider->gameObject->get_activeInHierarchy() && collider->get_enabled()) ++active;
            }
            INFO("[SnowCollision] Physics colliders discovered: total={}, active={}", total, active);
        }

        // Limits are per emitter, not merely approximate emission targets.
        // Visual weighting is 55% fine, 30% medium, 15% original crystal.
        ParticleSystem* SpawnLayer(const char* name, float height,
                                   float lifetime, float size, float rate,
                                   int limit, float gravity, bool collide,
                                   safe_ptr<GameObject*, true>& emitter) {
            auto root = GameObject::Instantiate(dustGo->gameObject);
            if (!root) return nullptr;
            root->name = name;
            root->transform->parent = nullptr;
            root->transform->position = Vector3(0.0f, height, 0.0f);
            root->transform->rotation = Quaternion::Euler(Vector3(0.0f, 0.0f, 0.0f));

            auto* ps = root->GetComponent<ParticleSystem*>();
            if (!ps) { GameObject::Destroy(root); return nullptr; }

            ps->main.set_startLifetime(ParticleSystem_MinMaxCurve::op_Implicit___UnityEngine__ParticleSystem_MinMaxCurve(lifetime));
            ParticleController::MainModule::set_startSize(ps, size);
            ps->main.set_startSpeedMultiplier(0.0f);
            ParticleController::MainModule::set_maxParticles(ps, limit);
            ParticleController::MainModule::set_simulationSpace(ps, ParticleController::SimulationSpace::World);
            ps->main.set_gravityModifierMultiplier(gravity);
            ps->emission.set_enabled(true);
            ps->emission.set_rateOverTime(ParticleSystem_MinMaxCurve::op_Implicit___UnityEngine__ParticleSystem_MinMaxCurve(rate));
            ParticleController::ShapeModule::set_shapeType(ps, ParticleController::ShapeType::Box);
            ParticleController::ShapeModule::set_position(ps, Vector3(0.0f, 0.0f, 0.0f));
            ParticleController::ShapeModule::set_rotation(ps, Vector3(0.0f, 0.0f, 0.0f));
            ParticleController::ShapeModule::set_scale(ps, Vector3(20.0f, 1.0f, 20.0f));

            // Collisions on thousands of tiny flakes are expensive in VR.
            ParticleController::CollisionModule::set_enabled(ps, collide);
            if (collide) {
                // Restore world/ground collisions only for medium and crystal flakes.
                ParticleController::CollisionModule::set_type(
                    ps, ParticleController::CollisionType::World);
                // The original theme did not override collision mode.
                ParticleController::CollisionModule::set_collidesWith(
                    ps, UnityEngine::LayerMask(0xffffffff));
                ParticleController::CollisionModule::set_dampen(ps, 0.08f);
                ParticleController::CollisionModule::set_bounce(ps, 0.0f);
                ParticleController::CollisionModule::set_lifetimeLoss(ps, 0.02f);
                // Preserve DustPS inherited dynamic-collider behavior from the original theme.
                // Preserve the original DustPS collision quality and mode.
                // Preserve inherited collision radius and scene interaction.
            }
            ParticleController::VelocityOverLifetimeModule::set_enabled(ps, !collide);
            ParticleController::NoiseModule::set_enabled(ps, true);
            ParticleController::NoiseModule::set_strength(ps, 0.045f);
            ParticleController::NoiseModule::set_frequency(ps, 0.22f);

            emitter = root;
            return ps;
        }

        void SetFlow(ParticleSystem* ps, float horizontal, float downward) {
            if (!ps) return;
            ParticleController::VelocityOverLifetimeModule::set_x(ps, horizontal);
            ParticleController::VelocityOverLifetimeModule::set_y(ps, downward);
            ParticleController::VelocityOverLifetimeModule::set_z(ps, 0.0f);
        }
    }

    void CreateSnowflakes() {
        if (fineGo && mediumGo && snowflakesGo) return;
        if (fineGo || mediumGo || snowflakesGo) DestroySnowflakes();

        if (!dustGo) for (auto particle : Resources::FindObjectsOfTypeAll<ParticleSystem*>()) {
            if (particle && particle->name == "DustPS") {
                dustGo = particle->gameObject;
                break;
            }
        }
        if (!dustGo) { INFO("[SnowBlend] DustPS source unavailable"); return; }

        dustGo->active = false;
        EnsureSnowGround();
        LogSceneColliders();
        auto* fine = SpawnLayer("ChristmasEveFineWindSnow", 7.3f, 7.0f,
                                0.012f, 850.0f, 6000, 0.012f, false, fineGo);
        auto* medium = SpawnLayer("ChristmasEveMediumSnow", 7.0f, 9.0f,
                                  0.029f, 260.0f, 2500, 0.018f, false, mediumGo);
        auto* crystal = SpawnLayer("ChristmasEveCrystalSnow", 7.0f, 10.0f,
                                   0.055f, 48.0f, 550, 0.10f, true, snowflakesGo);
        if (!fine || !medium || !crystal) {
            DestroySnowflakes();
            INFO("[SnowBlend] A layer could not be initialized; snow will retry on menu activation");
            return;
        }

        // Retain the original five-frame crystalline texture on the sparse
        // decorative layer, without applying it to the two finer layers.
        auto* renderer = snowflakesGo->GetComponent<ParticleSystemRenderer*>();
        auto templateMaterial = renderer ? renderer->get_material() : nullptr;
        if (templateMaterial) {
            auto crystalMaterial = UnityEngine::Object::Instantiate(templateMaterial);
            renderer->set_material(crystalMaterial);
            crystalMaterial->set_mainTexture( BSML::Utilities::LoadTextureRaw(
                static_cast<ArrayW<uint8_t>>(IncludedAssets::christmas_eve_snowflakes_png)));
            ParticleController::TextureSheetAnimationModule::set_enabled(crystal, true);
            ParticleController::TextureSheetAnimationModule::set_mode(
                crystal, ParticleController::AnimationMode::Grid);
            ParticleController::TextureSheetAnimationModule::set_numTilesX(crystal, 5);
            ParticleController::TextureSheetAnimationModule::set_numTilesY(crystal, 1);
            ParticleController::TextureSheetAnimationModule::set_frameOverTime(
                crystal, ParticleController::MinMaxCurve(0.0f, 5.0f));
            ParticleController::TextureSheetAnimationModule::set_startFrame(
                crystal, ParticleController::MinMaxCurve(0.0f, 5.0f));
            ParticleController::TextureSheetAnimationModule::set_cycleCount(crystal, 0);
        }

        SetFlow(fine, 0.65f, -0.90f);
        // Medium and crystal now use original DustPS gravity-driven motion.
        
        snowflakesGo->AddComponent<ChristmasEve::SnowWindController*>();
        fineGo->SetActive(true);
        mediumGo->SetActive(true);
        snowflakesGo->SetActive(true);
        INFO("[SnowBlend] Build51: fine 850/s and medium 260/s collision-free; crystal 48/s original-style gravity/world collision");
    }

    void UpdateWind() {
        if (!fineGo || !mediumGo || !snowflakesGo) return;
        const float now = Time::get_time();
        if (now < nextWindUpdate) return;
        nextWindUpdate = now + 0.35f;

        // Incommensurate slow oscillations prevent a repeating synchronized
        // gust pattern, while the low amplitudes preserve menu readability.
        const float gust = 0.23f * std::sin(now * 0.17f)
                         + 0.11f * std::sin(now * 0.43f + 1.7f)
                         + 0.06f * std::sin(now * 0.083f + 2.4f);
        SetFlow(fineGo->GetComponent<ParticleSystem*>(), 0.65f + gust, -0.90f);
        // Do not override gravity-driven colliding layers each frame.
        
    }

    void DestroySnowflakes() {
        for (auto* emitter : {&fineGo, &mediumGo, &snowflakesGo, &collisionFloor}) {
            if (*emitter) {
                (*emitter)->SetActive(false);
                GameObject::Destroy(emitter->ptr());
            }
            *emitter = nullptr;
        }
        nextWindUpdate = 0.0f;
    }
}

void ChristmasEve::SnowWindController::Update() {
    ChristmasEve::snowflakes::UpdateWind();
}
