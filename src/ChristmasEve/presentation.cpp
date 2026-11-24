#include "ChristmasEve/presentation.hpp"
#include "paper2_scotland2/shared/logger.hpp"
#define INFO(message, ...) Paper::Logger::fmtLogTag<Paper::LogLevel::INF>(message, "seasonalthemes" __VA_OPT__(, __VA_ARGS__))
#include "assets.hpp"
#include "bsml/shared/BSML.hpp"
#include "bsml/shared/Helpers/utilities.hpp"

#include "GlobalNamespace/TubeBloomPrePassLight.hpp"
#include "UnityEngine/Color.hpp"
#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/SpriteRenderer.hpp"
#include "UnityEngine/Vector2.hpp"
#include "UnityEngine/Vector3.hpp"
#include "UnityEngine/MonoBehaviour.hpp"
#include "UnityEngine/Time.hpp"
#include "UnityEngine/UI/Image.hpp"
#include "UnityEngine/RectTransform.hpp"
#include "custom-types/shared/macros.hpp"
#include <array>
#include <cmath>

DECLARE_CLASS_CODEGEN(ChristmasEve, GarlandSparkleController, UnityEngine::MonoBehaviour) {
    DECLARE_INSTANCE_METHOD(void, Update);
};

DEFINE_TYPE(ChristmasEve, GarlandSparkleController);

namespace ChristmasEve {
    void UpdateGarlandSparkles();
}

namespace ChristmasEve {

    namespace {
        SafePtrUnity<UnityEngine::GameObject> solidE, originalFlicker, originalNeon;

        void ReplaceFlickeringE() {
            // Restore the original canvas placement that produced a visible thin E in Quest.
            auto flickeringE = UnityEngine::GameObject::Find("EFlickering");
            if (!flickeringE || !flickeringE->transform) { INFO("[SolidE] EFlickering anchor not found"); return; }
            INFO("[SolidE] EFlickering anchor found");

            if (solidE) return;
            auto parent = flickeringE->transform->parent;
            if (!parent) { INFO("[SolidE] No anchor parent"); return; }
            INFO("[SolidE] Anchor parent found");
            if (!parent->gameObject->get_activeInHierarchy()) INFO("[SolidE] WARNING: anchor parent is inactive in hierarchy");

            auto* imageSprite = BSML::Utilities::LoadSpriteRaw(
                static_cast<ArrayW<uint8_t>>(IncludedAssets::solid_e_red_png));
            if (!imageSprite) { INFO("[SolidE] PNG sprite loading FAILED"); return; }
            INFO("[SolidE] PNG sprite loaded");

            auto* replacement = UnityEngine::GameObject::New_ctor("ChristmasSolidE");
            if (!replacement || !replacement->transform) { INFO("[SolidE] Replacement object creation FAILED"); return; }
            INFO("[SolidE] Replacement object created");

            replacement->transform->SetParent(parent);
            // The original E transform is our anchor, not the entire logo's origin.
            // Sprite dimensions and depth are provisional until inspected in Quest.
            replacement->transform->position = flickeringE->transform->position;

            auto* canvas = BSML::Lite::CreateCanvas();
            if (!canvas || !canvas->transform) {
                INFO("[SolidE] Canvas creation FAILED");
                UnityEngine::GameObject::Destroy(replacement);
                return;
            }
            INFO("[SolidE] Canvas created");
            canvas->name = "SolidECanvas";
            canvas->transform->parent = replacement->transform;
            canvas->transform->localPosition = UnityEngine::Vector3(0.0f, 0.0f, -0.02f);

            auto* image = BSML::Lite::CreateImage(
                canvas, imageSprite, UnityEngine::Vector2(-12.0f, 0.0f),
                UnityEngine::Vector2(165.0f, 204.0f));
            if (!image) {
                INFO("[SolidE] Image creation FAILED");
                UnityEngine::GameObject::Destroy(replacement);
                return;
            }
            image->gameObject->name = "SolidEImage";
            INFO("[SolidE] Image created");
            if (!replacement->get_activeInHierarchy()) INFO("[SolidE] WARNING: replacement inactive in hierarchy");
            if (!canvas->gameObject->get_activeInHierarchy()) INFO("[SolidE] WARNING: canvas inactive in hierarchy");
            if (!image->gameObject->get_activeInHierarchy()) INFO("[SolidE] WARNING: image inactive in hierarchy");

            // Switch only once the PNG image exists; original flicker stays suppressed.
            solidE = replacement;
            originalFlicker = flickeringE.ptr();
            auto neon = UnityEngine::GameObject::Find("ENeon");
            if (neon) { originalNeon = neon.ptr(); neon->SetActive(false); }
            INFO("[SolidE] Replacement installed; Quest visibility still unverified");
            flickeringE->SetActive(false);
            // ENeon can have its own original E glow, which would keep flickering.
            // Hide that E-only layer; the PNG includes its static red neon glow.
            // Original E-only neon already suppressed before asset loading.
        }

