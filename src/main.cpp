#include "beatsaber-hook/shared/safeptr.hpp"
// Development-stage lighting-only prototype for Beat Saber 1.40.8_7379.
// Seasonal audio and artwork are incorporated at build time; see THIRD_PARTY_NOTICES.md.
#include "GlobalNamespace/MainMenuViewController.hpp"
#include "AutumnLeaves/autumnLeaves.hpp"
#include "Audio/ghostDriftController.hpp"
#include "Audio/ghostSting.hpp"
#include "Audio/seasonalAudio.hpp"
#include "ChristmasEve/presentation.hpp"
#include "ChristmasEve/snow.hpp"
#include "GlobalNamespace/ResultsViewController.hpp"
#include "GlobalNamespace/LevelCompletionResults.hpp"
#include "GlobalNamespace/FireworkItemController.hpp"
#include "GlobalNamespace/RankModel.hpp"
#include "GlobalNamespace/MediaAsyncLoader.hpp"
#include "UnityEngine/AudioSource.hpp"
#include "UnityEngine/AudioClip.hpp"
#include "System/Threading/Tasks/Tasks.hpp"
#include <filesystem>
#include "NewYears/newYears.hpp"
#include "GlobalNamespace/SongPreviewPlayer.hpp"
#include "GlobalNamespace/LightWithIdManager.hpp"
#include "UnityEngine/Color.hpp"
#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/Resources.hpp"
#include "UnityEngine/ParticleSystem.hpp"
#include "UnityEngine/Transform.hpp"
#include "UnityEngine/Vector3.hpp"
#include "UnityEngine/PrimitiveType.hpp"
#include "UnityEngine/MeshRenderer.hpp"
#include "UnityEngine/Collider.hpp"
#include "UnityEngine/Material.hpp"
#include "UnityEngine/MaterialPropertyBlock.hpp"
#include "UnityEngine/Renderer.hpp"
#include "UnityEngine/Time.hpp"
#include "UnityEngine/MonoBehaviour.hpp"
#include "custom-types/shared/macros.hpp"
#include "custom-types/shared/register.hpp"
#include "UnityEngine/Object.hpp"
#include "UnityEngine/Shader.hpp"
#include "UnityEngine/SpriteRenderer.hpp"
#include "GlobalNamespace/TubeBloomPrePassLight.hpp"
#include "GlobalNamespace/FlickeringNeonSign.hpp"
#include "UnityEngine/SceneManagement/SceneManager.hpp"
#include "UnityEngine/SceneManagement/SceneManagement.hpp"
#include "scotland2/shared/modloader.h"
#include "beatsaber-hook/shared/hooking.hpp"
#include "beatsaber-hook/shared/api.hpp"
#include "paper2_scotland2/shared/logger.hpp"
#include "bsml/shared/BSML.hpp"
#include <fstream>
#include <ctime>
#include <sys/stat.h>
#include <string>
#include <algorithm>
#include <cctype>
#include <vector>

using namespace GlobalNamespace;
using namespace UnityEngine;
static constexpr auto PaperLogger = Paper::ConstLoggerContext("seasonalthemes");
static modloader::ModInfo modInfo{MOD_ID, VERSION, 0};

enum class Theme { Default, Halloween, Thanksgiving, ChristmasEve, NewYears };
static Theme currentTheme = Theme::Halloween;
static bool dynamicMode = false;

namespace {
    safe_ptr<MeshRenderer*, true> menuFloor;
    Color menuFloorBaseline{};
    bool hasMenuFloorBaseline = false;
    bool floorRestorationActive = false;
    bool loggedFloorRestore = false;

    MeshRenderer* FindMenuFloor(int colorProperty) {
        for (auto* renderer : Resources::FindObjectsOfTypeAll<MeshRenderer*>()) {
            if (!renderer || !renderer->get_enabled()) continue;
            auto go = renderer->get_gameObject();
            if (!go || !go->get_activeInHierarchy() || go->get_name() != "BasicMenuGround") continue;
            const auto materials = renderer->get_sharedMaterials();
            if (materials.size() != 1) continue;
            auto material = materials[0];
            if (material && material->get_name() == "MenuFloor" && material->HasProperty(colorProperty))
                return renderer;
        }
        return nullptr;
    }

}

// Unity 6 routes SetPropertyBlock through a thunk too small for an inline hook.
// Correct the menu renderer in LateUpdate after gameplay.
DECLARE_CLASS_CODEGEN(SeasonalThemes, FloorRendererSample, UnityEngine::MonoBehaviour) {
    DECLARE_INSTANCE_FIELD(float, nextLookupAt);
    DECLARE_INSTANCE_FIELD(UnityEngine::MaterialPropertyBlock*, propertyBlock);
    DECLARE_INSTANCE_METHOD(void, LateUpdate);
};
DEFINE_TYPE(SeasonalThemes, FloorRendererSample);

