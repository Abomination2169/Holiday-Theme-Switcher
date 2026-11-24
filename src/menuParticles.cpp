#include "Particles/menuParticles.hpp"

#include "System/IntPtr.hpp"
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
#include "UnityEngine/ParticleSystemGradientMode.hpp"
#include "UnityEngine/ParticleSystemSimulationSpace.hpp"
#include "UnityEngine/Rendering/MeshUpdateFlags.hpp"
#include "UnityEngine/Resources.hpp"
#include "UnityEngine/Shader.hpp"
#include "UnityEngine/Texture2D.hpp"
#include "UnityEngine/TextureFormat.hpp"
#include "UnityEngine/TextureWrapMode.hpp"
#include "UnityEngine/Time.hpp"
#include "UnityEngine/Transform.hpp"
#include "UnityEngine/Vector2.hpp"
#include "UnityEngine/Vector3.hpp"
#include "custom-types/shared/macros.hpp"
#include "paper2_scotland2/shared/logger.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

DECLARE_CLASS_CODEGEN(Seasonal, MenuParticleController, UnityEngine::MonoBehaviour) {
    DECLARE_INSTANCE_FIELD(Array<UnityEngine::Vector3>*, vertexBuffer);
    DECLARE_INSTANCE_FIELD(Array<UnityEngine::Color32>*, crystalColorBuffer);
    DECLARE_INSTANCE_FIELD(UnityEngine::Mesh*, particleMesh);
    DECLARE_INSTANCE_FIELD(UnityEngine::Material*, particleMaterial);
    DECLARE_INSTANCE_FIELD(UnityEngine::Texture2D*, ownedTexture);
    DECLARE_INSTANCE_FIELD(Array<UnityEngine::ParticleSystem*>*, nativeSystems);
    DECLARE_INSTANCE_FIELD(Array<UnityEngine::Material*>*, nativeMaterials);
    DECLARE_INSTANCE_FIELD(Array<UnityEngine::Texture2D*>*, nativeTextures);
    DECLARE_INSTANCE_METHOD(void, LateUpdate);
    DECLARE_INSTANCE_METHOD(void, OnDestroy);
};
DEFINE_TYPE(Seasonal, MenuParticleController);

namespace {
    using namespace UnityEngine;
    using Seasonal::MenuParticlePattern;

    constexpr auto ParticleLogger = Paper::ConstLoggerContext("seasonalthemes");

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
        std::vector<ParticleSystem_Particle> nativeBuffer;
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

    ParticleSystem* FindDustSystem() {
        for (auto* particle : Resources::FindObjectsOfTypeAll<ParticleSystem*>()) {
            if (particle && particle->name == "DustPS") return particle;
        }
        return nullptr;
    }

    // Unity 6 strips the texture-sheet setters used by the old port. Give
    // each atlas frame its own DustPS clone with the original vertex streams.
    bool CreateNativeDustRenderers(GameObject* root, Seasonal::MenuParticleController* controller,
                                   ParticleSystem* dust, Material* dustMaterial, Texture2D* sheet,
                                   Emitter& emitter, int frames) {
        if (!dustMaterial || !sheet || !sheet->get_isReadable()) return false;
        const int width = sheet->get_width();
        const int height = sheet->get_height();
        if (width <= 0 || height <= 0 || width % frames != 0) return false;

        const int frameWidth = width / frames;
        const int capacity = (emitter.pattern.capacity + frames - 1) / frames;
        ArrayW<ParticleSystem*> systems(frames);
        ArrayW<Material*> materials(frames);
        ArrayW<Texture2D*> textures(frames);
        root->SetActive(false);
        int frame = 0;
        for (; frame < frames; ++frame) {
            auto* clone = Object::Instantiate(dust->get_gameObject().ptr(), root->get_transform().ptr(), false);
            if (!clone) break;
            clone->set_name("ChristmasEveCrystalFrame");
            auto* particles = clone->GetComponent<ParticleSystem*>();
            auto* renderer = clone->GetComponent<ParticleSystemRenderer*>();
            if (!particles || !renderer) {
                Object::Destroy(clone);
                break;
            }
            auto* tile = Texture2D::New_ctor(frameWidth, height, TextureFormat::RGBA32, false);
            tile->set_wrapMode(TextureWrapMode::Clamp);
            tile->SetPixels(sheet->GetPixels(frame * frameWidth, 0, frameWidth, height));
            tile->Apply(false);
            auto* material = Object::Instantiate(dustMaterial);
            material->set_mainTexture(tile);
            renderer->set_sharedMaterial(material);

            auto main = particles->main;
            main.set_playOnAwake(false);
            main.set_simulationSpace(ParticleSystemSimulationSpace::World);
            main.set_maxParticles(capacity);
            particles->set_startSize(emitter.pattern.maxSize);
            particles->set_startLifetime(emitter.pattern.lifetime);
            particles->set_gravityModifier(emitter.pattern.gravityMultiplier);
            particles->emission.set_enabled(false);
            clone->SetActive(true);
            systems[frame] = particles;
            materials[frame] = material;
            textures[frame] = tile;
        }
        if (frame != frames) {
            for (int i = 0; i < frame; ++i) {
                auto* child = systems[i]->get_gameObject().ptr();
                child->SetActive(false);
                Object::Destroy(child);
                Object::Destroy(materials[i]);
                Object::Destroy(textures[i]);
            }
            root->SetActive(true);
            return false;
        }

        controller->nativeSystems = systems.operator->();
        controller->nativeMaterials = materials.operator->();
        controller->nativeTextures = textures.operator->();
        emitter.nativeBuffer.resize(capacity);
        root->SetActive(true);
        for (auto* particles : systems) {
            particles->Clear(false);
            particles->Play(false);
        }
        return true;
    }