        // Build #63: dark emerald reference tree and glossy left-hook cane, half-size.
        // Tree artwork contains its two presents; neither asset includes lettering.
        SafePtrUnity<UnityEngine::GameObject> christmasSideDecorations;

        void InstallChristmasSideDecorations(UnityEngine::Transform* anchorParent,
                                              UnityEngine::Vector3 anchorPosition) {
            if (christmasSideDecorations) return;
            if (!anchorParent || !anchorParent->gameObject->get_activeInHierarchy()) {
                INFO("[SideDecor] Active logo anchor unavailable");
                return;
            }
            auto* treeSprite = BSML::Utilities::LoadSpriteRaw(
                static_cast<ArrayW<uint8_t>>(IncludedAssets::christmas_tree_presents_v66_png));
            auto* caneSprite = BSML::Utilities::LoadSpriteRaw(
                static_cast<ArrayW<uint8_t>>(IncludedAssets::christmas_candy_cane_v63_png));
            if (!treeSprite || !caneSprite) {
                INFO("[SideDecor] PNG sprite decode failed: tree={} cane={}",
                     treeSprite != nullptr, caneSprite != nullptr);
                return;
            }
            auto* root = UnityEngine::GameObject::New_ctor("ChristmasSideDecorations");
            if (!root || !root->transform) return;
            root->transform->SetParent(anchorParent);
            root->transform->position = anchorPosition;
            auto* canvas = BSML::Lite::CreateCanvas();
            if (!canvas || !canvas->transform) {
                UnityEngine::GameObject::Destroy(root);
                return;
            }
            canvas->name = "ChristmasSideDecorCanvas";
            canvas->transform->parent = root->transform;
            canvas->transform->localPosition = UnityEngine::Vector3(0.0f, 0.0f, -0.019f);

            // Build 81: mirror the image centres about the established logo/
            // garland centre X=33. Both share a 204-unit height and Y=-69,
            // aligning their baselines while keeping each sprite's aspect ratio.
            // The cane's 84:146 ratio becomes approximately 117:204.
            constexpr float decorationCentreX = 33.0f;
            constexpr float flankOffsetX = 307.0f;
            constexpr float decorationY = -69.0f;
            auto* cane = BSML::Lite::CreateImage(
                canvas, caneSprite,
                UnityEngine::Vector2(decorationCentreX - flankOffsetX + 30.0f, decorationY),
                UnityEngine::Vector2(117.37f, 204.0f));
            auto* tree = BSML::Lite::CreateImage(
                canvas, treeSprite,
                UnityEngine::Vector2(decorationCentreX + flankOffsetX, decorationY),
                UnityEngine::Vector2(137.0f, 204.0f));
            if (!cane || !tree) {
                INFO("[SideDecor] CreateImage failed: tree={} cane={}",
                     tree != nullptr, cane != nullptr);
                UnityEngine::GameObject::Destroy(root);
                return;
            }
            cane->gameObject->name = "ChristmasCandyCaneImage";
            tree->gameObject->name = "ChristmasTreePresentsImage";
            christmasSideDecorations = root;
            INFO("[SideDecor] Static flanking decorations installed; Quest placement unverified");
            if (!root->get_activeInHierarchy() ||
                !canvas->gameObject->get_activeInHierarchy())
                INFO("[SideDecor] WARNING: decoration hierarchy inactive");
        }