void SeasonalThemes::FloorRendererSample::LateUpdate() {
    if (UnityEngine::SceneManagement::SceneManager::GetActiveScene().name == "GameCore") {
        if (!floorRestorationActive) loggedFloorRestore = false;
        floorRestorationActive = true;
        return;
    }
    if (hasMenuFloorBaseline && (!floorRestorationActive || currentTheme == Theme::Default)) return;

    static const int colorProperty = Shader::PropertyToID("_Color");
    if (!menuFloor) {
        const float now = Time::get_realtimeSinceStartup();
        if (now < nextLookupAt) return;
        nextLookupAt = now + 0.5f;
        menuFloor = FindMenuFloor(colorProperty);
        if (!menuFloor) return;
    }

    auto* renderer = menuFloor.ptr();
    auto go = renderer->get_gameObject();
    if (!go || !go->get_activeInHierarchy() || !renderer->get_enabled()) return;
    renderer->GetPropertyBlock(propertyBlock);
    const auto current = propertyBlock->GetColor(colorProperty);

    if (!hasMenuFloorBaseline) {
        if (floorRestorationActive && currentTheme == Theme::Default) return;
        // An unset override is reported as zero by the Quest binding.
        const bool unset = current.r == 0 && current.g == 0 && current.b == 0 && current.a == 0;
        const bool tinted = current.r != current.g || current.g != current.b;
        menuFloorBaseline = (unset || tinted) ? renderer->get_sharedMaterials()[0]->GetColor(colorProperty) : current;
        hasMenuFloorBaseline = true;
        PaperLogger.info("[MenuFloorRestoration] baselineRGBA=({},{},{},{})",
             menuFloorBaseline.r, menuFloorBaseline.g, menuFloorBaseline.b, menuFloorBaseline.a);
        return;
    }

    if (current.r == menuFloorBaseline.r && current.g == menuFloorBaseline.g &&
        current.b == menuFloorBaseline.b && current.a == menuFloorBaseline.a) return;
    // Preserve the other properties written by the menu lighting system.
    propertyBlock->SetColor(colorProperty, menuFloorBaseline);
    renderer->SetPropertyBlock(propertyBlock);
    if (!loggedFloorRestore) {
        loggedFloorRestore = true;
        PaperLogger.info("[MenuFloorRestoration] LateUpdate corrected BasicMenuGround from RGBA=({},{},{},{})",
             current.r, current.g, current.b, current.a);
    }
}

namespace {
    void StartMenuFloorRestoration() {
        auto* probe = GameObject::New_ctor("ChristmasMenuFloorRestoration");
        auto* sampler = probe->AddComponent<SeasonalThemes::FloorRendererSample*>();
        if (!sampler) {
            GameObject::Destroy(probe);
            PaperLogger.info("[MenuFloorRestoration] Failed to create floor controller");
            return;
        }
        sampler->nextLookupAt = Time::get_realtimeSinceStartup() + 0.5f;
        sampler->propertyBlock = MaterialPropertyBlock::New_ctor();
        GameObject::DontDestroyOnLoad(probe);
    }
}


bool SeasonalAudio::IsThanksgiving() { return currentTheme == Theme::Thanksgiving; }
bool SeasonalAudio::IsHalloween() { return currentTheme == Theme::Halloween; }
bool SeasonalAudio::IsChristmasEve() { return currentTheme == Theme::ChristmasEve; }
bool SeasonalAudio::IsNewYears() { return currentTheme == Theme::NewYears; }
struct LightRecord {
    LightWithIdManager* manager;
    int id;
    Color original;
};
static std::vector<LightRecord> menuLights;
static std::string recordedScene;
static void RefreshLights();
static Color ApplyTheme(Color original, int lightId) {
    if (currentTheme == Theme::Default) return original;
    if (currentTheme == Theme::ChristmasEve) return Color(1.0f, 1.0f, 1.0f, original.a);
    if (currentTheme == Theme::NewYears) {
        // New Year's menu lighting uses a neutral base; decorations and audio are managed separately.
        constexpr float blend = 0.52f;
        const Color navy(0.035f, 0.095f, 0.31f, original.a);
        return Color(original.r * (1.f - blend) + navy.r * blend,
                     original.g * (1.f - blend) + navy.g * blend,
                     original.b * (1.f - blend) + navy.b * blend, original.a);
    }
    const bool even = (lightId % 2 == 0);
    const Color tint = currentTheme == Theme::Halloween
        ? (even ? Color(1.0f, 0.22f, 0.025f, original.a) : Color(0.52f, 0.08f, 0.95f, original.a))
        : (even ? Color(1.0f, 0.34f, 0.055f, original.a) : Color(1.0f, 0.69f, 0.16f, original.a));
    constexpr float blend = 0.70f;
    return Color(original.r * (1.f - blend) + tint.r * blend,
                 original.g * (1.f - blend) + tint.g * blend,
                 original.b * (1.f - blend) + tint.b * blend, original.a);
}
static constexpr auto configPath =
    "/sdcard/ModData/com.beatgames.beatsaber/Mods/SeasonalThemes/theme.txt";



