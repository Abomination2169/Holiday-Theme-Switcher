#include "Particles/menuParticles.hpp"

#include "UnityEngine/Bounds.hpp"
#include "UnityEngine/Camera.hpp"
#include "UnityEngine/Color.hpp"
#include "UnityEngine/Color32.hpp"
#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/Material.hpp"
#include "UnityEngine/Mesh.hpp"
#include "UnityEngine/MeshFilter.hpp"
#include "UnityEngine/MeshRenderer.hpp"
#include "UnityEngine/MonoBehaviour.hpp"
#include "UnityEngine/ParticleSystem.hpp"
#include "UnityEngine/ParticleSystemRenderer.hpp"
#include "UnityEngine/Rendering/MeshUpdateFlags.hpp"
#include "UnityEngine/Resources.hpp"
#include "UnityEngine/Shader.hpp"
#include "UnityEngine/Texture2D.hpp"
#include "UnityEngine/TextureFormat.hpp"
#include "UnityEngine/Time.hpp"
#include "UnityEngine/Transform.hpp"
#include "UnityEngine/Vector2.hpp"
#include "UnityEngine/Vector3.hpp"
#include "custom-types/shared/macros.hpp"

#include <algorithm>
#include <cmath>
#include <memory>
#include <random>
#include <unordered_map>
#include <vector>

DECLARE_CLASS_CODEGEN(Seasonal, MenuParticleController, UnityEngine::MonoBehaviour) {
    DECLARE_INSTANCE_FIELD(Array<UnityEngine::Vector3>*, vertexBuffer);
    DECLARE_INSTANCE_FIELD(UnityEngine::Mesh*, particleMesh);
    DECLARE_INSTANCE_FIELD(UnityEngine::Material*, particleMaterial);
    DECLARE_INSTANCE_FIELD(UnityEngine::Texture2D*, ownedTexture);
    DECLARE_INSTANCE_METHOD(void, LateUpdate);
    DECLARE_INSTANCE_METHOD(void, OnDestroy);
};
DEFINE_TYPE(Seasonal, MenuParticleController);

namespace {
    using namespace UnityEngine;
    using Seasonal::MenuParticlePattern;

    constexpr float fullTurn = 6.2831853f;
    constexpr float gravity = 9.81f;
    constexpr float floorY = 0.0f;
    constexpr float spawnHalfWidth = 10.0f;
    constexpr float collisionDampen = 0.08f;
    constexpr float collisionLifetimeLoss = 0.02f;

    struct Particle {
        Vector3 position;
        float remaining = 0.0f;
        float size;
        float downwardVelocity;
        float driftX;
        float driftZ;
        float phase;
        float rotation;
        float spin;
        bool grounded;
    };

    struct Emitter {
        MenuParticlePattern pattern;
        std::vector<Particle> particles;
        std::mt19937 random{std::random_device{}()};
        float emissionRemainder = 0.0f;
        int nextSlot = 0;

        float Range(float min, float max) {
            return std::uniform_real_distribution<float>(min, max)(random);
        }

        void Spawn(Particle& particle) {
            particle = {
                .position = Vector3(Range(-spawnHalfWidth, spawnHalfWidth),
                                    pattern.height + Range(-0.5f, 0.5f),
                                    Range(-spawnHalfWidth, spawnHalfWidth)),
                .remaining = pattern.lifetime,
                .size = Range(pattern.minSize, pattern.maxSize),
                .downwardVelocity = -pattern.fallSpeed,
                .driftX = Range(-pattern.flutter, pattern.flutter),
                .driftZ = Range(-pattern.flutter, pattern.flutter),
                .phase = Range(0.0f, fullTurn),
                .rotation = Range(0.0f, fullTurn),
                .spin = Range(-pattern.spin, pattern.spin),
                .grounded = false
            };
        }
    };

    std::unordered_map<Seasonal::MenuParticleController*, std::unique_ptr<Emitter>> emitters;

    Material* FindDustMaterial() {
        for (auto* particle : Resources::FindObjectsOfTypeAll<ParticleSystem*>()) {
            if (!particle || particle->name != "DustPS") continue;
            auto* renderer = particle->GetComponent<ParticleSystemRenderer*>();
            auto* material = renderer ? renderer->get_sharedMaterial().ptr() : nullptr;
            if (material && material->get_mainTexture()) return material;
        }
        return nullptr;
    }

    Texture2D* CreateSoftFlakeTexture() {
        constexpr int size = 16;
        auto* texture = Texture2D::New_ctor(size, size, TextureFormat::RGBA32, false);
        ArrayW<Color32> pixels(size * size);
        for (int y = 0; y < size; ++y) {
            for (int x = 0; x < size; ++x) {
                const float dx = (2.0f * x + 1.0f) / size - 1.0f;
                const float dy = (2.0f * y + 1.0f) / size - 1.0f;
                const float alpha = std::max(0.0f, 1.0f - dx * dx - dy * dy);
                pixels[y * size + x]._ctor(255, 255, 255, static_cast<uint8_t>(255.0f * alpha * alpha));
            }
        }
        texture->SetPixels32(pixels);
        texture->Apply(false);
        return texture;
    }

