#include "ChristmasEve/presentation.hpp"
#include "paper2_scotland2/shared/logger.hpp"
#define INFO(message, ...) Paper::Logger::fmtLogTag<Paper::LogLevel::INF>(message, "seasonalthemes" __VA_OPT__(, __VA_ARGS__))
#include "assets.hpp"
#include "bsml/shared/BSML.hpp"
#include "bsml/shared/Helpers/utilities.hpp"
#include "GlobalNamespace/MediaAsyncLoader.hpp"
#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/MonoBehaviour.hpp"
#include "UnityEngine/Transform.hpp"
#include "UnityEngine/UI/Image.hpp"
#include "UnityEngine/RectTransform.hpp"
#include "UnityEngine/AudioSource.hpp"
#include "UnityEngine/AudioClip.hpp"
#include "UnityEngine/Time.hpp"
#include "UnityEngine/Object.hpp"
#include "UnityEngine/SceneManagement/SceneManager.hpp"
#include "System/Threading/Tasks/Tasks.hpp"
#include "beatsaber-hook/shared/safeptr.hpp"
#include "custom-types/shared/macros.hpp"
#include <array>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <utility>
#include <string>

DECLARE_CLASS_CODEGEN(ChristmasEve, AuroraCurtainController, UnityEngine::MonoBehaviour) {
    DECLARE_INSTANCE_METHOD(void, Update);
};
DEFINE_TYPE(ChristmasEve, AuroraCurtainController);

namespace ChristmasEve::aurora {
    namespace {
        using BellTask = decltype(std::declval<GlobalNamespace::MediaAsyncLoader*>()
            ->LoadAudioClipFromFilePathAsync(std::string{}));

        safe_ptr<UnityEngine::GameObject*, true> root;
        std::array<safe_ptr<UnityEngine::UI::Image*, true>, 3> bands{};
        safe_ptr<UnityEngine::AudioSource*, true> soundSource;
        safe_ptr<BellTask, false> bellTask;
        bool anchoredToLogo = false;
        bool showing = false;
        bool previewing = false;
        float previewSeconds = 0.0f;
        float phase = 0.0f;
        float opacity = 0.0f;

        constexpr const char* jinglePath =
            "/sdcard/ModData/com.beatgames.beatsaber/Mods/SeasonalThemes/christmas-eve-aurora-jingle.ogg";

        void LoadBell() {
            std::error_code error;
            if (!soundSource || !std::filesystem::exists(jinglePath, error)) return;
            auto* loader = GlobalNamespace::MediaAsyncLoader::New_ctor();
            if (loader) bellTask = loader->LoadAudioClipFromFilePathAsync(std::string(jinglePath));
        }

        void PlayLoadedBell() {
            if (!bellTask || !bellTask->get_IsCompleted()) return;
            if (showing && soundSource && !bellTask->get_IsFaulted() && !bellTask->get_IsCanceled()) {
                auto clip = bellTask->get_Result();
                if (clip) {
                    soundSource->set_clip(clip);
                    soundSource->Play();
                }
            }
            bellTask = nullptr;
        }

        void StopBell() {
            bellTask = nullptr;
            if (soundSource) soundSource->Stop();
        }