namespace Halloween::pumpkinAccents { void CreatePumpkinAccents(); void DestroyPumpkinAccents(); }
namespace Thanksgiving::pumpkinAccents { void CreatePumpkinAccents(); void DestroyPumpkinAccents(); }

static Theme lastDecorTheme = Theme::Default;
static bool hasAppliedDecor = false;
static bool newYearsConfettiNeedsPreviewRestore = false;
static bool newYearsBallNeedsPreviewRestore = false;
static void RestoreNewYearsPreviewDecor() {
    if (currentTheme != Theme::NewYears ||
        UnityEngine::SceneManagement::SceneManager::GetActiveScene().name == "GameCore") return;
    if (newYearsConfettiNeedsPreviewRestore) {
        NewYears::confetti::CreateConfetti();
        newYearsConfettiNeedsPreviewRestore = false;
    }
    if (newYearsBallNeedsPreviewRestore) {
        NewYears::ball::Create();
        newYearsBallNeedsPreviewRestore = false;
    }
}
static void SetOriginalDustVisibility() {
    // Keep ambient floating dust alongside confetti for New Years only.
    // Other holiday particle environments retain their established behavior.
    const bool visible = currentTheme == Theme::Default || currentTheme == Theme::NewYears;
    for (auto particle : Resources::FindObjectsOfTypeAll<ParticleSystem*>()) {
        if (particle && particle->name == "DustPS" && particle->gameObject)
            particle->gameObject->active = visible;
    }
}
static void RestoreSelectedSnow() {
    if (UnityEngine::SceneManagement::SceneManager::GetActiveScene().name == "GameCore") return;
    if (currentTheme == Theme::ChristmasEve) {
        ChristmasEve::snowPrimitives::CreateSnowPrimitives();
        ChristmasEve::snowflakes::CreateSnowflakes();
    }
}

namespace {
    void StopCelebration();
    void RestoreResultsFireworks();
}

static void ClearThemeDecor() {
    if (lastDecorTheme != currentTheme) {
        RestoreResultsFireworks();
        Seasonal::autumnLeaves::DestroyAutumnLeaves();
        if (lastDecorTheme == Theme::Halloween) Halloween::ghostDrift::EndResultsScreen();
    }
    if (lastDecorTheme == Theme::ChristmasEve) {
        ChristmasEve::aurora::EndResults(true);
        ChristmasEve::HideDecor();
        ChristmasEve::snowflakes::DestroySnowflakes();
        ChristmasEve::snowPrimitives::DestroySnowPrimitives();
    }
    if (lastDecorTheme == Theme::NewYears) {
        StopCelebration();
        NewYears::confetti::DestroyConfetti();
        newYearsConfettiNeedsPreviewRestore = false;
        newYearsBallNeedsPreviewRestore = false;
        NewYears::fireworks::Destroy();
        NewYears::ball::Destroy();
    }
    Halloween::pumpkinAccents::DestroyPumpkinAccents();
    Thanksgiving::pumpkinAccents::DestroyPumpkinAccents();
}
static void RefreshLogoColors();

static void RefreshThemeDecor() {
    if (hasAppliedDecor && lastDecorTheme == Theme::ChristmasEve && currentTheme == Theme::ChristmasEve) {
        RestoreSelectedSnow();
        ChristmasEve::ShowDecor();
        ChristmasEve::aurora::PreviewMenu();
        SetOriginalDustVisibility();
        return;
    }
    if (hasAppliedDecor && lastDecorTheme == Theme::NewYears && currentTheme == Theme::NewYears) {
        // Preserve active particles on menu reactivation instead of rebuilding.
        NewYears::ball::Create();
        NewYears::confetti::CreateConfetti();
        NewYears::fireworks::Create();
        SetOriginalDustVisibility();
        return;
    }
    ClearThemeDecor();
    if (currentTheme == Theme::Halloween) Halloween::pumpkinAccents::CreatePumpkinAccents();
    else if (currentTheme == Theme::Thanksgiving) Thanksgiving::pumpkinAccents::CreatePumpkinAccents();
    else if (currentTheme == Theme::ChristmasEve) {
        RestoreSelectedSnow();
        RefreshLogoColors();
        ChristmasEve::ShowDecor();
        ChristmasEve::aurora::PreviewMenu();
    }
    if (currentTheme == Theme::NewYears) {
        NewYears::ball::Create();
        NewYears::confetti::CreateConfetti();
        NewYears::fireworks::Create();
    }
    lastDecorTheme = currentTheme;
    hasAppliedDecor = true;
    SetOriginalDustVisibility();
}