        SafePtrUnity<UnityEngine::GameObject> christmasGarland;
        std::array<UnityEngine::UI::Image*, 8> sparkleImages{};

        constexpr std::array<float, 8> sparkleX{-205.0f, -174.0f, -128.0f, -90.0f,
                                                  90.0f, 133.0f, 174.0f, 210.0f};
        constexpr std::array<float, 8> sparkleY{24.0f, -18.0f, 31.0f, -23.0f,
                                                  27.0f, -20.0f, 31.0f, -17.0f};

        void InstallChristmasGarland(UnityEngine::Transform* anchorParent,
                                     UnityEngine::Vector3 anchorPosition) {
            // Capture EFlickering's live transform before ReplaceFlickeringE disables it.
            // GameObject::Find cannot locate the inactive EFlickering afterwards.
            if (christmasGarland) return;
            if (!anchorParent || !anchorParent->gameObject->get_activeInHierarchy()) {
                INFO("[Garland] Active logo anchor parent unavailable");
                return;
            }
            INFO("[Garland] Active logo anchor captured before E suppression");

            auto* garlandSprite = BSML::Utilities::LoadSpriteRaw(
                static_cast<ArrayW<uint8_t>>(IncludedAssets::garland_christmas_png));
            if (!garlandSprite) {
                INFO("[Garland] Transparent PNG decoding failed");
                return;
            }

            auto* root = UnityEngine::GameObject::New_ctor("ChristmasGarland");
            if (!root || !root->transform) return;
            root->transform->SetParent(anchorParent);
            root->transform->position = anchorPosition;

            auto* canvas = BSML::Lite::CreateCanvas();
            if (!canvas || !canvas->transform) {
                UnityEngine::GameObject::Destroy(root);
                return;
            }
            canvas->name = "ChristmasGarlandCanvas";
            canvas->transform->parent = root->transform;
            // Slightly behind the letter sprites, never parented to the E layer.
            canvas->transform->localPosition = UnityEngine::Vector3(0.0f, 0.0f, -0.018f);

            // Positioned along the narrow gap between BEAT and SABER.
            auto* image = BSML::Lite::CreateImage(
                canvas, garlandSprite, UnityEngine::Vector2(33.0f, -63.0f),
                UnityEngine::Vector2(475.0f, 154.0f));
            if (!image) {
                UnityEngine::GameObject::Destroy(root);
                return;
            }
            image->gameObject->name = "ChristmasGarlandImage";

            // The pine and bow remain completely still. Eight separate light
            // glints pulse independently with short, gentle intensity peaks.
            auto* sparkleSprite = BSML::Utilities::LoadSpriteRaw(
                static_cast<ArrayW<uint8_t>>(IncludedAssets::garland_sparkle_png));
            if (sparkleSprite) {
                for (int i = 0; i < 8; ++i) {
                    auto* glow = BSML::Lite::CreateImage(
                        canvas, sparkleSprite,
                        UnityEngine::Vector2(33.0f + sparkleX[i], -63.0f + sparkleY[i]),
                        UnityEngine::Vector2(24.0f, 24.0f));
                    if (!glow) continue;
                    glow->gameObject->name = "ChristmasGarlandSparkle";
                    glow->set_color(UnityEngine::Color(1.0f, 0.93f, 0.70f, 0.10f));
                    sparkleImages[i] = glow;
                }
                auto* controller = root->AddComponent<ChristmasEve::GarlandSparkleController*>();
                INFO("[Garland] Sparkle controller attached: {}", controller != nullptr);
                if (controller && !controller->get_enabled()) controller->set_enabled(true);
                INFO("[Garland] Eight independent low-cost light glints installed");
            } else {
                INFO("[Garland] Sparkle overlay unavailable; base garland remains visible");
            }

            christmasGarland = root;
            INFO("[Garland] Active canvas sprite installed at logo gap; Quest placement unverified");
            if (!root->get_activeInHierarchy()) INFO("[Garland] WARNING: root inactive");
            if (!canvas->gameObject->get_activeInHierarchy()) INFO("[Garland] WARNING: canvas inactive");
            if (!image->gameObject->get_activeInHierarchy()) INFO("[Garland] WARNING: image inactive");
        }
    }

