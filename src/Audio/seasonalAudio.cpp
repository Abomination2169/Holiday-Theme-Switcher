#include <cmath>
#include "Audio/seasonalAudio.hpp"

#include "paper2_scotland2/shared/logger.hpp"
static constexpr auto PaperLogger = Paper::ConstLoggerContext("seasonalthemes-audio");

#include "custom-types/shared/macros.hpp"
#include "custom-types/shared/delegate.hpp"
#include "System/Threading/Tasks/Tasks.hpp"
#include "GlobalNamespace/MediaAsyncLoader.hpp"
#include "GlobalNamespace/SongPreviewPlayer.hpp"
#include "UnityEngine/AudioClip.hpp"
#include "System/Action.hpp"
#include "UnityEngine/AudioSource.hpp"
#include "UnityEngine/AudioClip.hpp"
#include "UnityEngine/Time.hpp"
#include "GlobalNamespace/MainMenuViewController.hpp"
#include "UnityEngine/MonoBehaviour.hpp"
#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/Object.hpp"
#include "UnityEngine/SceneManagement/SceneManager.hpp"
#include "UnityEngine/SceneManagement/Scene.hpp"
#include <functional>
#include <unistd.h>
#include <mutex>
#include <queue>
#include <cstdlib>
#include <string>
#include <array>
#include <filesystem>
#include <vector>
#include <cctype>
#include <system_error>

DECLARE_CLASS_CODEGEN(SeasonalAudio, GobbleMenuTicker, UnityEngine::MonoBehaviour)
{
    DECLARE_INSTANCE_METHOD(void, Update);
    DECLARE_INSTANCE_METHOD(void, OnDisable);
    DECLARE_INSTANCE_METHOD(void, OnDestroy);
};
DEFINE_TYPE(SeasonalAudio, GobbleMenuTicker);