struct LogoSnapshot {
    bool captured = false;
    Color beat;
    Color saber;
    Color sprite;
    Color flickerSprite;
    Color flickerLight;
};
static LogoSnapshot logoSnapshot;

static void RefreshLogoColors() {
    auto beatObj = GameObject::Find("BNeon");
    auto saberObj = GameObject::Find("SaberNeon");
    auto spriteObj = GameObject::Find("SaberLogo");
    auto flickerObj = GameObject::Find("EFlickering");
    auto* beat = beatObj ? beatObj->GetComponent<TubeBloomPrePassLight*>() : nullptr;
    auto* saber = saberObj ? saberObj->GetComponent<TubeBloomPrePassLight*>() : nullptr;
    auto* sprite = spriteObj ? spriteObj->GetComponent<SpriteRenderer*>() : nullptr;
    auto* flicker = flickerObj ? flickerObj->GetComponent<FlickeringNeonSign*>() : nullptr;
    if (!logoSnapshot.captured && beat && saber && sprite && flicker) {
        logoSnapshot.beat = beat->get_color();
        logoSnapshot.saber = saber->get_color();
        logoSnapshot.sprite = sprite->get_color();
        logoSnapshot.flickerSprite = flicker->__cordl_internal_get__spriteOnColor();
        logoSnapshot.flickerLight = flicker->__cordl_internal_get__lightOnColor();
        logoSnapshot.captured = true;
    }
    if (!logoSnapshot.captured) return;
    Color beatColor = logoSnapshot.beat;
    Color saberColor = logoSnapshot.saber;
    Color spriteColor = logoSnapshot.sprite;
    Color eSpriteColor = logoSnapshot.flickerSprite;
    Color eLightColor = logoSnapshot.flickerLight;
    if (currentTheme != Theme::Default && currentTheme != Theme::NewYears) {
        beatColor = currentTheme == Theme::Halloween
            ? Color(1.0f, 0.30f, 0.02f, 1.0f)
            : Color(1.0f, 0.24f, 0.035f, 1.0f);
        saberColor = currentTheme == Theme::Halloween
            ? Color(0.65f, 0.08f, 1.0f, 1.0f)
            : Color(1.0f, 0.68f, 0.12f, 1.0f);
        spriteColor = saberColor;
        eSpriteColor = beatColor;
        eLightColor = beatColor;
    }
    if (currentTheme == Theme::ChristmasEve) {
        beatColor = Color(227.f / 255.f, 38.f / 255.f, 54.f / 255.f, 1.f);
        saberColor = Color(22.f / 255.f, 184.f / 255.f, 78.f / 255.f, 1.f);
        spriteColor = saberColor;
        eSpriteColor = beatColor;
        eLightColor = beatColor;
    }
    for (const char* name : {"BNeon", "ENeon", "ANeon", "TNeon"}) {
        auto obj = GameObject::Find(name);
        if (!obj) continue;
        auto* neon = obj->GetComponent<TubeBloomPrePassLight*>();
        if (neon) neon->set_color(beatColor);
    }
    if (saber) saber->set_color(saberColor);
    if (sprite) sprite->set_color(spriteColor);
    if (flicker) {
        flicker->__cordl_internal_set__spriteOnColor(eSpriteColor);
        flicker->__cordl_internal_set__lightOnColor(eLightColor);
    }
}

static void RefreshAutumnLeaves() {
    if (currentTheme == Theme::Default || currentTheme == Theme::ChristmasEve || currentTheme == Theme::NewYears)
        Seasonal::autumnLeaves::DestroyAutumnLeaves();
    else
        Seasonal::autumnLeaves::CreateAutumnLeaves();
}

