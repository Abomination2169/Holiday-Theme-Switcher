#include "ChristmasEve/snow.hpp"
#include <cmath>
#include <functional>

#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/Transform.hpp"
#include "UnityEngine/Vector3.hpp"
#include "UnityEngine/Color.hpp"
#include "UnityEngine/PrimitiveType.hpp"
#include "UnityEngine/SphereCollider.hpp"
#include "UnityEngine/MeshCollider.hpp"
#include "UnityEngine/MeshRenderer.hpp"
#include "UnityEngine/MeshFilter.hpp"
#include "UnityEngine/Mesh.hpp"
#include "UnityEngine/Object.hpp"
#include "beatsaber-hook/shared/utils/typedefs-array.hpp"
#include "UnityEngine/Material.hpp"

//using namespace GlobalNamespace;
using namespace UnityEngine;

namespace ChristmasEve::snowPrimitives {

    SafePtrUnity<GameObject> snowPrimitives;

    void CreateSnowPrimitives() {
        // Don't create snow primitives if already created
        if(snowPrimitives) return;

        auto menuEnvironment = GameObject::Find("DefaultMenuEnvironment");
        auto note = GameObject::Find("Note");
        if (!note) note = GameObject::Find("Note (3)");
        auto* noteRenderer = note ? note->GetComponent<MeshRenderer*>() : nullptr;
        if (!menuEnvironment || !menuEnvironment->transform || !noteRenderer) return;

        // Create an object to contain all of the spheres
        snowPrimitives = GameObject::New_ctor("ChristmasEveSnowPrimitives");
        if (!snowPrimitives || !snowPrimitives->transform) return;
        snowPrimitives->transform->parent = menuEnvironment->transform;

        // Create a white sphere template
        GameObject* whiteSphere = GameObject::CreatePrimitive(PrimitiveType::Sphere);
        whiteSphere->transform->parent = snowPrimitives->transform;
        whiteSphere->GetComponent<MeshRenderer*>()->material = noteRenderer->material;
        Object::DestroyImmediate(whiteSphere->GetComponent<SphereCollider*>());
        MeshCollider* whiteSphereCollider = whiteSphere->AddComponent<MeshCollider*>();
        whiteSphereCollider->convex = true;

        // Create a black sphere template
        GameObject* blackSphere = GameObject::CreatePrimitive(PrimitiveType::Sphere);
        blackSphere->transform->parent = snowPrimitives->transform;
        Material* blackMaterial = blackSphere->GetComponent<MeshRenderer*>()->material;
        blackMaterial->SetColor("_Color", Color(0.0f, 0.0f, 0.0f, 1.0f));
        blackMaterial->SetFloat("_Glossiness", 0.0f);
        blackMaterial->SetFloat("_Metallic", 1.0f);

        // Create a function that automatically duplicated a sphere template based on a position and size
        std::function<void(GameObject*, Vector3, Vector3)> CreateSphere = [](GameObject* templateSphere, Vector3 position, Vector3 size){
            GameObject* newSphere = GameObject::Instantiate(templateSphere);
            newSphere->transform->parent = snowPrimitives->transform;
            newSphere->transform->position = Vector3(position.x / 2.0f, position.y / 2.0f, position.z / 2.0f); // Division by 2 is to fix the mistake of not realizing that Blender's cube is 2x2x2 while Unity's is 1x1x1 without retyping all the coordinates. It's not lazy, it's called taking shortcuts because this mod is supposed to be released in just a few hours
            newSphere->transform->localScale = size;
        };

        // Create a bunch of lumps of snow on the ground
        CreateSphere(whiteSphere, Vector3(-5.5f, -1.3f, 16.1f), Vector3(10.0f, 2.0f, 10.0f)); // 0
        CreateSphere(whiteSphere, Vector3(16.4f, -1.3f, 0.0f), Vector3(10.0f, 2.0f, 10.0f)); // 1
        CreateSphere(whiteSphere, Vector3(6.5f, -1.3f, 16.4f), Vector3(10.0f, 2.0f, 10.0f)); // 2
        CreateSphere(whiteSphere, Vector3(15.9f, -1.0f, 10.8f), Vector3(10.0f, 2.0f, 10.0f)); // 3
        CreateSphere(whiteSphere, Vector3(-18.3f, -0.8f, -11.4f), Vector3(10.0f, 2.0f, 10.0f)); // 4
        CreateSphere(whiteSphere, Vector3(-9.0f, -1.7f, -7.7f), Vector3(10.0f, 2.0f, 10.0f)); // 5
        CreateSphere(whiteSphere, Vector3(-18.1f, -1.3f, 2.7f), Vector3(10.0f, 2.0f, 10.0f)); // 6
        CreateSphere(whiteSphere, Vector3(-2.5f, -1.3f, -14.2f), Vector3(10.0f, 2.0f, 10.0f)); // 7
        CreateSphere(whiteSphere, Vector3(-15.7f, -1.3f, 12.0f), Vector3(10.0f, 2.0f, 10.0f)); // 8
        CreateSphere(whiteSphere, Vector3(10.0f, -1.3f, -10.5f), Vector3(10.0f, 2.0f, 10.0f)); // 9


        // Build #54: one continuous, snow-white panoramic horizon.
        // Keep Laynia/Furzz's original snow drift spheres and their colliders.
        // No blue rock faces, per-facet color changes, ground underlay or extra collision.
        // A single smooth static mesh wraps all directions at a distance, with its
        // inner skirt buried beneath the existing drifts.
        {
            constexpr int segments = 96;
            constexpr float innerRadius = 11.8f;
            constexpr float crestRadius = 20.0f;
            constexpr float outerRadius = 23.0f;
            constexpr float pi = 3.14159265358979323846f;
            auto* panorama = GameObject::New_ctor("ChristmasSnowPanorama");
            panorama->transform->parent = snowPrimitives->transform;
            panorama->transform->localPosition = Vector3(0.0f, 0.0f, 0.0f);
            auto* mesh = Mesh::New_ctor();
            ArrayW<Vector3> vertices(segments * 3);
            ArrayW<int> triangles(segments * 12);
            for (int i = 0; i < segments; ++i) {
                const float theta = (2.0f * pi * i) / segments;
                const float x = std::cos(theta);
                const float z = std::sin(theta);
                // Several broad, irregular alpine crests without angular facets.
                const float peaks = 0.72f * std::sin(theta * 5.0f + 0.4f)
                                  + 0.39f * std::sin(theta * 9.0f - 0.8f)
                                  + 0.22f * std::sin(theta * 13.0f + 1.1f);
                float height = 2.75f + peaks;
                // Build #60: the Build #54 ring rose above Build #52's low
                // center valley (Y~1.2 versus panorama Y~3.8 at X=0).
                // Follow the historical summit profile only on the front arc;
                // blend continuously into the untouched panorama at either side.
                if (z > 0.0f && x * crestRadius > -11.350f && x * crestRadius < 9.150f) {
                    const float frontX = x * crestRadius;
                    constexpr float valleyX[] = {
                        -11.350f, -8.000f, -5.500f, -2.350f, -1.350f,
                         1.000f, 3.500f, 7.150f, 9.150f
                    };
                    constexpr float valleyY[] = {
                        4.504f, 3.424f, 2.290f, 1.588f, 1.210f,
                        1.318f, 1.858f, 2.776f, 4.180f
                    };
                    float historicHeight = valleyY[8];
                    for (int j = 0; j < 8; ++j) {
                        if (frontX <= valleyX[j + 1]) {
                            const float fraction = (frontX - valleyX[j]) /
                                                   (valleyX[j + 1] - valleyX[j]);
                            historicHeight = valleyY[j] +
                                fraction * (valleyY[j + 1] - valleyY[j]);
                            break;
                        }
                    }
                    // Keep the background ring lower than the historical summit.
                    // The softened shoulder preserves seamless 360-degree continuity.
                    float blend = 1.0f;
                    if (frontX < -8.000f) blend = (frontX + 11.350f) / 3.350f;
                    if (frontX > 7.150f) blend = (9.150f - frontX) / 2.000f;
                    blend = blend * blend * (3.0f - 2.0f * blend);
                    const float lowered = (historicHeight - 0.30f < height)
                        ? historicHeight - 0.30f : height;
                    height += (lowered - height) * blend;
                }
                vertices[i * 3] = Vector3(x * innerRadius, -1.55f, z * innerRadius);
                vertices[i * 3 + 1] = Vector3(x * crestRadius, height, z * crestRadius);
                vertices[i * 3 + 2] = Vector3(x * outerRadius, -1.8f, z * outerRadius);
                const int next = (i + 1) % segments;
                const int base = i * 12;
                const int inner = i * 3;
                const int outer = next * 3;
                triangles[base] = inner;
                triangles[base + 1] = outer;
                triangles[base + 2] = inner + 1;
                triangles[base + 3] = outer;
                triangles[base + 4] = outer + 1;
                triangles[base + 5] = inner + 1;
                triangles[base + 6] = inner + 1;
                triangles[base + 7] = outer + 1;
                triangles[base + 8] = inner + 2;
                triangles[base + 9] = outer + 1;
                triangles[base + 10] = outer + 2;
                triangles[base + 11] = inner + 2;
            }
            mesh->set_vertices(vertices);
            mesh->set_triangles(triangles);
            mesh->RecalculateNormals();
            panorama->AddComponent<MeshFilter*>()->set_sharedMesh(mesh);
            // Reuse the original drift's snow material for uniform white.
            panorama->AddComponent<MeshRenderer*>()->set_material(
                whiteSphere->GetComponent<MeshRenderer*>()->material);
        }
        // Build #60: restore the actual Build #52 far-north mesh triangles.
        // Five historical facet strips are combined into one snow-white static mesh.
        // All summit, shoulder and foothill vertices (including the central valley)
        // and triangle winding are copied exactly from that source, without colors.
        {
            auto* frontRange = GameObject::New_ctor("ChristmasFrontAlpineSilhouette");
            frontRange->transform->parent = snowPrimitives->transform;
            frontRange->transform->localPosition = Vector3(0.0f, 0.0f, 0.0f);
            auto* mesh = Mesh::New_ctor();
            ArrayW<Vector3> vertices{
                Vector3(-26.000f, -1.350f, 13.060f),
                Vector3(-26.350f, 0.448f, 17.350f),
                Vector3(-22.600f, 0.780f, 17.350f),
                Vector3(-26.000f, -1.350f, 13.060f),
                Vector3(-22.600f, 0.780f, 17.350f),
                Vector3(-23.000f, -1.350f, 13.540f),
                Vector3(-23.000f, -1.350f, 13.540f),
                Vector3(-22.600f, 0.780f, 17.350f),
                Vector3(-20.000f, -1.350f, 13.060f),
                Vector3(-20.000f, -1.350f, 13.060f),
                Vector3(-22.600f, 0.780f, 17.350f),
                Vector3(-20.350f, 0.992f, 17.350f),
                Vector3(-20.000f, -1.350f, 13.060f),
                Vector3(-20.350f, 0.992f, 17.350f),
                Vector3(-16.600f, 0.841f, 17.350f),
                Vector3(-20.000f, -1.350f, 13.060f),
                Vector3(-16.600f, 0.841f, 17.350f),
                Vector3(-17.000f, -1.350f, 13.540f),
                Vector3(-17.000f, -1.350f, 13.540f),
                Vector3(-16.600f, 0.841f, 17.350f),
                Vector3(-14.000f, -1.350f, 13.060f),
                Vector3(-14.000f, -1.350f, 13.060f),
                Vector3(-16.600f, 0.841f, 17.350f),
                Vector3(-14.350f, 1.143f, 17.350f),
                Vector3(-14.000f, -1.350f, 13.060f),
                Vector3(-14.350f, 1.143f, 17.350f),
                Vector3(-10.600f, 0.901f, 17.350f),
                Vector3(-14.000f, -1.350f, 13.060f),
                Vector3(-10.600f, 0.901f, 17.350f),
                Vector3(-11.000f, -1.350f, 13.540f),
                Vector3(-11.000f, -1.350f, 13.540f),
                Vector3(-10.600f, 0.901f, 17.350f),
                Vector3(-8.000f, -1.350f, 13.060f),
                Vector3(-8.000f, -1.350f, 13.060f),
                Vector3(-10.600f, 0.901f, 17.350f),
                Vector3(-8.350f, 0.599f, 17.350f),
                Vector3(-8.000f, -1.350f, 13.060f),
                Vector3(-8.350f, 0.599f, 17.350f),
                Vector3(-5.100f, 0.281f, 17.350f),
                Vector3(-8.000f, -1.350f, 13.060f),
                Vector3(-5.100f, 0.281f, 17.350f),
                Vector3(-5.500f, -1.350f, 13.540f),
                Vector3(-5.500f, -1.350f, 13.540f),
                Vector3(-5.100f, 0.281f, 17.350f),
                Vector3(-3.000f, -1.350f, 13.060f),
                Vector3(-3.000f, -1.350f, 13.060f),
                Vector3(-5.100f, 0.281f, 17.350f),
                Vector3(-3.350f, 0.085f, 17.350f),
                Vector3(-3.000f, -1.350f, 13.060f),
                Vector3(-3.350f, 0.085f, 17.350f),
                Vector3(-0.600f, -0.021f, 17.350f),
                Vector3(-3.000f, -1.350f, 13.060f),
                Vector3(-0.600f, -0.021f, 17.350f),
                Vector3(-1.000f, -1.350f, 13.540f),
                Vector3(-1.000f, -1.350f, 13.540f),
                Vector3(-0.600f, -0.021f, 17.350f),
                Vector3(1.000f, -1.350f, 13.060f),
                Vector3(1.000f, -1.350f, 13.060f),
                Vector3(-0.600f, -0.021f, 17.350f),
                Vector3(0.650f, 0.009f, 17.350f),
                Vector3(1.000f, -1.350f, 13.060f),
                Vector3(0.650f, 0.009f, 17.350f),
                Vector3(3.900f, 0.160f, 17.350f),
                Vector3(1.000f, -1.350f, 13.060f),
                Vector3(3.900f, 0.160f, 17.350f),
                Vector3(3.500f, -1.350f, 13.540f),
                Vector3(3.500f, -1.350f, 13.540f),
                Vector3(3.900f, 0.160f, 17.350f),
                Vector3(6.500f, -1.350f, 13.060f),
                Vector3(6.500f, -1.350f, 13.060f),
                Vector3(3.900f, 0.160f, 17.350f),
                Vector3(6.150f, 0.417f, 17.350f),
                Vector3(6.500f, -1.350f, 13.060f),
                Vector3(6.150f, 0.417f, 17.350f),
                Vector3(9.900f, 0.810f, 17.350f),
                Vector3(6.500f, -1.350f, 13.060f),
                Vector3(9.900f, 0.810f, 17.350f),
                Vector3(9.500f, -1.350f, 13.540f),
                Vector3(9.500f, -1.350f, 13.540f),
                Vector3(9.900f, 0.810f, 17.350f),
                Vector3(12.500f, -1.350f, 13.060f),
                Vector3(12.500f, -1.350f, 13.060f),
                Vector3(9.900f, 0.810f, 17.350f),
                Vector3(12.150f, 1.173f, 17.350f),
                Vector3(12.500f, -1.350f, 13.060f),
                Vector3(12.150f, 1.173f, 17.350f),
                Vector3(15.900f, 0.992f, 17.350f),
                Vector3(12.500f, -1.350f, 13.060f),
                Vector3(15.900f, 0.992f, 17.350f),
                Vector3(15.500f, -1.350f, 13.540f),
                Vector3(15.500f, -1.350f, 13.540f),
                Vector3(15.900f, 0.992f, 17.350f),
                Vector3(18.500f, -1.350f, 13.060f),
                Vector3(18.500f, -1.350f, 13.060f),
                Vector3(15.900f, 0.992f, 17.350f),
                Vector3(18.150f, 0.810f, 17.350f),
                Vector3(18.500f, -1.350f, 13.060f),
                Vector3(18.150f, 0.810f, 17.350f),
                Vector3(22.400f, 0.931f, 17.350f),
                Vector3(18.500f, -1.350f, 13.060f),
                Vector3(22.400f, 0.931f, 17.350f),
                Vector3(22.000f, -1.350f, 13.540f),
                Vector3(22.000f, -1.350f, 13.540f),
                Vector3(22.400f, 0.931f, 17.350f),
                Vector3(26.000f, -1.350f, 13.060f),
                Vector3(26.000f, -1.350f, 13.060f),
                Vector3(22.400f, 0.931f, 17.350f),
                Vector3(25.650f, 0.568f, 17.350f),
                Vector3(-22.600f, 0.780f, 17.350f),
                Vector3(-23.350f, 2.606f, 19.150f),
                Vector3(-19.820f, 3.090f, 19.150f),
                Vector3(-22.600f, 0.780f, 17.350f),
                Vector3(-19.820f, 3.090f, 19.150f),
                Vector3(-20.350f, 0.992f, 17.350f),
                Vector3(-16.600f, 0.841f, 17.350f),
                Vector3(-16.400f, 2.744f, 19.150f),
                Vector3(-14.350f, 3.436f, 19.150f),
                Vector3(-16.600f, 0.841f, 17.350f),
                Vector3(-14.350f, 3.436f, 19.150f),
                Vector3(-14.350f, 1.143f, 17.350f),
                Vector3(-10.600f, 0.901f, 17.350f),
                Vector3(-10.820f, 2.883f, 19.150f),
                Vector3(-7.400f, 2.191f, 19.150f),
                Vector3(-10.600f, 0.901f, 17.350f),
                Vector3(-7.400f, 2.191f, 19.150f),
                Vector3(-8.350f, 0.599f, 17.350f),
                Vector3(-5.100f, 0.281f, 17.350f),
                Vector3(-5.850f, 1.466f, 19.150f),
                Vector3(-2.820f, 1.016f, 19.150f),
                Vector3(-5.100f, 0.281f, 17.350f),
                Vector3(-2.820f, 1.016f, 19.150f),
                Vector3(-3.350f, 0.085f, 17.350f),
                Vector3(-0.600f, -0.021f, 17.350f),
                Vector3(-0.400f, 0.774f, 19.150f),
                Vector3(0.650f, 0.844f, 19.150f),
                Vector3(-0.600f, -0.021f, 17.350f),
                Vector3(0.650f, 0.844f, 19.150f),
                Vector3(0.650f, 0.009f, 17.350f),
                Vector3(3.900f, 0.160f, 17.350f),
                Vector3(3.680f, 1.189f, 19.150f),
                Vector3(7.100f, 1.777f, 19.150f),
                Vector3(3.900f, 0.160f, 17.350f),
                Vector3(7.100f, 1.777f, 19.150f),
                Vector3(6.150f, 0.417f, 17.350f),
                Vector3(9.900f, 0.810f, 17.350f),
                Vector3(9.150f, 2.675f, 19.150f),
                Vector3(12.680f, 3.505f, 19.150f),
                Vector3(9.900f, 0.810f, 17.350f),
                Vector3(12.680f, 3.505f, 19.150f),
                Vector3(12.150f, 1.173f, 17.350f),
                Vector3(15.900f, 0.992f, 17.350f),
                Vector3(16.100f, 3.090f, 19.150f),
                Vector3(18.150f, 2.675f, 19.150f),
                Vector3(15.900f, 0.992f, 17.350f),
                Vector3(18.150f, 2.675f, 19.150f),
                Vector3(18.150f, 0.810f, 17.350f),
                Vector3(22.400f, 0.931f, 17.350f),
                Vector3(22.180f, 2.952f, 19.150f),
                Vector3(26.600f, 2.122f, 19.150f),
                Vector3(22.400f, 0.931f, 17.350f),
                Vector3(26.600f, 2.122f, 19.150f),
                Vector3(25.650f, 0.568f, 17.350f),
                Vector3(-26.350f, 0.448f, 17.350f),
                Vector3(-25.400f, 1.846f, 19.150f),
                Vector3(-22.600f, 0.780f, 17.350f),
                Vector3(-22.600f, 0.780f, 17.350f),
                Vector3(-25.400f, 1.846f, 19.150f),
                Vector3(-23.350f, 2.606f, 19.150f),
                Vector3(-20.350f, 0.992f, 17.350f),
                Vector3(-19.820f, 3.090f, 19.150f),
                Vector3(-16.600f, 0.841f, 17.350f),
                Vector3(-16.600f, 0.841f, 17.350f),
                Vector3(-19.820f, 3.090f, 19.150f),
                Vector3(-16.400f, 2.744f, 19.150f),
                Vector3(-14.350f, 1.143f, 17.350f),
                Vector3(-14.350f, 3.436f, 19.150f),
                Vector3(-10.600f, 0.901f, 17.350f),
                Vector3(-10.600f, 0.901f, 17.350f),
                Vector3(-14.350f, 3.436f, 19.150f),
                Vector3(-10.820f, 2.883f, 19.150f),
                Vector3(-8.350f, 0.599f, 17.350f),
                Vector3(-7.400f, 2.191f, 19.150f),
                Vector3(-5.100f, 0.281f, 17.350f),
                Vector3(-5.100f, 0.281f, 17.350f),
                Vector3(-7.400f, 2.191f, 19.150f),
                Vector3(-5.850f, 1.466f, 19.150f),
                Vector3(-3.350f, 0.085f, 17.350f),
                Vector3(-2.820f, 1.016f, 19.150f),
                Vector3(-0.600f, -0.021f, 17.350f),
                Vector3(-0.600f, -0.021f, 17.350f),
                Vector3(-2.820f, 1.016f, 19.150f),
                Vector3(-0.400f, 0.774f, 19.150f),
                Vector3(0.650f, 0.009f, 17.350f),
                Vector3(0.650f, 0.844f, 19.150f),
                Vector3(3.900f, 0.160f, 17.350f),
                Vector3(3.900f, 0.160f, 17.350f),
                Vector3(0.650f, 0.844f, 19.150f),
                Vector3(3.680f, 1.189f, 19.150f),
                Vector3(6.150f, 0.417f, 17.350f),
                Vector3(7.100f, 1.777f, 19.150f),
                Vector3(9.900f, 0.810f, 17.350f),
                Vector3(9.900f, 0.810f, 17.350f),
                Vector3(7.100f, 1.777f, 19.150f),
                Vector3(9.150f, 2.675f, 19.150f),
                Vector3(12.150f, 1.173f, 17.350f),
                Vector3(12.680f, 3.505f, 19.150f),
                Vector3(15.900f, 0.992f, 17.350f),
                Vector3(15.900f, 0.992f, 17.350f),
                Vector3(12.680f, 3.505f, 19.150f),
                Vector3(16.100f, 3.090f, 19.150f),
                Vector3(18.150f, 0.810f, 17.350f),
                Vector3(18.150f, 2.675f, 19.150f),
                Vector3(22.400f, 0.931f, 17.350f),
                Vector3(22.400f, 0.931f, 17.350f),
                Vector3(18.150f, 2.675f, 19.150f),
                Vector3(22.180f, 2.952f, 19.150f),
                Vector3(-23.350f, 2.606f, 19.150f),
                Vector3(-23.350f, 4.072f, 19.770f),
                Vector3(-19.820f, 3.090f, 19.150f),
                Vector3(-19.820f, 3.090f, 19.150f),
                Vector3(-23.350f, 4.072f, 19.770f),
                Vector3(-20.000f, 4.828f, 20.130f),
                Vector3(-16.400f, 2.744f, 19.150f),
                Vector3(-17.000f, 4.288f, 20.330f),
                Vector3(-14.350f, 3.436f, 19.150f),
                Vector3(-14.350f, 3.436f, 19.150f),
                Vector3(-17.000f, 4.288f, 20.330f),
                Vector3(-13.350f, 5.368f, 19.770f),
                Vector3(-10.820f, 2.883f, 19.150f),
                Vector3(-11.350f, 4.504f, 20.130f),
                Vector3(-7.400f, 2.191f, 19.150f),
                Vector3(-7.400f, 2.191f, 19.150f),
                Vector3(-11.350f, 4.504f, 20.130f),
                Vector3(-8.000f, 3.424f, 20.330f),
                Vector3(-5.850f, 1.466f, 19.150f),
                Vector3(-5.500f, 2.290f, 19.770f),
                Vector3(-2.820f, 1.016f, 19.150f),
                Vector3(-2.820f, 1.016f, 19.150f),
                Vector3(-5.500f, 2.290f, 19.770f),
                Vector3(-2.350f, 1.588f, 20.130f),
                Vector3(-0.400f, 0.774f, 19.150f),
                Vector3(-1.350f, 1.210f, 20.330f),
                Vector3(0.650f, 0.844f, 19.150f),
                Vector3(0.650f, 0.844f, 19.150f),
                Vector3(-1.350f, 1.210f, 20.330f),
                Vector3(1.000f, 1.318f, 19.770f),
                Vector3(3.680f, 1.189f, 19.150f),
                Vector3(3.500f, 1.858f, 20.130f),
                Vector3(7.100f, 1.777f, 19.150f),
                Vector3(7.100f, 1.777f, 19.150f),
                Vector3(3.500f, 1.858f, 20.130f),
                Vector3(7.150f, 2.776f, 20.330f),
                Vector3(9.150f, 2.675f, 19.150f),
                Vector3(9.150f, 4.180f, 19.770f),
                Vector3(12.680f, 3.505f, 19.150f),
                Vector3(12.680f, 3.505f, 19.150f),
                Vector3(9.150f, 4.180f, 19.770f),
                Vector3(12.500f, 5.476f, 20.130f),
                Vector3(16.100f, 3.090f, 19.150f),
                Vector3(15.500f, 4.828f, 20.330f),
                Vector3(18.150f, 2.675f, 19.150f),
                Vector3(18.150f, 2.675f, 19.150f),
                Vector3(15.500f, 4.828f, 20.330f),
                Vector3(19.150f, 4.180f, 19.770f),
                Vector3(22.180f, 2.952f, 19.150f),
                Vector3(21.650f, 4.612f, 20.130f),
                Vector3(26.600f, 2.122f, 19.150f),
                Vector3(26.600f, 2.122f, 19.150f),
                Vector3(21.650f, 4.612f, 20.130f),
                Vector3(26.000f, 3.316f, 20.330f),
                Vector3(-25.400f, 1.846f, 19.150f),
                Vector3(-25.350f, 2.884f, 20.330f),
                Vector3(-23.350f, 4.072f, 19.770f),
                Vector3(-25.400f, 1.846f, 19.150f),
                Vector3(-23.350f, 4.072f, 19.770f),
                Vector3(-23.350f, 2.606f, 19.150f),
                Vector3(-19.820f, 3.090f, 19.150f),
                Vector3(-20.000f, 4.828f, 20.130f),
                Vector3(-17.000f, 4.288f, 20.330f),
                Vector3(-19.820f, 3.090f, 19.150f),
                Vector3(-17.000f, 4.288f, 20.330f),
                Vector3(-16.400f, 2.744f, 19.150f),
                Vector3(-14.350f, 3.436f, 19.150f),
                Vector3(-13.350f, 5.368f, 19.770f),
                Vector3(-11.350f, 4.504f, 20.130f),
                Vector3(-14.350f, 3.436f, 19.150f),
                Vector3(-11.350f, 4.504f, 20.130f),
                Vector3(-10.820f, 2.883f, 19.150f),
                Vector3(-7.400f, 2.191f, 19.150f),
                Vector3(-8.000f, 3.424f, 20.330f),
                Vector3(-5.500f, 2.290f, 19.770f),
                Vector3(-7.400f, 2.191f, 19.150f),
                Vector3(-5.500f, 2.290f, 19.770f),
                Vector3(-5.850f, 1.466f, 19.150f),
                Vector3(-2.820f, 1.016f, 19.150f),
                Vector3(-2.350f, 1.588f, 20.130f),
                Vector3(-1.350f, 1.210f, 20.330f),
                Vector3(-2.820f, 1.016f, 19.150f),
                Vector3(-1.350f, 1.210f, 20.330f),
                Vector3(-0.400f, 0.774f, 19.150f),
                Vector3(0.650f, 0.844f, 19.150f),
                Vector3(1.000f, 1.318f, 19.770f),
                Vector3(3.500f, 1.858f, 20.130f),
                Vector3(0.650f, 0.844f, 19.150f),
                Vector3(3.500f, 1.858f, 20.130f),
                Vector3(3.680f, 1.189f, 19.150f),
                Vector3(7.100f, 1.777f, 19.150f),
                Vector3(7.150f, 2.776f, 20.330f),
                Vector3(9.150f, 4.180f, 19.770f),
                Vector3(7.100f, 1.777f, 19.150f),
                Vector3(9.150f, 4.180f, 19.770f),
                Vector3(9.150f, 2.675f, 19.150f),
                Vector3(12.680f, 3.505f, 19.150f),
                Vector3(12.500f, 5.476f, 20.130f),
                Vector3(15.500f, 4.828f, 20.330f),
                Vector3(12.680f, 3.505f, 19.150f),
                Vector3(15.500f, 4.828f, 20.330f),
                Vector3(16.100f, 3.090f, 19.150f),
                Vector3(18.150f, 2.675f, 19.150f),
                Vector3(19.150f, 4.180f, 19.770f),
                Vector3(21.650f, 4.612f, 20.130f),
                Vector3(18.150f, 2.675f, 19.150f),
                Vector3(21.650f, 4.612f, 20.130f),
                Vector3(22.180f, 2.952f, 19.150f)
            };
            ArrayW<int> triangles(324);
            for (int i = 0; i < 324; ++i) triangles[i] = i;
            mesh->set_vertices(vertices);
            mesh->set_triangles(triangles);
            mesh->RecalculateNormals();
            frontRange->AddComponent<MeshFilter*>()->set_sharedMesh(mesh);
            frontRange->AddComponent<MeshRenderer*>()->set_material(
                whiteSphere->GetComponent<MeshRenderer*>()->material);
        }

        // Preserve the smaller foreground bank and its ground collision.
        CreateSphere(whiteSphere, Vector3(-25.8f, -0.7f, 7.0f), Vector3(10.0f, 2.0f, 10.0f)); // 19

        // Remove the template spheres
        GameObject::Destroy(whiteSphere);
        GameObject::Destroy(blackSphere);

    }

    void DestroySnowPrimitives() {
        if (!snowPrimitives) return;
        snowPrimitives->SetActive(false);
        GameObject::Destroy(snowPrimitives.ptr());
        snowPrimitives = nullptr;
    }

}