namespace {
constexpr const char* kPath = "/sdcard/ModData/com.beatgames.beatsaber/Mods/SeasonalThemes/Thanksgiving-Gobble-Quiet-Phrase.ogg";
std::mutex mutex;
std::queue<std::function<void()>> completions;
UnityW<UnityEngine::AudioSource> source;
UnityW<UnityEngine::AudioClip> clip;
UnityW<SeasonalAudio::GobbleMenuTicker> ticker;
UnityW<GlobalNamespace::SongPreviewPlayer> menuPlayer;
UnityW<UnityEngine::AudioClip> originalMenuClip;
UnityW<UnityEngine::AudioClip> halloweenClip;
bool halloweenLoading = false;
std::array<UnityW<UnityEngine::AudioClip>, 3> christmasClips;
int christmasIndex = -1;
bool christmasLoading = false;
// Foundation for future custom-song loads. Increment on each theme transition;
// asynchronous New Years completions must compare their captured generation.
unsigned int newYearsAudioGeneration = 0;
UnityW<UnityEngine::AudioClip> newYearsClip;
bool newYearsLoading = false;
bool newYearsLoadAttempted = false;
size_t newYearsSelectedIndex = 0;
bool newYearsSelectionReady = false;
// Never crossfade a preview player while gameplay is active or while returning
// to the menu before SongPreviewPlayer::OnEnable has run.
bool gameplaySeen = false;
bool previewEnabledAfterGameplay = false;
namespace fs = std::filesystem;
std::vector<std::string> newYearsCachedSongs;
bool newYearsCacheReady = false;
bool newYearsScanStarted = false;
size_t newYearsRootIndex = 0;
fs::recursive_directory_iterator newYearsIterator;
constexpr std::array<const char*, 2> newYearsRoots = {{
    "/sdcard/ModData/com.beatgames.beatsaber/Mods/SongCore/CustomLevels",
    "/sdcard/ModData/com.beatgames.beatsaber/Mods/SongLoader/CustomLevels"
}};
void TickNewYearsDiscovery() {
    if (newYearsCacheReady || !SeasonalAudio::IsNewYears()) return;
    if (!newYearsScanStarted) {
        newYearsScanStarted = true;
        PaperLogger.info("New Years: starting incremental custom song discovery");
    }
    // Limit directory work per frame to avoid blocking menu startup.
    for (int budget = 0; budget < 12; ++budget) {
        if (newYearsIterator == fs::recursive_directory_iterator{}) {
            if (newYearsRootIndex >= newYearsRoots.size()) {
                newYearsCacheReady = true;
                PaperLogger.info("New Years: cached {} custom song audio files", newYearsCachedSongs.size());
                return;
            }
            std::error_code ec;
            newYearsIterator = fs::recursive_directory_iterator(
                newYearsRoots[newYearsRootIndex++],
                fs::directory_options::skip_permission_denied, ec);
            if (ec) { newYearsIterator = {}; continue; }
            if (newYearsIterator == fs::recursive_directory_iterator{}) continue;
        }
        std::error_code ec;
        if (newYearsIterator->is_directory(ec)) {
            const auto dir = newYearsIterator->path();
            std::error_code infoEc;
            if (fs::exists(dir / "Info.dat", infoEc) ||
                fs::exists(dir / "info.dat", infoEc)) {
                std::error_code innerEc;
                for (fs::directory_iterator song(dir, innerEc), end;
                     song != end && !innerEc; song.increment(innerEc)) {
                    if (!song->is_regular_file(innerEc)) continue;
                    auto ext = song->path().extension().string();
                    for (char& c : ext)
                        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                    if (ext == ".ogg" || ext == ".egg")
                        newYearsCachedSongs.push_back(song->path().string());
                }
                newYearsIterator.disable_recursion_pending();
            }
        }
        newYearsIterator.increment(ec);
        if (ec) newYearsIterator = {};
    }
}

void ApplyHalloweenMusic();
void QueueNewYearsLoad() {
    if (!SeasonalAudio::IsNewYears() || !newYearsCacheReady ||
        newYearsLoading || newYearsLoadAttempted || newYearsClip || newYearsCachedSongs.empty()) return;
    if (!newYearsSelectionReady) {
        newYearsSelectedIndex = static_cast<size_t>(std::rand()) % newYearsCachedSongs.size();
        newYearsSelectionReady = true;
    }
    const auto path = newYearsCachedSongs[newYearsSelectedIndex];
    if (access(path.c_str(), R_OK) != 0) {
        PaperLogger.warn("New Years: selected custom song unavailable");
        return;
    }
    newYearsLoading = true;
    newYearsLoadAttempted = true;
    const auto generation = newYearsAudioGeneration;
    auto loader = GlobalNamespace::MediaAsyncLoader::New_ctor();
    auto task = loader->LoadAudioClipFromFilePathAsync(path);
    if (!task) { newYearsLoading = false; return; }
    task->GetAwaiter().OnCompleted(custom_types::MakeDelegate<System::Action*>(
        std::function([task, generation]() {
            std::lock_guard<std::mutex> guard(mutex);
            completions.emplace([task, generation]() {
                // Read the result on the Unity main thread; never apply a stale theme load.
                auto result = task->get_Result();
                newYearsLoading = false;
                if (generation != newYearsAudioGeneration || !SeasonalAudio::IsNewYears()) return;
                if (result) {
                    newYearsClip = result;
                    PaperLogger.info("New Years: custom song loaded");
                    ApplyHalloweenMusic();
                } else PaperLogger.warn("New Years: custom song load returned null");
            });
        })
    ));
}
constexpr std::array<const char*, 3> christmasPaths = {{
    "/sdcard/ModData/com.beatgames.beatsaber/Mods/SeasonalThemes/we_wish_you_a_merry_christmas.ogg",
    "/sdcard/ModData/com.beatgames.beatsaber/Mods/SeasonalThemes/dance_of_the_sugar_plum_fairy.ogg",
    "/sdcard/ModData/com.beatgames.beatsaber/Mods/SeasonalThemes/M668_Christmas_Lights_5m22_click_removed.ogg",
}};
constexpr const char* kHalloweenPath = "/sdcard/ModData/com.beatgames.beatsaber/Mods/SeasonalThemes/Ghostly_Horror_Menu.ogg";
void ApplyHalloweenMusic() {
    if (!menuPlayer || !originalMenuClip) return;
    auto target = originalMenuClip.ptr();
    if (SeasonalAudio::IsHalloween() && halloweenClip) target = halloweenClip.ptr();
    if (SeasonalAudio::IsChristmasEve() && christmasIndex >= 0 && christmasClips[christmasIndex])
        target = christmasClips[christmasIndex].ptr();
    if (SeasonalAudio::IsNewYears() && newYearsClip) target = newYearsClip.ptr();
    menuPlayer->_defaultAudioClip = target;
    const auto scene = UnityEngine::SceneManagement::SceneManager::GetActiveScene();
    if (const_cast<UnityEngine::SceneManagement::Scene&>(scene).get_name() == "GameCore") return;
    PaperLogger.info("Audio diagnostic: applying theme menu clip (christmas={}, newYears={}, gameplaySeen={})", SeasonalAudio::IsChristmasEve(), SeasonalAudio::IsNewYears(), gameplaySeen);
    menuPlayer->CrossfadeTo(target, 1.0f, 0.0f, 1.0f, true, nullptr);
}
void QueueChristmasLoad() {
    if (christmasLoading || !SeasonalAudio::IsChristmasEve()) return;
    if (christmasIndex < 0) christmasIndex = std::rand() % christmasPaths.size();
    if (christmasClips[christmasIndex]) {
        // The cached clip is already available. Do not crossfade during
        // unrelated menu events; explicit theme changes and safe return
        // restoration own playback transitions.
        return;
    }
    auto path = christmasPaths[christmasIndex];
    if (access(path, R_OK) != 0) return;
    christmasLoading = true;
    auto task = GlobalNamespace::MediaAsyncLoader::New_ctor()->LoadAudioClipFromFilePathAsync(std::string(path));
    if (!task) { christmasLoading = false; return; }
    const int index = christmasIndex;
    task->GetAwaiter().OnCompleted(custom_types::MakeDelegate<System::Action*>(
        std::function([task,index]() {
            std::lock_guard<std::mutex> guard(mutex);
            completions.emplace([task,index]() {
                auto result = task->get_Result();
                if (result) christmasClips[index] = result;
                christmasLoading = false;
                if (SeasonalAudio::IsChristmasEve()) {
                    if (christmasIndex == index) ApplyHalloweenMusic();
                    else QueueChristmasLoad();
                }
            });
        })
    ));
}
void QueueHalloweenLoad() {
    if (halloweenLoading || halloweenClip || access(kHalloweenPath, R_OK) != 0) return;
    halloweenLoading = true;
    auto task = GlobalNamespace::MediaAsyncLoader::New_ctor()->LoadAudioClipFromFilePathAsync(std::string(kHalloweenPath));
    if (!task) { halloweenLoading = false; return; }
    task->GetAwaiter().OnCompleted(custom_types::MakeDelegate<System::Action*>(
        std::function([task]() {
            std::lock_guard<std::mutex> guard(mutex);
            completions.emplace([task]() {
                auto result = task->get_Result();
                if (result) halloweenClip = result;
                halloweenLoading = false;
                if (SeasonalAudio::IsHalloween()) ApplyHalloweenMusic();
            });
        })
    ));
}
bool active = false;
bool loading = false;
float nextTime = 0.0f;

void QueueLoad() {
    if (loading || clip) return;
    if (!(access(kPath, R_OK) == 0)) {
        PaperLogger.error("Gobble file missing");
        return;
    }
    loading = true;
    auto loader = GlobalNamespace::MediaAsyncLoader::New_ctor();
    auto task = loader->LoadAudioClipFromFilePathAsync(std::string(kPath));
    if (!task) { loading = false; return; }
    task->GetAwaiter().OnCompleted(custom_types::MakeDelegate<System::Action*>(
        std::function([task]() {
            std::lock_guard<std::mutex> guard(mutex);
            completions.emplace([task]() {
                auto result = task->get_Result();
                if (result) { clip = result; PaperLogger.info("Gobble preloaded"); }
                else PaperLogger.error("Gobble load returned null");
                loading = false;
            });
        })
    ));
}
void Stop() {
    active = false;
    if (source && source->get_isPlaying()) source->Stop();
    nextTime = 0.0f;
}
}