static Theme CalendarTheme() {
    std::time_t now = std::time(nullptr);
    std::tm local{};
    if (now == static_cast<std::time_t>(-1) || !localtime_r(&now, &local))
        return currentTheme;
    const int month = local.tm_mon + 1;
    if (month == 10) return Theme::Halloween;
    if (month == 11) return Theme::Thanksgiving;
    if (month == 12) return local.tm_mday == 31 ? Theme::NewYears : Theme::ChristmasEve;
    if (month == 1) return Theme::NewYears;
    return Theme::Default;
}

static void ApplySelectedTheme(Theme theme) {
    if (theme == currentTheme) return;
    currentTheme = theme;
    RefreshLights();
    RefreshThemeDecor();
    RefreshAutumnLeaves();
    if (currentTheme != Theme::ChristmasEve) RefreshLogoColors();
    SeasonalAudio::RefreshTheme();
}

static void SaveThemeSelection(const char* value) {
    mkdir("/sdcard/ModData/com.beatgames.beatsaber/Mods/SeasonalThemes", 0775);
    std::ofstream out(configPath, std::ios::trunc);
    if (out.good()) out << value << "\n";
}

static void ChooseTheme(Theme theme) {
    dynamicMode = false;
    ApplySelectedTheme(theme);
    SaveThemeSelection(theme == Theme::Halloween ? "halloween" :
        theme == Theme::Thanksgiving ? "thanksgiving" :
        theme == Theme::ChristmasEve ? "christmaseve" :
        theme == Theme::NewYears ? "newyears" : "default");
}

static void ChooseDynamic() {
    dynamicMode = true;
    SaveThemeSelection("dynamic");
    ApplySelectedTheme(CalendarTheme());
}

// Called by the already persistent menu ticker; does not touch manual selections.
void SeasonalCalendarTick() {
    if (!dynamicMode) return;
    static std::time_t lastCheck = 0;
    const auto now = std::time(nullptr);
    if (now == static_cast<std::time_t>(-1) || (lastCheck && now >= lastCheck && now - lastCheck < 30))
        return;
    lastCheck = now;
    const auto scene = UnityEngine::SceneManagement::SceneManager::GetActiveScene();
    if (const_cast<UnityEngine::SceneManagement::Scene&>(scene).get_name() == "GameCore") return;
    ApplySelectedTheme(CalendarTheme());
}

static void SeasonalSettings(HMUI::ViewController* self, bool firstActivation,
                             bool addedToHierarchy, bool screenSystemEnabling) {
    if (!firstActivation) return;
    auto* container = BSML::Lite::CreateScrollableSettingsContainer(self->get_transform());
    BSML::Lite::CreateUIButton(container->get_transform(), "Default 🎮",
        [] { ChooseTheme(Theme::Default); });
    BSML::Lite::CreateUIButton(container->get_transform(), "Halloween 🎃",
        [] { ChooseTheme(Theme::Halloween); });
    BSML::Lite::CreateUIButton(container->get_transform(), "Thanksgiving 🦃",
        [] { ChooseTheme(Theme::Thanksgiving); });
    BSML::Lite::CreateUIButton(container->get_transform(), "Christmas Eve ❄️",
        [] { ChooseTheme(Theme::ChristmasEve); });
    BSML::Lite::CreateUIButton(container->get_transform(), "New Years 🥂",
        [] { ChooseTheme(Theme::NewYears); });
    BSML::Lite::CreateUIButton(container->get_transform(), "Dynamic 📅",
        [] { ChooseDynamic(); });
}

static void LoadTheme() {
    std::ifstream file(configPath);
    if (!file.good()) return; // Preserve the existing first-install default.
    std::string value;
    std::getline(file, value);
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (value == "dynamic") { dynamicMode = true; currentTheme = CalendarTheme(); }
    else if (value == "default") { dynamicMode = false; currentTheme = Theme::Default; }
    else if (value == "halloween") { dynamicMode = false; currentTheme = Theme::Halloween; }
    else if (value == "thanksgiving") { dynamicMode = false; currentTheme = Theme::Thanksgiving; }
    else if (value == "christmas" || value == "christmaseve") { dynamicMode = false; currentTheme = Theme::ChristmasEve; }
    else if (value == "newyears") { dynamicMode = false; currentTheme = Theme::NewYears; }
}

MAKE_HOOK_MATCH(SeasonalMenuDidActivate, &MainMenuViewController::DidActivate,
    void, MainMenuViewController* self, bool firstActivation,
    bool addedToHierarchy, bool screenSystemEnabling) {
    SeasonalMenuDidActivate(self, firstActivation, addedToHierarchy, screenSystemEnabling);
    if (firstActivation) StartMenuFloorRestoration();
    SeasonalCalendarTick();
    RefreshThemeDecor();
    RefreshAutumnLeaves();
    if (currentTheme != Theme::ChristmasEve) RefreshLogoColors();
    SeasonalAudio::EnterMenu(self);
    // Theme audio is applied only when explicitly changing themes, not every menu activation.
}

