#include "ChristmasEve/snow.hpp"

#include "Particles/menuParticles.hpp"
#include "UnityEngine/GameObject.hpp"
#include "assets.hpp"
#include "beatsaber-hook/shared/safeptr.hpp"
#include "bsml/shared/Helpers/utilities.hpp"

namespace ChristmasEve::snowflakes {
    namespace {
        safe_ptr<UnityEngine::GameObject*, true> fine;
        safe_ptr<UnityEngine::GameObject*, true> medium;
        safe_ptr<UnityEngine::GameObject*, true> crystal;
    }

    void CreateSnowflakes() {
        if (fine && medium && crystal) return;
        DestroySnowflakes();

        auto* crystalTexture = BSML::Utilities::LoadTextureRaw(
            static_cast<ArrayW<uint8_t>>(IncludedAssets::christmas_eve_snowflakes_png));
        if (!crystalTexture) return;

        fine = Seasonal::CreateMenuParticles("ChristmasEveFineWindSnow", {
            .capacity = 6000, .rate = 850.0f, .lifetime = 7.0f, .height = 7.3f,
            .minSize = 0.012f, .maxSize = 0.012f, .gravityMultiplier = 0.012f,
            .fallSpeed = 0.90f, .wind = 0.65f, .flutter = 0.045f, .spin = 0.2f
        });
        medium = Seasonal::CreateMenuParticles("ChristmasEveMediumSnow", {
            .capacity = 2500, .rate = 260.0f, .lifetime = 9.0f, .height = 7.0f,
            .minSize = 0.029f, .maxSize = 0.029f, .gravityMultiplier = 0.018f,
            .fallSpeed = 0.22f, .wind = 0.0f, .flutter = 0.045f, .spin = 0.35f
        });
        crystal = Seasonal::CreateMenuParticles("ChristmasEveCrystalSnow", {
            .capacity = 550, .rate = 48.0f, .lifetime = 10.0f, .height = 7.0f,
            .minSize = 0.055f, .maxSize = 0.055f, .gravityMultiplier = 0.10f,
            .fallSpeed = 0.0f, .wind = 0.0f, .flutter = 0.045f, .spin = 0.45f,
            .useDustMaterial = true
        }, crystalTexture, 5);
        if (!fine || !medium || !crystal) DestroySnowflakes();
    }

    void DestroySnowflakes() {
        for (auto* layer : {&fine, &medium, &crystal}) {
            if (*layer) Seasonal::DestroyMenuParticles(layer->ptr());
            *layer = nullptr;
        }
    }
}
