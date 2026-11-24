#include "AutumnLeaves/autumnLeaves.hpp"

#include "Particles/menuParticles.hpp"
#include "UnityEngine/GameObject.hpp"
#include "assets.hpp"
#include "beatsaber-hook/shared/safeptr.hpp"
#include "bsml/shared/Helpers/utilities.hpp"

namespace Seasonal::autumnLeaves {
    namespace {
        safe_ptr<UnityEngine::GameObject*, true> leaves;
    }

    void CreateAutumnLeaves() {
        if (leaves) return;

        auto* texture = BSML::Utilities::LoadTextureRaw(
            static_cast<ArrayW<uint8_t>>(IncludedAssets::autumn_leaves_png));
        if (!texture) return;
        leaves = CreateMenuParticles("AutumnLeavesPS", {
            .capacity = 150, .rate = 5.0f, .lifetime = 14.0f, .height = 7.0f,
            .minSize = 0.18f, .maxSize = 0.18f, .gravityMultiplier = 0.025f,
            .fallSpeed = 0.0f, .wind = 0.12f, .flutter = 0.24f, .spin = 0.7f
        }, texture, 5);
    }

    void DestroyAutumnLeaves() {
        if (leaves) DestroyMenuParticles(leaves.ptr());
        leaves = nullptr;
    }
}
