#include "NewYears/newYears.hpp"

#include "Particles/menuParticles.hpp"
#include "UnityEngine/GameObject.hpp"
#include "assets.hpp"
#include "beatsaber-hook/shared/safeptr.hpp"
#include "bsml/shared/Helpers/utilities.hpp"

namespace NewYears::confetti {
    namespace {
        safe_ptr<UnityEngine::GameObject*, true> confetti;
    }

    void CreateConfetti() {
        if (confetti) return;

        auto* texture = BSML::Utilities::LoadTextureRaw(
            static_cast<ArrayW<uint8_t>>(IncludedAssets::confetti_streamers_png));
        if (!texture) return;
        confetti = Seasonal::CreateMenuParticles("NewYearsConfettiPS", {
            .capacity = 350, .rate = 16.0f, .lifetime = 16.0f, .height = 7.0f,
            .minSize = 0.045f, .maxSize = 0.12f, .gravityMultiplier = 0.008f,
            .fallSpeed = 0.0f, .wind = 0.0f, .flutter = 0.12f, .spin = 0.55f
        }, texture, 16);
    }

    void DestroyConfetti() {
        if (confetti) Seasonal::DestroyMenuParticles(confetti.ptr());
        confetti = nullptr;
    }
}