MAKE_HOOK_MATCH(SeasonalSongPlayerEnable, &SongPreviewPlayer::OnEnable,
    void, SongPreviewPlayer* self) {
    SeasonalSongPlayerEnable(self);
    SeasonalAudio::CaptureMenuPlayer(self);
    RestoreSelectedSnow();
    RestoreNewYearsPreviewDecor();
}

// Observe Beat Saber's normal preview crossfades without creating new audio
// sources or modifying the gameplay-return lifecycle.
MAKE_HOOK_MATCH(SeasonalPreviewCrossfade,
    static_cast<void (SongPreviewPlayer::*)(UnityEngine::AudioClip*, float, float, float, bool, System::Action*)>(&SongPreviewPlayer::CrossfadeTo),
    void, SongPreviewPlayer* self, UnityEngine::AudioClip* clip,
    float musicVolume, float startTime, float duration, bool isDefault,
    System::Action* callback) {
    const float adjustedStart = SeasonalAudio::AdjustPreviewStartTime(self, clip, startTime, isDefault);
    SeasonalPreviewCrossfade(self, clip, musicVolume, adjustedStart, duration, isDefault, callback);
    RestoreSelectedSnow();
    RestoreNewYearsPreviewDecor();
}

MAKE_HOOK_MATCH(SeasonalMenuLighting, &LightWithIdManager::SetColorForId,
    void, LightWithIdManager* self, int lightId, Color color) {
    auto scene = UnityEngine::SceneManagement::SceneManager::GetActiveScene();
    const std::string sceneName = static_cast<std::string>(scene.name);
    if (sceneName == "GameCore") {
        if (hasAppliedDecor && lastDecorTheme == Theme::NewYears) {
            NewYears::confetti::DestroyConfetti();
            newYearsConfettiNeedsPreviewRestore = true;
            NewYears::fireworks::Destroy();
            NewYears::ball::Destroy();
            newYearsBallNeedsPreviewRestore = true;
            hasAppliedDecor = false;
        }
        menuLights.clear();
        recordedScene.clear();
        SeasonalMenuLighting(self, lightId, color);
        return;
    }
    if (recordedScene != sceneName) {
        menuLights.clear();
        recordedScene = sceneName;
    }
    // Keep the original input, never a color that has already been themed.
    auto found = std::find_if(menuLights.begin(), menuLights.end(),
        [self, lightId](const LightRecord& r) {
            return r.manager == self && r.id == lightId;
        });
    if (found != menuLights.end()) {
        found->original = color;
    } else if (menuLights.size() < 256) {
        menuLights.push_back({self, lightId, color});
    }
    SeasonalMenuLighting(self, lightId, ApplyTheme(color, lightId));
}

static void RefreshLights() {
    auto scene = UnityEngine::SceneManagement::SceneManager::GetActiveScene();
    const std::string sceneName = static_cast<std::string>(scene.name);
    if (sceneName == "GameCore" || sceneName != recordedScene) {
        menuLights.clear();
        recordedScene.clear();
        return;
    }
    // Invoke the original trampoline to avoid treating our own themed
    // output as a new source color in the hook.
    for (const auto& light : menuLights) {
        if (light.manager)
            hook_SeasonalMenuLighting::SeasonalMenuLighting(light.manager, light.id, ApplyTheme(light.original, light.id));
    }
}

DECLARE_CLASS_CODEGEN(SeasonalThemes, NewYearCelebrationAudio, UnityEngine::MonoBehaviour) {
    DECLARE_INSTANCE_METHOD(void, Update);
};
DEFINE_TYPE(SeasonalThemes, NewYearCelebrationAudio);

namespace {
    struct SuppressedFirework {
        safe_ptr<ParticleSystem*, true> particles;
        bool emissionEnabled;
    };
    std::vector<SuppressedFirework> suppressedFireworks;
    bool replaceResultsFireworks = false;
    struct SuppressedFireworkLight {
        safe_ptr<FireworkItemController*, true> controller;
        float intensity;
    };
    std::vector<SuppressedFireworkLight> suppressedFireworkLights;

