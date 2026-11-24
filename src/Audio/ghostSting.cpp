#include "Audio/ghostSting.hpp"
#include "paper2_scotland2/shared/logger.hpp"
static constexpr auto PaperLogger = Paper::ConstLoggerContext("seasonalthemes-halloween");
#include <fstream>
#include "GlobalNamespace/MediaAsyncLoader.hpp"
#include "UnityEngine/AudioClip.hpp"
#include "UnityEngine/AudioSource.hpp"
#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/Object.hpp"
#include "System/Threading/Tasks/Tasks.hpp"
#include "custom-types/shared/delegate.hpp"

#include <functional>
#include <string>
#include <array>
#include <cstdlib>
#include <utility>

namespace Halloween::ghostSting {
    namespace {
        bool active = false;
        // Five recordings included with the v1.0.1 public QMOD; details and
        // attribution are in GHOST_AUDIO_CREDITS.txt / PIXABAY_AUDIO_CREDITS.txt.
        const std::array<std::string, 5> voicePaths = {{
            "/sdcard/ModData/com.beatgames.beatsaber/Mods/SeasonalThemes/ghost_ogrebane.ogg",
            "/sdcard/ModData/com.beatgames.beatsaber/Mods/SeasonalThemes/ghost_scary_1.ogg",
            "/sdcard/ModData/com.beatgames.beatsaber/Mods/SeasonalThemes/ghost_breath.ogg",
            "/sdcard/ModData/com.beatgames.beatsaber/Mods/SeasonalThemes/ghost_pixabay_creepy.ogg",
            "/sdcard/ModData/com.beatgames.beatsaber/Mods/SeasonalThemes/ghost_pixabay_wind.ogg"
        }};
        std::array<UnityEngine::AudioClip*, 5> voiceClips = {nullptr, nullptr, nullptr, nullptr, nullptr};
        bool voiceLoadingStarted = false;

        // Shuffle-bag rotation: each loaded recording plays once per cycle
        // before any track repeats. Keep the last choice across results screens
        // to avoid immediately repeating a sound at the cycle boundary.
        std::array<std::size_t, 5> voiceOrder{};
        std::size_t voiceOrderLength = 0;
        std::size_t voiceOrderCursor = 0;
        int lastVoiceIndex = -1;

        int NextVoiceIndex() {
            if (voiceOrderCursor >= voiceOrderLength) {
                voiceOrderLength = 0;
                voiceOrderCursor = 0;
                for (std::size_t i = 0; i < voiceClips.size(); ++i) {
                    if (voiceClips[i]) voiceOrder[voiceOrderLength++] = i;
                }
                if (!voiceOrderLength) return -1;
                // Fisher-Yates shuffle, then avoid a consecutive repeat
                // across cycles if more than one recording is available.
                for (std::size_t i = voiceOrderLength; i > 1; --i) {
                    const std::size_t j = static_cast<std::size_t>(std::rand()) % i;
                    std::swap(voiceOrder[i - 1], voiceOrder[j]);
                }
                if (voiceOrderLength > 1 &&
                    static_cast<int>(voiceOrder[0]) == lastVoiceIndex) {
                    std::swap(voiceOrder[0], voiceOrder[1]);
                }
            }
            const int selected = static_cast<int>(voiceOrder[voiceOrderCursor++]);
            lastVoiceIndex = selected;
            return selected;
        }

        void LoadOriginalVoices() {
            if (voiceLoadingStarted) return;
            voiceLoadingStarted = true;
            for (std::size_t i = 0; i < voicePaths.size(); ++i) {
                if (!std::ifstream(voicePaths[i]).good()) {
                    PaperLogger.info("Original ghost sound missing: {}", voicePaths[i]);
                    continue;
                }
                auto task = GlobalNamespace::MediaAsyncLoader::New_ctor()
                    ->LoadAudioClipFromFilePathAsync(voicePaths[i]);
                task->GetAwaiter().OnCompleted(
                    custom_types::MakeDelegate<System::Action*>(
                        std::function([task, i]() {
                            auto clip = task->get_Result();
                            if (clip) voiceClips[i] = clip.ptr();
                        }))
                );
            }
        }


    }

    void EnterResults() {
        active = true;
        LoadOriginalVoices();
    }

    void PlaySpawnVoice(UnityEngine::GameObject* ghost) {
        if (!active || !ghost) return;
        // Approximately 42% of spawned ghosts produce a sound.
        // The other 58% remain silent to prevent a crowded soundscape.
        if (std::rand() % 100 >= 42) return;
        auto* src = ghost->AddComponent<UnityEngine::AudioSource*>();
        if (!src) return;
        const int index = NextVoiceIndex();
        if (index < 0) {
            UnityEngine::Object::Destroy(src);
            return;  // No clips loaded yet; never invent a sound.
        }
        auto* clip = voiceClips[static_cast<std::size_t>(index)];
        src->set_playOnAwake(false);
        src->set_loop(false);
        src->set_spatialBlend(0.0f);
        src->set_volume(0.38f);
        src->set_clip(clip);
        src->Play();
    }

    void ExitResults() { active = false; }
}
