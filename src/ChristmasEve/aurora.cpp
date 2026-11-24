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
#include "custom-types/shared/delegate.hpp"
#include "custom-types/shared/macros.hpp"
#include <array>
#include <algorithm>
#include <cmath>
#include <functional>
#include <string>

DECLARE_CLASS_CODEGEN(ChristmasEve, AuroraCurtainController, UnityEngine::MonoBehaviour) {
    DECLARE_INSTANCE_METHOD(void, Update);
};
DEFINE_TYPE(ChristmasEve, AuroraCurtainController);

namespace ChristmasEve::aurora {
    namespace {
        UnityEngine::GameObject* root = nullptr;
        std::array<UnityEngine::UI::Image*, 3> bands{};
        UnityEngine::AudioSource* soundSource = nullptr;
        UnityEngine::AudioClip* soundClip = nullptr;
        bool soundLoading = false;
        bool playPending = false;
        bool anchoredToLogo = false;
        unsigned int appearanceId = 0;
        unsigned int loadedForAppearance = 0;
        bool showing = false;
        bool previewing = false;
        float previewSeconds = 0.0f;
        float phase = 0.0f;
        float opacity = 0.0f;

        constexpr const char* jinglePath =
            "/sdcard/ModData/com.beatgames.beatsaber/Mods/SeasonalThemes/christmas-eve-aurora-jingle.ogg";

        void PlayBellOnce(unsigned int requestedAppearance) {
            if (!showing || requestedAppearance != appearanceId) {
                INFO("[Aurora78] Stale bell request ignored id={} current={}", requestedAppearance, appearanceId);
                return;
            }
            if (soundSource && soundClip && loadedForAppearance == requestedAppearance) {
                soundSource->Stop();
                soundSource->set_loop(false);
                soundSource->set_clip(soundClip);
                soundSource->set_time(0.0f);
                soundSource->Play();
                playPending = false;
                INFO("[Aurora78] Bell PLAY id={} isPlaying={} volume={} loop=false",
                     requestedAppearance, soundSource->get_isPlaying(), soundSource->get_volume());
            } else {
                playPending = true;
                INFO("[Aurora78] Bell awaiting fresh clip id={} clip={} source={} loadedId={}",
                     requestedAppearance, soundClip != nullptr, soundSource != nullptr,
                     loadedForAppearance);
            }
        }

        void LoadBell(unsigned int requestedAppearance) {
            // A scene transition can invalidate audio loaded for the menu.
            // Load a fresh clip for every aurora appearance, including results.
            soundLoading = true;
            auto* loader = GlobalNamespace::MediaAsyncLoader::New_ctor();
            if (!loader) { soundLoading = false; INFO("[Aurora78] ERROR: Media loader unavailable"); return; }
            auto* task = loader->LoadAudioClipFromFilePathAsync(jinglePath);
            if (!task) { soundLoading = false; INFO("[Aurora78] ERROR: Bell load task unavailable"); return; }
            INFO("[Aurora78] Bell load requested for appearance {}", requestedAppearance);
            task->GetAwaiter().OnCompleted(custom_types::MakeDelegate<System::Action*>(
                std::function([task, requestedAppearance]() {
                    if (requestedAppearance != appearanceId) {
                        INFO("[Aurora78] Ignoring old audio load id={} current={}",
                             requestedAppearance, appearanceId);
                        return;
                    }
                    soundLoading = false;
                    if (task->get_IsFaulted() || task->get_IsCanceled()) {
                        playPending = false;
                        INFO("[Aurora78] ERROR: Bell load faulted/cancelled id={}", requestedAppearance);
                        return;
                    }
                    soundClip = task->get_Result();
                    loadedForAppearance = requestedAppearance;
                    INFO("[Aurora78] Bell clip fresh-loaded id={} valid={}",
                         requestedAppearance, soundClip != nullptr);
                    if (playPending && showing) PlayBellOnce(requestedAppearance);
                })
            ));
        }

        bool Create() {
            if (root) return true;
            auto sprite = BSML::Utilities::LoadSpriteRaw(
                static_cast<ArrayW<uint8_t>>(IncludedAssets::christmas_eve_aurora_v77_png));
            if (!sprite) {
                INFO("[Aurora77] ERROR: transparent aurora PNG failed to decode");
                return false;
            }
            root = UnityEngine::GameObject::New_ctor("ChristmasEveAurora77");
            if (!root) return false;
            UnityEngine::Object::DontDestroyOnLoad(root);
            auto* canvas = BSML::Lite::CreateCanvas();
            if (!canvas || !canvas->transform) {
                INFO("[Aurora77] ERROR: BSML aurora canvas unavailable");
                UnityEngine::Object::Destroy(root);
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
                    UnityEngine::Object::Destroy(root);
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
            ++appearanceId;
            previewing = isPreview;
            previewSeconds = isPreview ? 18.0f : 0.0f;
            showing = true;
            phase = 0.0f;
            opacity = 0.0f;
            root->SetActive(true);
            if (isPreview) PositionAboveLogo(); else PositionForResults();
            // Every menu/results appearance requests a distinct, uncached OGG playback.
            soundClip = nullptr;
            loadedForAppearance = 0;
            playPending = true;
            if (soundSource) soundSource->Stop();
            LoadBell(appearanceId);
            PlayBellOnce(appearanceId);
            INFO("[Aurora78] START id={} mode={} rootActive={} bands={} previewSeconds={}",
                appearanceId, isPreview ? "menu" : "results", root->get_activeInHierarchy(),
                bands.size(), previewSeconds);
        }
    }

    void BeginResults() { Start(false); }
    void EndResults(bool immediately) {
        showing = false;
        previewing = false;
        playPending = false;
        if (soundSource) soundSource->Stop();
        if (immediately) {
            opacity = 0.0f;
            if (root) root->SetActive(false);
        }
        INFO("[Aurora78] Results deactivated id={} immediate={}", appearanceId, immediately);
    }
    void PreviewMenu() {
        // Cache the live menu logo pose silently for later results screens.
        // The persistent root is not displayed and no audio is requested here.
        if (anchoredToLogo) return;
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
        const float dt = UnityEngine::Time::get_deltaTime();
        phase += dt;
        if (previewing) {
            if (UnityEngine::SceneManagement::SceneManager::GetActiveScene().name == "GameCore") {
                previewing = false;
                showing = false;
                playPending = false;
                if (soundSource) soundSource->Stop();
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
            playPending = false;
            if (soundSource) soundSource->Stop();
            INFO("[Aurora77] Render root disabled after fade");
        }
    }
}
void ChristmasEve::AuroraCurtainController::Update() { ChristmasEve::aurora::Tick(); }
