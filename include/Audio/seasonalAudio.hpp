#pragma once
#include "GlobalNamespace/MainMenuViewController.hpp"
#include "GlobalNamespace/SongPreviewPlayer.hpp"
namespace SeasonalAudio {
bool IsThanksgiving();
bool IsHalloween();
bool IsChristmasEve();
bool IsNewYears();
float AdjustPreviewStartTime(GlobalNamespace::SongPreviewPlayer* player, UnityEngine::AudioClip* clip, float requestedStartTime, bool isDefault);
void CaptureMenuPlayer(GlobalNamespace::SongPreviewPlayer* player);
void EnterMenu(GlobalNamespace::MainMenuViewController* menu);
void EnterSoloSelection();
void ExitMenu();
void RefreshTheme();
void Tick();
}