    void Advance(Particle& particle, const MenuParticlePattern& pattern, float dt, float wind, float contactY) {
        const float age = pattern.lifetime - particle.remaining;
        particle.remaining -= dt;
        if (particle.grounded) {
            const float friction = std::max(0.0f, 1.0f - 5.0f * dt);
            particle.driftX *= friction;
            particle.driftZ *= friction;
            particle.position.x += particle.driftX * dt;
            particle.position.z += particle.driftZ * dt;
        } else {
            const float xVelocity = particle.driftX + wind
                + pattern.flutter * std::sin(particle.phase + age * 1.6f);
            const float zVelocity = particle.driftZ
                + pattern.flutter * std::cos(particle.phase + age * 1.2f);
            particle.position.x += xVelocity * dt;
            particle.position.z += zVelocity * dt;
            particle.downwardVelocity -= gravity * pattern.gravityMultiplier * dt;
            particle.position.y += particle.downwardVelocity * dt;

            if (particle.position.y <= contactY) {
                particle.position.y = contactY;
                particle.downwardVelocity = 0.0f;
                particle.driftX = xVelocity * (1.0f - collisionDampen);
                particle.driftZ = zVelocity * (1.0f - collisionDampen);
                particle.remaining = std::max(0.0f, particle.remaining - pattern.lifetime * collisionLifetimeLoss);
                particle.grounded = true;
            }
        }
    }
}

UnityEngine::GameObject* Seasonal::CreateMenuParticles(const char* name, const MenuParticlePattern& pattern,
                                                       UnityEngine::Texture2D* spriteSheet, int frames) {
    using namespace UnityEngine;
    auto* dustMaterial = (!spriteSheet || pattern.useDustMaterial) ? FindDustMaterial() : nullptr;
    auto* crystalTemplate = pattern.useDustMaterial ? dustMaterial : nullptr;
    auto* shader = crystalTemplate ? nullptr : Shader::Find("Sprites/Default").ptr();
    if (!crystalTemplate && !shader) {
        if (spriteSheet) Object::Destroy(spriteSheet);
        return nullptr;
    }

    auto* root = GameObject::New_ctor(name);
    auto* controller = root->AddComponent<MenuParticleController*>();
    if (!controller) {
        if (spriteSheet) Object::Destroy(spriteSheet);
        Object::Destroy(root);
        return nullptr;
    }

    auto emitter = std::make_unique<Emitter>();
    emitter->pattern = pattern;
    emitter->particles.resize(pattern.capacity);
    controller->ownedTexture = spriteSheet;
    Texture* texture = spriteSheet;
    if (!texture && dustMaterial) texture = dustMaterial->get_mainTexture().ptr();
    if (!texture) {
        controller->ownedTexture = CreateSoftFlakeTexture();
        texture = controller->ownedTexture;
    }

    // The original crystal flakes used a private copy of DustPS's material.
    // Keep its tint and blend settings when replacing the texture.
    controller->particleMaterial = crystalTemplate
        ? Object::Instantiate(crystalTemplate)
        : Material::New_ctor(shader);
    controller->particleMaterial->set_mainTexture(texture);
    if (!crystalTemplate) {
        controller->particleMaterial->SetColor(StringW("_Color"), Color(1, 1, 1, 1));
    }

    controller->particleMesh = Mesh::New_ctor();
    controller->particleMesh->MarkDynamic();
    const int vertices = pattern.capacity * 4;
    ArrayW<Vector3> positions(vertices);
    ArrayW<Vector2> uvs(vertices);
    ArrayW<Color32> colors(vertices);
    ArrayW<int> indices(pattern.capacity * 6);
    Color32 white;
    white._ctor(255, 255, 255, 255);
    for (int i = 0; i < pattern.capacity; ++i) {
        const int vertex = i * 4;
        const int index = i * 6;
        const int frame = std::uniform_int_distribution<int>(0, frames - 1)(emitter->random);
        const float left = static_cast<float>(frame) / frames;
        const float right = static_cast<float>(frame + 1) / frames;
        uvs[vertex] = Vector2(left, 0.0f);
        uvs[vertex + 1] = Vector2(right, 0.0f);
        uvs[vertex + 2] = Vector2(left, 1.0f);
        uvs[vertex + 3] = Vector2(right, 1.0f);
        for (int j = 0; j < 4; ++j) colors[vertex + j] = white;
        indices[index] = vertex;
        indices[index + 1] = vertex + 2;
        indices[index + 2] = vertex + 1;
        indices[index + 3] = vertex + 1;
        indices[index + 4] = vertex + 2;
        indices[index + 5] = vertex + 3;
    }

    controller->vertexBuffer = positions.operator->();
    controller->particleMesh->SetVertices(positions);
    controller->particleMesh->SetUVs(0, uvs);
    controller->particleMesh->set_colors32(colors);
    controller->particleMesh->SetTriangles(indices, 0, false);
    controller->particleMesh->set_bounds(Bounds(Vector3(0, 4, 0), Vector3(60, 12, 60)));
    root->AddComponent<MeshFilter*>()->set_sharedMesh(controller->particleMesh);
    root->AddComponent<MeshRenderer*>()->set_sharedMaterial(controller->particleMaterial);
    emitters.emplace(controller, std::move(emitter));
    return root;
}