    void UpdateNativeSnow(ArrayW<ParticleSystem*> systems, Emitter& emitter, int toSpawn) {
        const auto& pattern = emitter.pattern;
        for (int i = 0; i < toSpawn; ++i) {
            const int frame = std::uniform_int_distribution<int>(0, systems.size() - 1)(emitter.random);
            ParticleSystem_EmitParams params{};
            // The generated default constructor leaves the override flags uninitialized.
            std::memset(&params, 0, sizeof(params));
            params.set_position(Vector3(emitter.Range(-spawnHalfWidth, spawnHalfWidth),
                                        pattern.height + emitter.Range(-0.5f, 0.5f),
                                        emitter.Range(-spawnHalfWidth, spawnHalfWidth)));
            // Only override the spawn position. Unity samples DustPS's original
            // color, angle and velocity and evaluates its curves while playing.
            systems[frame]->Emit(params, 1);
        }

        auto& buffer = emitter.nativeBuffer;
        const System::IntPtr data(buffer.data());
        const int capacity = static_cast<int>(buffer.size());
        const float contactY = floorY + pattern.maxSize * 0.5f;
        for (auto* system : systems) {
            const int count = system->GetParticlesWithNativeArray(data, capacity, capacity, 0);
            bool touched = false;
            for (int i = 0; i < count; ++i) {
                auto& particle = buffer[i];
                if (particle.m_Position.y >= contactY) continue;
                particle.m_Position.y = contactY;
                particle.m_Velocity.y = std::max(0.0f, particle.m_Velocity.y);
                particle.m_AngularVelocity = Vector3(0, 0, 0);
                touched = true;
            }
            if (touched) system->SetParticlesWithNativeArray(data, capacity, count, 0);
        }
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

    Color32 CrystalReflection(const Particle& particle, const Vector3& eye, float angle) {
        const float dx = eye.x - particle.position.x;
        const float dy = eye.y - particle.position.y;
        const float dz = eye.z - particle.position.z;
        const float distance = std::max(0.001f, std::sqrt(dx * dx + dy * dy + dz * dz));
        const float phase = particle.phase + angle + (3.1f * dx + 1.7f * dy + 2.2f * dz) / distance;
        const float facing = 0.5f + 0.5f * std::cos(phase);
        const float facingSquared = facing * facing;
        const float glint = facingSquared * facingSquared;
        const float hue = 2.0f * particle.phase + 1.6f * phase;
        const auto channel = [glint, hue](float offset) {
            // Most viewing angles show white ice. The color peaks in a narrow glint.
            return static_cast<uint8_t>(255.0f * (1.0f - 0.34f * glint * (0.5f - 0.5f * std::cos(hue + offset))));
        };
        Color32 color;
        color._ctor(channel(0.0f), channel(fullTurn / 3.0f), channel(2.0f * fullTurn / 3.0f), 255);
        return color;
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
    auto* dust = (!spriteSheet || pattern.useNativeDustRenderer) ? FindDustSystem() : nullptr;
    auto* renderer = dust ? dust->GetComponent<ParticleSystemRenderer*>() : nullptr;
    auto* dustMaterial = renderer ? renderer->get_sharedMaterial().ptr() : nullptr;

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
    if (pattern.useNativeDustRenderer) {
        if (CreateNativeDustRenderers(root, controller, dust, dustMaterial, spriteSheet, *emitter, frames)) {
            auto shader = dustMaterial->get_shader();
            const auto startColor = dust->main.get_startColor();
            ParticleLogger.info("[CrystalSnow] DustPS shader={}, startColorMode={}, minRGB=({},{},{}), maxRGB=({},{},{})",
                shader ? static_cast<std::string>(shader->get_name()) : std::string("unavailable"),
                static_cast<int>(startColor.m_Mode),
                startColor.m_ColorMin.r, startColor.m_ColorMin.g, startColor.m_ColorMin.b,
                startColor.m_ColorMax.r, startColor.m_ColorMax.g, startColor.m_ColorMax.b);
            Object::Destroy(spriteSheet);
            emitters.emplace(controller, std::move(emitter));
            return root;
        }
        ParticleLogger.info("[CrystalSnow] Using sprite renderer fallback");
    }

    auto* shader = Shader::Find("Sprites/Default").ptr();
    if (!shader) {
        if (spriteSheet) Object::Destroy(spriteSheet);
        Object::Destroy(root);
        return nullptr;
    }

    controller->ownedTexture = spriteSheet;
    Texture* texture = spriteSheet;
    if (!texture && dustMaterial) texture = dustMaterial->get_mainTexture().ptr();
    if (!texture) {
        controller->ownedTexture = CreateSoftFlakeTexture();
        texture = controller->ownedTexture;
    }

    controller->particleMaterial = Material::New_ctor(shader);
    controller->particleMaterial->set_mainTexture(texture);
    controller->particleMaterial->SetColor(StringW("_Color"), Color(1, 1, 1, 1));

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
    if (pattern.useNativeDustRenderer) controller->crystalColorBuffer = colors.operator->();
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
    if (nativeSystems) {
        UpdateNativeSnow(ArrayW<ParticleSystem*>(nativeSystems), emitter, toSpawn);
        return;
    }
    while (toSpawn-- > 0) {
        auto& particle = emitter.particles[emitter.nextSlot];
        if (particle.remaining <= 0.0f) emitter.Spawn(particle);
        emitter.nextSlot = (emitter.nextSlot + 1) % pattern.capacity;
    }

    ArrayW<Vector3> positions(vertexBuffer);
    ArrayW<Color32> crystalColors(crystalColorBuffer);
    const Vector3 eye = pattern.useNativeDustRenderer && view ? view->get_position() : Vector3(0, 1.6f, 0);
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
            const float displayedAngle = particle.rotation;
            if (!particle.grounded) particle.rotation += particle.spin * dt;

            if (particle.remaining > 0.0f) {
                const Vector3 center(particle.position.x,
                                     particle.grounded ? restingY : std::max(particle.position.y, restingY),
                                     particle.position.z);
                if (crystalColorBuffer) {
                    const auto tint = CrystalReflection(particle, eye, displayedAngle);
                    for (int j = 0; j < 4; ++j) crystalColors[vertex + j] = tint;
                }
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
    if (crystalColorBuffer) particleMesh->set_colors32(crystalColors);
    particleMesh->SetVertices(positions, 0, pattern.capacity * 4,
                               Rendering::MeshUpdateFlags::DontRecalculateBounds);
}

void Seasonal::MenuParticleController::OnDestroy() {
    const auto found = emitters.find(this);
    if (found == emitters.end()) return;
    if (nativeMaterials) {
        for (auto* material : ArrayW<UnityEngine::Material*>(nativeMaterials)) UnityEngine::Object::Destroy(material);
        for (auto* texture : ArrayW<UnityEngine::Texture2D*>(nativeTextures)) UnityEngine::Object::Destroy(texture);
    }
    if (particleMesh) UnityEngine::Object::Destroy(particleMesh);
    if (particleMaterial) UnityEngine::Object::Destroy(particleMaterial);
    if (ownedTexture) UnityEngine::Object::Destroy(ownedTexture);
    emitters.erase(found);
}