        bool Create() {
            if (root && std::all_of(bands.begin(), bands.end(), [](const auto& band) {
                return static_cast<bool>(band);
            })) return true;
            if (root) UnityEngine::Object::Destroy(root.ptr());
            root = nullptr;
            soundSource = nullptr;
            for (auto& band : bands) band = nullptr;
            anchoredToLogo = false;
            auto sprite = BSML::Utilities::LoadSpriteRaw(
                static_cast<ArrayW<uint8_t>>(IncludedAssets::christmas_eve_aurora_v77_png));
            if (!sprite) {
                INFO("[Aurora77] ERROR: transparent aurora PNG failed to decode");
                return false;
            }
            root = UnityEngine::GameObject::New_ctor("ChristmasEveAurora77");
            if (!root) return false;
            UnityEngine::Object::DontDestroyOnLoad(root.ptr());
            auto* canvas = BSML::Lite::CreateCanvas();
            if (!canvas || !canvas->transform) {
                INFO("[Aurora77] ERROR: BSML aurora canvas unavailable");
                UnityEngine::Object::Destroy(root.ptr());
                root = nullptr;
                return false;
            }
            canvas->name = "ChristmasEveAuroraCanvas";
            canvas->transform->SetParent(root->transform);
            canvas->transform->localPosition = UnityEngine::Vector3(0.0f, 0.0f, -0.018f);

            for (int k = 0; k < 3; ++k) {
                const float x = 50.0f + (k - 1) * 17.0f;
                const float y = 258.0f + k * 5.0f;
                bands[k] = BSML::Lite::CreateImage(canvas, sprite,
                    UnityEngine::Vector2(x, y),
                    UnityEngine::Vector2(1469.0f, 341.0f));
                if (!bands[k]) {
                    INFO("[Aurora77] ERROR: CreateImage failed for band {}", k);
                    UnityEngine::Object::Destroy(root.ptr());
                    root = nullptr;
                    return false;
                }
                bands[k]->gameObject->name = "ChristmasEveAuroraBand";
                bands[k]->set_color(UnityEngine::Color(1, 1, 1, 0));
            }
            soundSource = root->AddComponent<UnityEngine::AudioSource*>();
            if (soundSource) {
                soundSource->set_playOnAwake(false);
                soundSource->set_loop(false);
                soundSource->set_volume(0.95f);
                soundSource->set_spatialBlend(0.0f);
            }
            root->AddComponent<ChristmasEve::AuroraCurtainController*>();
            INFO("[Aurora77] Created 3 BSML image bands; rootActive={} canvasActive={}",
                root->get_activeInHierarchy(), canvas->gameObject->get_activeInHierarchy());
            return true;
        }

        // Use the exact live neon E world anchor rather than assumed terrain
        // coordinates. The same location/size convention displays side art.
        void PositionAboveLogo() {
            auto e = UnityEngine::GameObject::Find("ChristmasSolidE");
            if (!e) e = UnityEngine::GameObject::Find("EFlickering");
            if (!e || !e->transform || !e->transform->parent) {
                INFO("[Aurora77] Logo anchor missing; menu placement deferred");
                return;
            }
            auto parent = e->transform->parent;
            const auto forward = parent->get_forward();
            const auto logoPosition = e->transform->position;
            root->transform->position = UnityEngine::Vector3(
                logoPosition.x + forward.x * 1.6f,
                logoPosition.y + forward.y * 1.6f,
                logoPosition.z + forward.z * 1.6f);
            root->transform->rotation = parent->rotation;
            root->transform->localScale = parent->get_lossyScale();
            anchoredToLogo = true;
            INFO("[Aurora77] Menu aurora aligned above neon logo; parentActive={}",
                parent->gameObject->get_activeInHierarchy());
        }

        // Preserve the logo's WORLD pose. The old Camera.main TransformPoint
        // made the aurora jump to wherever the player happened to look when
        // the results appeared. Never use the HMD to set a results pose.
        void PositionForResults() {
            if (!anchoredToLogo) PositionAboveLogo();
            if (!anchoredToLogo) {
                // Absolute fallback only when this session never had a logo.
                root->transform->position = UnityEngine::Vector3(0.0f, 3.8f, 10.4f);
                root->transform->localScale = UnityEngine::Vector3(1.0f, 1.0f, 1.0f);
                INFO("[Aurora78] WARNING: results using fixed world fallback (no logo pose)");
            } else {
                INFO("[Aurora78] RESULTS pose locked to captured logo; no HMD-relative movement");
            }
            const auto p = root->transform->position;
            INFO("[Aurora79] Results WORLD anchor x={} y={} z={} (width 1469, logo-forward offset 1.6m)", p.x, p.y, p.z);
        }

        void Start(bool isPreview) {
            if (!Create()) {
                INFO("[Aurora77] ERROR: requested aurora, but Create failed");
                return;
            }
            StopBell();
            previewing = isPreview;
            previewSeconds = isPreview ? 18.0f : 0.0f;
            showing = true;
            phase = 0.0f;
            opacity = 0.0f;
            if (isPreview) PositionAboveLogo(); else PositionForResults();
            root->SetActive(true);
            LoadBell();
            INFO("[Aurora] Started {} animation", isPreview ? "menu" : "results");
        }
    }