    void SuppressFirework(FireworkItemController* controller) {
        if (!controller) return;
        auto object = controller->get_gameObject();
        if (!object || !object->get_activeInHierarchy()) return;
        if (std::none_of(suppressedFireworkLights.begin(), suppressedFireworkLights.end(),
                         [controller](const auto& saved) {
            return saved.controller && saved.controller.ptr() == controller;
        })) {
            suppressedFireworkLights.push_back({controller, controller->__cordl_internal_get__directionalLightIntensity()});
        }
        controller->set_directionalLightIntensity(0.0f);
        controller->SetLightsColor(0.0f);
        auto systems = controller->__cordl_internal_get__particleSystems();
        if (!systems) return;
        for (auto* entry : systems) {
            if (!entry || !entry->_particleSystem) continue;
            auto* particles = entry->_particleSystem.ptr();
            auto emission = particles->emission;
            if (std::none_of(suppressedFireworks.begin(), suppressedFireworks.end(), [particles](const auto& saved) {
                return saved.particles && saved.particles.ptr() == particles;
            })) {
                suppressedFireworks.push_back({particles, emission.get_enabled()});
            }
            emission.set_enabled(false);
            particles->Clear(false);
        }
    }

    bool IsCelebrationResult(LevelCompletionResults* result) {
        return result && result->__cordl_internal_get_levelEndStateType() == LevelCompletionResultsBase::LevelEndStateType::Cleared &&
               result->__cordl_internal_get_rank().value__ >= RankModel::Rank::S.value__;
    }

    void PrepareResultsFireworks(LevelCompletionResults* result) {
        if (replaceResultsFireworks || !IsCelebrationResult(result) ||
            (currentTheme != Theme::Halloween && currentTheme != Theme::ChristmasEve)) return;
        replaceResultsFireworks = true;
        for (auto* controller : Resources::FindObjectsOfTypeAll<FireworkItemController*>()) {
            SuppressFirework(controller);
        }
    }

    void RestoreResultsFireworks() {
        replaceResultsFireworks = false;
        for (auto& saved : suppressedFireworkLights) {
            if (saved.controller) saved.controller->set_directionalLightIntensity(saved.intensity);
        }
        suppressedFireworkLights.clear();
        for (auto& saved : suppressedFireworks) {
            if (saved.particles) saved.particles->emission.set_enabled(saved.emissionEnabled);
        }
        suppressedFireworks.clear();
    }

    using CelebrationTask = decltype(std::declval<MediaAsyncLoader*>()
        ->LoadAudioClipFromFilePathAsync(std::string{}));
    safe_ptr<CelebrationTask, false> celebrationTask;
    safe_ptr<GameObject*, true> celebrationHost;
    safe_ptr<AudioClip*, true> celebrationClip;
    bool celebrationActive = false;
    bool celebrationPlayed = false;
    constexpr const char* celebrationPath = "/sdcard/ModData/com.beatgames.beatsaber/Mods/SeasonalThemes/new_year_celebration.ogg";
    void StopCelebration() {
        celebrationActive = false;
        celebrationPlayed = false;
        celebrationTask = nullptr;
        if (celebrationHost) {
            auto* source = celebrationHost->GetComponent<AudioSource*>();
            if (source) source->Stop();
            Object::Destroy(celebrationHost.ptr());
            celebrationHost = nullptr;
        }
    }
    void PlayCelebration() {
        if (!celebrationActive || celebrationPlayed || !celebrationHost || !celebrationClip) return;
        auto* source = celebrationHost->GetComponent<AudioSource*>();
        if (!source) return;
        celebrationPlayed = true;
        source->set_playOnAwake(false);
        source->set_loop(false);
        source->set_spatialBlend(0.0f);
        source->set_volume(0.75f);
        source->set_clip(celebrationClip.ptr());
        source->Play();
    }
    void StartCelebration() {
        if (celebrationActive) return;
        celebrationActive = true;
        std::error_code ec;
        if (!std::filesystem::exists(celebrationPath, ec)) {
            StopCelebration();
            return;
        }
        celebrationHost = GameObject::New_ctor("NewYearsCelebrationAudio");
        if (!celebrationHost) { StopCelebration(); return; }
        auto* source = celebrationHost->AddComponent<AudioSource*>();
        if (!source) { StopCelebration(); return; }
        source->set_playOnAwake(false);
        if (celebrationClip) { PlayCelebration(); return; }
        auto* loader = MediaAsyncLoader::New_ctor();
        if (!loader || !celebrationHost->AddComponent<SeasonalThemes::NewYearCelebrationAudio*>()) {
            StopCelebration();
            return;
        }
        celebrationTask = loader->LoadAudioClipFromFilePathAsync(std::string(celebrationPath));
        if (!celebrationTask) StopCelebration();
    }
}

