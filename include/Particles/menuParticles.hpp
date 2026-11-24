#pragma once

namespace UnityEngine { class GameObject; class Texture2D; }

namespace Seasonal {
    struct MenuParticlePattern {
        int capacity;
        float rate;
        float lifetime;
        float height;
        float minSize;
        float maxSize;
        float gravityMultiplier;
        float fallSpeed;
        float wind;
        float flutter;
        float spin;
        bool useDustMaterial = false;
    };

    // Transfers ownership of the optional sprite sheet to the emitter.
    UnityEngine::GameObject* CreateMenuParticles(const char* name, const MenuParticlePattern& pattern,
                                                  UnityEngine::Texture2D* spriteSheet = nullptr, int frames = 1);
    void DestroyMenuParticles(UnityEngine::GameObject* emitter);
}