// Capture the outgoing menu soundtrack time for every theme when a song preview begins.
// When Beat Saber returns to that same default clip, reuse its saved position.
// Thanksgiving's separate turkey-gobble AudioSource is not modified.
// No separate audio player, no crossfade from our hook, and no gameplay changes.
namespace {
UnityW<UnityEngine::AudioClip> previewPausedClip;
float previewPausedTime = 0.0f;
bool previewPositionValid = false;
}
float SeasonalAudio::AdjustPreviewStartTime(GlobalNamespace::SongPreviewPlayer* player,
    UnityEngine::AudioClip* clip, float requestedStartTime, bool isDefault) {
    if (!player || !clip) return requestedStartTime;
    const auto scene = UnityEngine::SceneManagement::SceneManager::GetActiveScene();
    if (const_cast<UnityEngine::SceneManagement::Scene&>(scene).get_name() == "GameCore")
        return requestedStartTime;
    auto defaultClip = player->_defaultAudioClip.ptr();
    if (!defaultClip) return requestedStartTime;
    if (clip != defaultClip) {
        // A preview is replacing the holiday default soundtrack.
        const int channel = player->_activeChannel;
        auto controllers = player->_audioSourceControllers;
        if (channel >= 0 && channel < controllers.size()) {
            auto controller = controllers[channel];
            if (controller && controller->audioSource) {
                auto audio = controller->audioSource.ptr();
                if (audio && audio->get_clip().ptr() == defaultClip) {
                    previewPausedTime = audio->get_time();
                    previewPausedClip = defaultClip;
                    previewPositionValid = true;
                    PaperLogger.info("Audio preview: saved holiday playback at {} seconds", previewPausedTime);
                }
            }
        }
        return requestedStartTime;
    }
    if (isDefault && previewPositionValid && previewPausedClip.ptr() == clip) {
        const float length = clip->get_length();
        const float resume = length > 0.1f ? std::fmod(previewPausedTime, length) : requestedStartTime;
        previewPositionValid = false;
        PaperLogger.info("Audio preview: restoring holiday playback at {} seconds", resume);
        return resume;
    }
    return requestedStartTime;
}
void SeasonalAudio::EnterMenu(GlobalNamespace::MainMenuViewController* menu) {
    if (!menu) return;
    if (gameplaySeen && previewEnabledAfterGameplay && menuPlayer) {
        const auto scene = UnityEngine::SceneManagement::SceneManager::GetActiveScene();
        if (const_cast<UnityEngine::SceneManagement::Scene&>(scene).get_name() != "GameCore") {
            gameplaySeen = false;
            previewEnabledAfterGameplay = false;
            PaperLogger.info("Audio diagnostic: restoring menu music after gameplay");
            ApplyHalloweenMusic();
        }
    }

    // The scheduler and audio source must not live on the menu GameObject:
    // Beat Saber can disable that object during gameplay.
    if (!ticker || !ticker->get_gameObject()) {
        auto controller = UnityEngine::GameObject::New_ctor("ThanksgivingGobbleController");
        if (!controller) {
            PaperLogger.error("Gobble controller creation failed");
            return;
        }
        UnityEngine::Object::DontDestroyOnLoad(controller);
        ticker = controller->AddComponent<SeasonalAudio::GobbleMenuTicker*>();
        source = controller->AddComponent<UnityEngine::AudioSource*>();
        if (!ticker || !source) {
            PaperLogger.error("Gobble components creation failed");
            return;
        }
        source->set_playOnAwake(false);
        source->set_loop(false);
        source->set_spatialBlend(0.0f);
        source->set_volume(0.22f);
    }

    // DidActivate is the authoritative rearm event, including returns from songs.
    active = SeasonalAudio::IsThanksgiving();
    nextTime = UnityEngine::Time::get_unscaledTime() + 5.0f;
    if (active) QueueLoad();
}