void SeasonalThemes::NewYearCelebrationAudio::Update() {
    if (!celebrationHost || get_gameObject().ptr() != celebrationHost.ptr()) return;
    if (!celebrationTask || !celebrationTask->get_IsCompleted()) return;
    if (!celebrationTask->get_IsFaulted() && !celebrationTask->get_IsCanceled()) {
        auto clip = celebrationTask->get_Result();
        if (clip) celebrationClip = clip.ptr();
    }
    celebrationTask = nullptr;
    PlayCelebration();
}

MAKE_HOOK_MATCH(SeasonalFireworkInit, &FireworkItemController::InitializeParticleSystem,
    void, FireworkItemController* self) {
    SeasonalFireworkInit(self);
    if (replaceResultsFireworks) SuppressFirework(self);
}

MAKE_HOOK_MATCH(SeasonalFireworkSound, &FireworkItemController::PlayExplosionSound,
    void, FireworkItemController* self) {
    if (!replaceResultsFireworks) SeasonalFireworkSound(self);
}

MAKE_HOOK_MATCH(SeasonalFireworkLights, &FireworkItemController::SetLightsColor,
    void, FireworkItemController* self, float intensity) {
    SeasonalFireworkLights(self, replaceResultsFireworks ? 0.0f : intensity);
}

MAKE_HOOK_MATCH(SeasonalFireworkFire, &FireworkItemController::Fire,
    void, FireworkItemController* self) {
    if (replaceResultsFireworks) SuppressFirework(self);
    SeasonalFireworkFire(self);
}

MAKE_HOOK_MATCH(SeasonalResultsInit, &ResultsViewController::Init,
    void, ResultsViewController* self, LevelCompletionResults* result, IReadonlyBeatmapData* beatmapData,
    by_ref<BeatmapKey const> key, BeatmapLevel* level, bool practice, bool newHighScore) {
    RestoreResultsFireworks();
    PrepareResultsFireworks(result);
    SeasonalResultsInit(self, result, beatmapData, key, level, practice, newHighScore);
}

MAKE_HOOK_MATCH(SeasonalResultsOpen, &ResultsViewController::DidActivate,
    void, ResultsViewController* self, bool first, bool added, bool enabling) {
    auto* result = self ? self->__cordl_internal_get__levelCompletionResults() : nullptr;
    if (added) PrepareResultsFireworks(result);
    SeasonalResultsOpen(self, first, added, enabling);
    if (!added || !IsCelebrationResult(result)) return;
    if (currentTheme == Theme::ChristmasEve) ChristmasEve::aurora::BeginResults();
    if (currentTheme == Theme::NewYears) StartCelebration();
    if (currentTheme == Theme::Halloween) {
        Halloween::ghostDrift::SetResultsScreen(self, true);
        Halloween::ghostSting::EnterResults();
        if (!Halloween::ghostDrift::Begin()) RestoreResultsFireworks();
    }
}

MAKE_HOOK_MATCH(SeasonalResultsClose, &ResultsViewController::DidDeactivate,
    void, ResultsViewController* self, bool removed, bool disabling) {
    if (currentTheme == Theme::ChristmasEve) ChristmasEve::aurora::EndResults();
    StopCelebration();
    Halloween::ghostSting::ExitResults();
    Halloween::ghostDrift::EndResultsScreen();
    SeasonalResultsClose(self, removed, disabling);
    RestoreResultsFireworks();
}

extern "C" __attribute__((visibility("default"))) void setup(CModInfo* info) noexcept { *info = modInfo.to_c(); }

extern "C" __attribute__((visibility("default"))) void late_load() noexcept {
    i2c::functions::initialize();
    custom_types::Register::AutoRegister();
    LoadTheme();
    INSTALL_HOOK(PaperLogger, SeasonalMenuDidActivate);
    INSTALL_HOOK(PaperLogger, SeasonalMenuLighting);
    INSTALL_HOOK(PaperLogger, SeasonalResultsInit);
    INSTALL_HOOK(PaperLogger, SeasonalFireworkFire);
    INSTALL_HOOK(PaperLogger, SeasonalFireworkInit);
    INSTALL_HOOK(PaperLogger, SeasonalFireworkSound);
    INSTALL_HOOK(PaperLogger, SeasonalFireworkLights);
    INSTALL_HOOK(PaperLogger, SeasonalResultsOpen);
    INSTALL_HOOK(PaperLogger, SeasonalResultsClose);
    INSTALL_HOOK(PaperLogger, SeasonalSongPlayerEnable);
    INSTALL_HOOK(PaperLogger, SeasonalPreviewCrossfade);
    BSML::Init();
    BSML::Register::RegisterMainMenuViewControllerMethod(
        "Holiday Theme Switcher", "Holiday Theme Switcher",
        "Seasonal Theme Switcher",
        SeasonalSettings);
}
