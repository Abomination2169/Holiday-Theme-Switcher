#include "beatsaber-hook/shared/safeptr.hpp"
#include "NewYears/newYears.hpp"
#include "AutumnLeaves/particleController.hpp"
#include "assets.hpp"
#include "bsml/shared/Helpers/utilities.hpp"
#include "UnityEngine/Resources.hpp"
#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/Quaternion.hpp"
#include "UnityEngine/ParticleSystemRenderer.hpp"
#include "UnityEngine/Material.hpp"
#include "UnityEngine/Shader.hpp"
#include "UnityEngine/Color.hpp"
#include "UnityEngine/LayerMask.hpp"

using namespace UnityEngine;
using namespace Flair;

namespace NewYears::confetti {
static safe_ptr<GameObject*, true> templateDust;
static safe_ptr<GameObject*, true> confettiGo;

void CreateConfetti() {
    if (confettiGo) return;
    if (!templateDust) {
        for (auto particle : Resources::FindObjectsOfTypeAll<ParticleSystem*>()) {
            if (particle && particle->name == "DustPS" && particle->gameObject) {
                templateDust = particle->gameObject;
                break;
            }
        }
    }
    if (!templateDust) return;
    const bool dustWasActive = templateDust->get_activeSelf();
    templateDust->SetActive(false);
    confettiGo = GameObject::Instantiate(templateDust->gameObject);
    templateDust->SetActive(dustWasActive);
    if (!confettiGo) return;
    confettiGo->name = "NewYearsConfettiPS";
    confettiGo->transform->parent = nullptr;
    confettiGo->transform->position = Vector3(0.0f, 7.0f, 0.0f);
    confettiGo->transform->rotation = Quaternion::Euler(Vector3(0.0f, 0.0f, 0.0f));

    auto* particles = confettiGo->GetComponent<ParticleSystem*>();
    auto* renderer = confettiGo->GetComponent<ParticleSystemRenderer*>();
    if (!particles || !renderer) { DestroyConfetti(); return; }

    ParticleController::MainModule::set_startLifetime(particles, 16.0f);
    ParticleController::MainModule::set_startSize(particles,
        ParticleController::MinMaxCurve(0.045f, 0.12f));
    ParticleController::MainModule::set_startRotation(particles,
        ParticleController::MinMaxCurve(0.0f, 6.2831853f));
    ParticleController::RotationOverLifetimeModule::set_enabled(particles, true);
    ParticleController::RotationOverLifetimeModule::set_z(particles,
        ParticleController::MinMaxCurve(-0.55f, 0.55f));
    ParticleController::MainModule::set_maxParticles(particles, 350);
    ParticleController::MainModule::set_gravityModifier(particles, 0.008f);

    ParticleController::MinMaxGradient white{};
    white.mode = ParticleController::GradientMode::Color;
    white.colorMin = Color(1, 1, 1, 1);
    white.colorMax = Color(1, 1, 1, 1);
    ParticleController::MainModule::set_startColor(particles, white);
    ParticleController::ColorOverLifetimeModule::set_enabled(particles, false);
    ParticleController::ColorBySpeedModule::set_enabled(particles, false);
    ParticleController::EmissionModule::set_enabled(particles, true);
    ParticleController::EmissionModule::set_rateOverTime(particles, 16.0f);
    ParticleController::ShapeModule::set_shapeType(particles, ParticleController::ShapeType::Box);
    ParticleController::ShapeModule::set_position(particles, Vector3(0, 0, 0));
    ParticleController::ShapeModule::set_rotation(particles, Vector3(0, 0, 0));
    ParticleController::ShapeModule::set_scale(particles, Vector3(20.0f, 1.0f, 20.0f));

    ParticleController::CollisionModule::set_enabled(particles, true);
    ParticleController::CollisionModule::set_type(particles, ParticleController::CollisionType::World);
    ParticleController::CollisionModule::set_collidesWith(particles, LayerMask(0xffffffff));
    ParticleController::CollisionModule::set_dampen(particles, 0.08f);
    ParticleController::CollisionModule::set_bounce(particles, 0.0f);
    ParticleController::CollisionModule::set_lifetimeLoss(particles, 0.02f);

    // Do not change the shared DustPS material: Halloween and Christmas
    // depend on their textures remaining isolated when themes switch.
    auto shared = renderer->get_material();
    if (!shared) { DestroyConfetti(); return; }
    auto material = Object::Instantiate(shared);
    renderer->set_material(material);
    auto texture = BSML::Utilities::LoadTextureRaw(
        static_cast<ArrayW<uint8_t>>(IncludedAssets::confetti_streamers_png));
    auto shader = Shader::Find("Sprites/Default");
    if (!texture || !shader) { DestroyConfetti(); return; }
    material->set_shader(shader);
    material->SetTexture(StringW("_MainTex"), texture);
    material->SetColor(StringW("_Color"), Color(1, 1, 1, 1));

    constexpr int frames = 16;
    ParticleController::TextureSheetAnimationModule::set_enabled(particles, true);
    ParticleController::TextureSheetAnimationModule::set_mode(
        particles, ParticleController::AnimationMode::Grid);
    ParticleController::TextureSheetAnimationModule::set_numTilesX(particles, frames);
    ParticleController::TextureSheetAnimationModule::set_numTilesY(particles, 1);
    ParticleController::TextureSheetAnimationModule::set_frameOverTime(
        particles, ParticleController::MinMaxCurve(0.0f, 0.0f));
    ParticleController::TextureSheetAnimationModule::set_startFrame(
        particles, ParticleController::MinMaxCurve(0.0f, frames));
    ParticleController::TextureSheetAnimationModule::set_cycleCount(particles, 0);
    confettiGo->active = true;
}
void DestroyConfetti() {
    if (confettiGo) {
        confettiGo->SetActive(false);
        auto ps = confettiGo->GetComponent<ParticleSystem*>();
        if (ps) { ps->Stop(); ps->Clear(); }
        GameObject::Destroy(confettiGo.ptr());
        confettiGo = nullptr;
    }
}
}