void SeasonalAudio::EnterSoloSelection() {
    // Controller was initialized by the main menu. Do not create one on a
    // transient level-selection GameObject.
    if (!ticker || !source) return;
    if (!SeasonalAudio::IsThanksgiving()) return;
    // Avoid restarting the timer when moving between menu screens while
    // gobbles are already active. Returning from GameCore sets active=false.
    if (active) return;
    active = true;
    nextTime = UnityEngine::Time::get_unscaledTime() + 5.0f;
    QueueLoad();
}

void SeasonalAudio::ExitMenu() { Stop(); }
void SeasonalAudio::RefreshTheme() {
    previewPositionValid = false;
    previewPausedClip = nullptr;
    ++newYearsAudioGeneration;
    newYearsClip = nullptr;
    newYearsSelectionReady = false;
    newYearsLoadAttempted = false;
    Stop();
    if (IsThanksgiving()) {
        active = true;
        nextTime = UnityEngine::Time::get_unscaledTime() + 5.0f;
        QueueLoad();
    }
    if (IsHalloween()) QueueHalloweenLoad();
    if (IsChristmasEve()) { christmasIndex = std::rand() % christmasPaths.size(); QueueChristmasLoad(); }
    if (IsNewYears()) QueueNewYearsLoad();
    ApplyHalloweenMusic();
}
void SeasonalAudio::CaptureMenuPlayer(GlobalNamespace::SongPreviewPlayer* player) {
    if (!player) return;
    const auto scene = UnityEngine::SceneManagement::SceneManager::GetActiveScene();
    if (const_cast<UnityEngine::SceneManagement::Scene&>(scene).get_name() == "GameCore") return;
    if (gameplaySeen) {
        previewEnabledAfterGameplay = true;
        PaperLogger.info("Audio diagnostic: preview player enabled after gameplay");
    }
    if (!originalMenuClip || menuPlayer.ptr() != player) originalMenuClip = player->_defaultAudioClip;
    menuPlayer = player;
    if (IsHalloween()) QueueHalloweenLoad();
    if (IsChristmasEve()) QueueChristmasLoad();
    if (IsNewYears()) QueueNewYearsLoad();
    if (!gameplaySeen && (halloweenClip || IsChristmasEve() || (IsNewYears() && newYearsClip))) ApplyHalloweenMusic();
}

