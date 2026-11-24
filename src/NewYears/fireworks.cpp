#include "beatsaber-hook/shared/safeptr.hpp"
#include "NewYears/newYears.hpp"
#include "GlobalNamespace/FireworksController.hpp"
#include "UnityEngine/Resources.hpp"
#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/Behaviour.hpp"

using namespace UnityEngine;
using namespace GlobalNamespace;

namespace NewYears::fireworks {
static safe_ptr<FireworksController*, true> controller;
static bool previouslyEnabled = false;
static bool enabledByMod = false;

void Create() {
    if (enabledByMod) return;
    for (auto* candidate : Resources::FindObjectsOfTypeAll<FireworksController*>()) {
        if (!candidate || !candidate->gameObject || !candidate->gameObject->activeInHierarchy)
            continue;
        controller = candidate;
        previouslyEnabled = candidate->get_enabled();
        if (!previouslyEnabled) {
            candidate->set_enabled(true);
            enabledByMod = true;
        }
        return;
    }
}
void Destroy() {
    if (controller && enabledByMod)
        controller->set_enabled(previouslyEnabled);
    controller = nullptr;
    enabledByMod = false;
}
}