    void ShowDecor() {
        // Capture while EFlickering is active; the E replacement hides it next.
        auto garlandAnchor = UnityEngine::GameObject::Find("EFlickering");
        auto garlandParent = garlandAnchor && garlandAnchor->transform
            ? garlandAnchor->transform->parent : nullptr;
        auto garlandPosition = garlandAnchor && garlandAnchor->transform
            ? garlandAnchor->transform->position : UnityEngine::Vector3(0.0f, 0.0f, 0.0f);

        ReplaceFlickeringE();
        InstallChristmasGarland(garlandParent, garlandPosition);
        InstallChristmasSideDecorations(garlandParent, garlandPosition);
    }
    void HideDecor() {
        if (christmasGarland) UnityEngine::GameObject::Destroy(christmasGarland.ptr());
        if (christmasSideDecorations) UnityEngine::GameObject::Destroy(christmasSideDecorations.ptr());
        if (solidE) UnityEngine::GameObject::Destroy(solidE.ptr());
        christmasGarland = nullptr;
        christmasSideDecorations = nullptr;
        solidE = nullptr;
        sparkleImages.fill(nullptr);
        if (originalFlicker) originalFlicker->SetActive(true);
        if (originalNeon) originalNeon->SetActive(true);
        originalFlicker = nullptr;
        originalNeon = nullptr;
    }
}

namespace ChristmasEve {
    void UpdateGarlandSparkles() {
        static bool firstUpdateLogged = false;
        if (!firstUpdateLogged) {
            INFO("[Garland] Sparkle Update running on Quest");
            firstUpdateLogged = true;
        }
        const float now = UnityEngine::Time::get_time();
        for (int i = 0; i < 8; ++i) {
            auto* image = sparkleImages[i];
            if (!image) continue;
            // Different rates and offsets avoid synchronized flashing.
            // Different uneven periods and a second, slower phase keep the
            // original lively irregular twinkle without hard flashing.
            const float rate = 1.10f + 0.23f * static_cast<float>((i * 5 + 3) % 7);
            const float phase = 0.79f * static_cast<float>(i * i) + 2.13f * static_cast<float>(i);
            float peak = 0.82f * std::sin(now * rate + phase)
                       + 0.18f * std::sin(now * (0.37f + 0.06f * i) + phase * 0.31f);
            if (peak < 0.0f) peak = 0.0f;
            peak *= peak;
            peak *= peak;
            image->set_color(UnityEngine::Color(
                1.0f, 0.93f, 0.70f, 0.08f + 0.78f * peak));
            // Tiny independent drifting glints live over the greenery, not
            // at the baked-in gold bulbs. Keep movement below a few UI units.
            auto rect = image->get_rectTransform();
            if (rect) {
                const float driftX = 3.0f * std::sin(now * (0.23f + 0.05f * i) + phase);
                const float driftY = 2.5f * std::cos(now * (0.29f + 0.04f * i) + phase * 0.7f);
                rect->set_anchoredPosition(UnityEngine::Vector2(
                    33.0f + sparkleX[i] + driftX,
                    -63.0f + sparkleY[i] + driftY));
            }
        }
    }
}

void ChristmasEve::GarlandSparkleController::Update() {
    ChristmasEve::UpdateGarlandSparkles();
}