    void BeginResults() { Start(false); }
    void EndResults(bool immediately) {
        showing = false;
        previewing = false;
        StopBell();
        if (immediately) {
            opacity = 0.0f;
            if (root) root->SetActive(false);
        }
        INFO("[Aurora] Results deactivated immediate={}", immediately);
    }
    void PreviewMenu() {
        // Cache the live menu logo pose silently for later results screens.
        // The persistent root is not displayed and no audio is requested here.
        if (anchoredToLogo && root) return;
        if (!UnityEngine::GameObject::Find("ChristmasSolidE") &&
            !UnityEngine::GameObject::Find("EFlickering")) return;
        if (!Create()) return;
        PositionAboveLogo();
        if (anchoredToLogo) {
            root->SetActive(false);
            INFO("[Aurora79] Captured menu logo anchor; preview and menu bell disabled");
        }
    }
    void Tick() {
        if (!root) return;
        PlayLoadedBell();
        const float dt = UnityEngine::Time::get_deltaTime();
        phase += dt;
        if (previewing) {
            if (UnityEngine::SceneManagement::SceneManager::GetActiveScene().name == "GameCore") {
                previewing = false;
                showing = false;
                StopBell();
                opacity = 0.0f;
                root->SetActive(false);
                INFO("[Aurora77] Preview stopped for gameplay");
                return;
            }
            previewSeconds -= dt;
            if (previewSeconds <= 0) {
                previewing = false;
                showing = false;
                INFO("[Aurora77] Menu preview finished");
            }
        }
        opacity += (showing ? 1.0f - opacity : -opacity) *
                   std::min(1.0f, dt * (showing ? 1.5f : 1.7f));
        for (int k = 0; k < 3; ++k) {
            if (!bands[k]) continue;
            auto rect = bands[k]->get_rectTransform();
            // Build 92: layered, gentle quasi-periodic flow. Multiple slow
            // incommensurate oscillations make each curtain drift, rise and
            // breathe independently without abrupt changes or mesh deformation.
            // Only the image positions and alpha change; the world anchor stays fixed.
            const float layer = static_cast<float>(k);
            const float driftDirection = (k == 1) ? -1.0f : 1.0f;
            const float drift = driftDirection * (
                29.0f * std::sin(phase * (0.113f + layer * 0.019f) + layer * 1.71f) +
                15.0f * std::sin(phase * (0.047f + layer * 0.007f) + layer * 2.37f) +
                 7.0f * std::sin(phase * (0.241f - layer * 0.013f) + layer * 0.83f));
            const float rise =
                20.0f * std::sin(phase * (0.297f + layer * 0.039f) + layer * 2.11f) +
                10.0f * std::sin(phase * (0.173f - layer * 0.011f) + layer * 0.91f) +
                 5.0f * std::sin(phase * (0.061f + layer * 0.008f) + layer * 1.39f);
            // Build 93: soft rolling oscillations sit on top of the existing
            // Build 92 trajectories. Layer-shifted phases let the wave travel
            // from curtain to curtain without introducing geometry or pulsing.
            const float rollingPhase = phase * 0.157f + layer * 1.23f +
                0.42f * std::sin(phase * 0.041f + layer * 0.77f);
            const float verticalRipple =
                 8.0f * std::sin(rollingPhase) +
                 4.5f * std::sin(phase * (0.213f + layer * 0.016f) +
                                 layer * 2.06f + 0.27f * std::sin(phase * 0.053f));
            const float secondarySway =
                 6.5f * std::sin(phase * (0.079f + layer * 0.009f) +
                                  layer * 1.86f + 0.35f * std::sin(phase * 0.031f)) +
                 3.0f * std::sin(phase * (0.183f - layer * 0.012f) + layer * 0.61f);
            if (rect) rect->set_anchoredPosition(UnityEngine::Vector2(
                50.0f + (k - 1) * 17.0f + drift + secondarySway,
                258.0f + layer * 5.0f + rise + verticalRipple));
            // Gentle asynchronous brightness waves preserve original alpha
            // weighting: central band 0.82, outer bands 0.28.
            const float breath = 0.88f +
                0.085f * std::sin(phase * (0.301f + layer * 0.047f) + layer * 2.47f) +
                0.035f * std::sin(phase * (0.119f + layer * 0.018f) + layer * 1.31f);
            bands[k]->set_color(UnityEngine::Color(1, 1, 1,
                opacity * (k == 1 ? 0.82f : 0.28f) * breath));
        }
        if (!showing && opacity < 0.006f) {
            opacity = 0.0f;
            root->SetActive(false);
            StopBell();
            INFO("[Aurora77] Render root disabled after fade");
        }
    }
}
void ChristmasEve::AuroraCurtainController::Update() { ChristmasEve::aurora::Tick(); }