void SeasonalAudio::Tick() {
    std::queue<std::function<void()>> ready;
    {
        std::lock_guard<std::mutex> guard(mutex);
        std::swap(ready, completions);
    }
    while (!ready.empty()) { ready.front()(); ready.pop(); }
    TickNewYearsDiscovery();
    if (IsNewYears()) QueueNewYearsLoad();

    // Track gameplay for every theme before Thanksgiving-only early returns.
    const auto activeScene = UnityEngine::SceneManagement::SceneManager::GetActiveScene();
    if (const_cast<UnityEngine::SceneManagement::Scene&>(activeScene).get_name() == "GameCore") {
        if (!gameplaySeen) PaperLogger.info("Audio diagnostic: entered GameCore");
        gameplaySeen = true;
        previewEnabledAfterGameplay = false;
    }
    if (!ticker || !source) return;
    if (!SeasonalAudio::IsThanksgiving()) { Stop(); return; }

    // GameCore is never allowed to play turkey audio. The persistent ticker
    // remains alive so asynchronous clip completions can still be drained.
    const auto scene = UnityEngine::SceneManagement::SceneManager::GetActiveScene();
    if (const_cast<UnityEngine::SceneManagement::Scene&>(scene).get_name() == "GameCore") {
        if (active) Stop();
        return;
    }
    if (!active) return;
    if (!clip) { QueueLoad(); return; }

    const float now = UnityEngine::Time::get_unscaledTime();
    if (now < nextTime) return;
    if (!source->get_isPlaying()) {
        source->set_clip(clip.ptr());
        source->Play();
        PaperLogger.info("Gobble played");
    }
    nextTime = now + 15.0f + float(std::rand() % 11);
}

extern void SeasonalCalendarTick();
void SeasonalAudio::GobbleMenuTicker::Update() { SeasonalCalendarTick(); SeasonalAudio::Tick(); }
void SeasonalAudio::GobbleMenuTicker::OnDisable() { SeasonalAudio::ExitMenu(); }
void SeasonalAudio::GobbleMenuTicker::OnDestroy() {
    SeasonalAudio::ExitMenu();
    ticker = nullptr;
    source = nullptr;
}

// Legacy hooks remain disabled. Their entry points are retained only for linking.
void InitializeTurkeyGobbles(GlobalNamespace::SongPreviewPlayer*) {}
void DisableTurkeyGobbles(GlobalNamespace::SongPreviewPlayer*) {}
void UpdateTurkeyGobbles(GlobalNamespace::SongPreviewPlayer*) {}