void Seasonal::DestroyMenuParticles(UnityEngine::GameObject* emitter) {
    if (!emitter) return;
    emitter->SetActive(false);
    UnityEngine::Object::Destroy(emitter);
}

void Seasonal::MenuParticleController::LateUpdate() {
    using namespace UnityEngine;
    const auto found = emitters.find(this);
    if (found == emitters.end()) return;
    auto& emitter = *found->second;
    const auto& pattern = emitter.pattern;
    const float dt = std::min(Time::get_deltaTime(), 0.05f);
    if (dt <= 0.0f) return;
    auto* camera = Camera::get_main().ptr();
    auto* view = camera ? camera->get_transform().ptr() : nullptr;
    const Vector3 right = view ? view->get_right() : Vector3(1, 0, 0);
    const Vector3 up = view ? view->get_up() : Vector3(0, 1, 0);
    const float time = Time::get_time();
    const float gust = 0.23f * std::sin(time * 0.17f) + 0.11f * std::sin(time * 0.43f + 1.7f)
                     + 0.06f * std::sin(time * 0.083f + 2.4f);
    const float wind = pattern.wind * (1.0f + gust / 0.65f);

    emitter.emissionRemainder += pattern.rate * dt;
    int toSpawn = static_cast<int>(emitter.emissionRemainder);
    emitter.emissionRemainder -= toSpawn;
    while (toSpawn-- > 0) {
        auto& particle = emitter.particles[emitter.nextSlot];
        if (particle.remaining <= 0.0f) emitter.Spawn(particle);
        emitter.nextSlot = (emitter.nextSlot + 1) % pattern.capacity;
    }

    ArrayW<Vector3> positions(vertexBuffer);
    for (int i = 0; i < pattern.capacity; ++i) {
        auto& particle = emitter.particles[i];
        const int vertex = i * 4;
        if (particle.remaining > 0.0f) {
            const float halfSize = particle.size * 0.5f;
            const float sine = std::sin(particle.rotation) * halfSize;
            const float cosine = std::cos(particle.rotation) * halfSize;
            // Keep the camera-facing sprite after impact. Lift its center
            // by the rotated quad's vertical extent so its lowest edge
            // rests at the floor without changing its landing angle.
            const Vector3 horizontal(right.x * cosine + up.x * sine,
                                     right.y * cosine + up.y * sine,
                                     right.z * cosine + up.z * sine);
            const Vector3 vertical(up.x * cosine - right.x * sine,
                                   up.y * cosine - right.y * sine,
                                   up.z * cosine - right.z * sine);
            const float restingY = floorY + std::abs(horizontal.y) + std::abs(vertical.y);
            Advance(particle, pattern, dt, wind, restingY);
            if (!particle.grounded) particle.rotation += particle.spin * dt;

            if (particle.remaining > 0.0f) {
                const Vector3 center(particle.position.x,
                                     particle.grounded ? restingY : std::max(particle.position.y, restingY),
                                     particle.position.z);
                positions[vertex] = Vector3(center.x - horizontal.x - vertical.x,
                                            center.y - horizontal.y - vertical.y,
                                            center.z - horizontal.z - vertical.z);
                positions[vertex + 1] = Vector3(center.x + horizontal.x - vertical.x,
                                                center.y + horizontal.y - vertical.y,
                                                center.z + horizontal.z - vertical.z);
                positions[vertex + 2] = Vector3(center.x - horizontal.x + vertical.x,
                                                center.y - horizontal.y + vertical.y,
                                                center.z - horizontal.z + vertical.z);
                positions[vertex + 3] = Vector3(center.x + horizontal.x + vertical.x,
                                                center.y + horizontal.y + vertical.y,
                                                center.z + horizontal.z + vertical.z);
                continue;
            }
        }
        for (int j = 0; j < 4; ++j) positions[vertex + j] = Vector3(0, 0, 0);
    }
    particleMesh->SetVertices(positions, 0, pattern.capacity * 4,
                               Rendering::MeshUpdateFlags::DontRecalculateBounds);
}

void Seasonal::MenuParticleController::OnDestroy() {
    const auto found = emitters.find(this);
    if (found == emitters.end()) return;
    UnityEngine::Object::Destroy(particleMesh);
    UnityEngine::Object::Destroy(particleMaterial);
    if (ownedTexture) UnityEngine::Object::Destroy(ownedTexture);
    emitters.erase(found);
}
