#include "Controllers/vkw_scene_controller.h"
#include "SceneGraph/scene1.h"
#include "SceneGraph/camera.h"
#include "SceneGraph/light.h"
#include "SceneGraph/shape.h"
#include "SceneGraph/material.h"
#include "SceneGraph/texture.h"
#include "SceneGraph/Collector/collector.h"
#include "SceneGraph/iterator.h"
#include "SceneGraph/uberv2material.h"
#include "SceneGraph/inputmaps.h"
#include "Utils/distribution1d.h"
#include "Utils/log.h"


#include <chrono>
#include <memory>
#include <stack>
#include <vector>
#include <array>

using namespace RadeonRays;

namespace Baikal
{
    // Convert Light:: types to VkwScene:: types
    static int GetLightType(Light const& light)
    {
        if (dynamic_cast<PointLight const*>(&light))
        {
            return VkwScene::kPoint;
        }
        else if (dynamic_cast<DirectionalLight const*>(&light))
        {
            return VkwScene::kDirectional;
        }
        else if (dynamic_cast<SpotLight const*>(&light))
        {
            return VkwScene::kSpot;
        }
        else if (dynamic_cast<ImageBasedLight const*>(&light))
        {
            return VkwScene::kIbl;
        }
        else
        {
            return VkwScene::kArea;
        }
    }

    VkwSceneController::VkwSceneController(vkw::MemoryAllocator& memory_allocator, vkw::MemoryManager& memory_manager, VkDevice device, VkPhysicalDevice physical_device, uint32_t queue_family_index)
    :
    memory_allocator_(memory_allocator)
    , memory_manager_(memory_manager)
    , device_(device)
    , physical_device_(physical_device)
    , default_material_(UberV2Material::Create())
    {
    }

    Material::Ptr VkwSceneController::GetDefaultMaterial() const
    {
        return default_material_;
    }

    VkwSceneController::~VkwSceneController()
    {
    }

    void VkwSceneController::UpdateCamera(Scene1 const& scene, Collector& mat_collector, Collector& tex_collector, Collector& vol_collector, VkwScene& out) const
    {
        PerspectiveCamera* camera = dynamic_cast<PerspectiveCamera*>(scene.GetCamera().get());

        if (camera == nullptr)
        {
            throw std::runtime_error("VkwSceneController supports only perspective camera");
        }

        const float focal_length = camera->GetFocalLength();
        const float2 sensor_size = camera->GetSensorSize();

        float2 z_range = camera->GetDepthRange();

        // Nan-avoidance in perspective matrix
        z_range.x = std::max(z_range.x, std::numeric_limits<float>::epsilon());

        const float fovy = atan(sensor_size.y / (2.0f * focal_length));

        const float3 up = camera->GetUpVector();
        const float3 right = -camera->GetRightVector();
        const float3 forward = camera->GetForwardVector();
        const float3 pos = camera->GetPosition();

        const matrix proj = perspective_proj_fovy_rh_gl(fovy, camera->GetAspectRatio(), z_range.x, z_range.y);
        const float3 ip = float3(-dot(right, pos), -dot(up, pos), -dot(forward, pos));

        const matrix view = matrix( right.x, right.y, right.z, ip.x,
                                    up.x, up.y, up.z, ip.y,
                                    forward.x, forward.y, forward.z, ip.z,
                                    0.0f, 0.0f, 0.0f, 1.0f);

        const matrix view_proj = proj * view;

        VkwScene::Camera camera_internal;
        camera_internal.camera_position = camera->GetPosition();
        camera_internal.view_projection = view_proj;
        camera_internal.inv_view        = inverse(view);
        camera_internal.inv_projection  = inverse(proj);

        if (out.camera == VK_NULL_HANDLE)
        {
            out.camera = memory_manager_.CreateBuffer( sizeof(VkwScene::Camera),
                                                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                                        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, &camera_internal);
        }
        
        memory_manager_.WriteBuffer(out.camera.get(), 0u, sizeof(VkwScene::Camera), &camera_internal);
    }

    void VkwSceneController::UpdateShapes(Scene1 const& scene, Collector& mat_collector, Collector& tex_collector, Collector& vol_collector, VkwScene& out) const
    {
        size_t num_shapes = scene.GetNumShapes();
        size_t num_vertices = 0;
        size_t num_indices = 0;

        vertex_buffer_.clear();
        index_buffer_.clear();
        mesh_transforms_.clear();
        out.meshes.clear();

        int mat_id = 0;

        std::unique_ptr<Iterator> mesh_iter(scene.CreateShapeIterator());
        for (; mesh_iter->IsValid(); mesh_iter->Next())
        {
            Baikal::Shape::Ptr shape = mesh_iter->ItemAs<Baikal::Shape>();
            Baikal::Mesh const* mesh = dynamic_cast<Baikal::Mesh*>(shape.get());

            if (mesh == nullptr)
                continue;
            
            // TODO: desc sets and roughness, metallic, diffuse
            VkwScene::MaterialConstants constants;
            constants.data[0] = mat_id++;
 
            VkwScene::VkwMesh vkw_mesh = { static_cast<uint32_t>(num_indices), static_cast<uint32_t>(mesh->GetNumIndices()), VK_NULL_HANDLE, constants };
            out.meshes.push_back(vkw_mesh);

            num_vertices += mesh->GetNumVertices();
            num_indices  += mesh->GetNumIndices();

            assert(mesh->GetNumVertices() == mesh->GetNumNormals());
            assert(mesh->GetNumVertices() == mesh->GetNumUVs());

            for (std::size_t v = 0; v < mesh->GetNumVertices(); v++)
            {
                Vertex vertex = { mesh->GetVertices()[v], mesh->GetNormals()[v], mesh->GetUVs()[v] };
                vertex_buffer_.push_back(vertex);
            }

            for (std::size_t idx = 0; idx < mesh->GetNumIndices(); idx++)
            {
                index_buffer_.push_back(mesh->GetIndices()[idx]);
            }

            mesh_transforms_.push_back(mesh->GetTransform());
        }
        
        if (out.vertex_count < num_vertices)
        {
            out.mesh_vertex_buffer.reset();
            out.mesh_vertex_buffer = memory_manager_.CreateBuffer(  sizeof(Vertex) * num_vertices,
                                                                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                                                    VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, nullptr);

            out.vertex_count = num_vertices;
        }

        if (out.index_count < num_indices)
        {
            out.mesh_index_buffer.reset();
            
            out.mesh_index_buffer = memory_manager_.CreateBuffer( sizeof(uint32_t) * num_indices,
                                                                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                                                VK_BUFFER_USAGE_INDEX_BUFFER_BIT, nullptr);

            out.index_count = num_indices;
        }

        if (out.shapes_count < num_shapes)
        {
            out.mesh_transforms.reset();

            out.mesh_transforms = memory_manager_.CreateBuffer( sizeof(RadeonRays::matrix) * num_shapes,
                                                                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                                                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, nullptr);

            out.shapes_count = num_shapes;
        }

        memory_manager_.WriteBuffer(out.mesh_vertex_buffer.get(), 0u, sizeof(Vertex) * num_vertices, &vertex_buffer_[0]);
        memory_manager_.WriteBuffer(out.mesh_index_buffer.get(), 0u, sizeof(uint32_t) * num_indices, &index_buffer_[0]);
        memory_manager_.WriteBuffer(out.mesh_transforms.get(), 0u, sizeof(RadeonRays::matrix) * num_shapes, &mesh_transforms_[0]);
    }

    void VkwSceneController::UpdateShapeProperties(Scene1 const& scene, Collector& mat_collector, Collector& tex_collector, Collector& volume_collector, VkwScene& out) const
    {
        size_t num_shapes = scene.GetNumShapes();

        mesh_transforms_.clear();

        std::unique_ptr<Iterator> mesh_iter(scene.CreateShapeIterator());
        for (; mesh_iter->IsValid(); mesh_iter->Next())
        {
            Baikal::Shape::Ptr shape = mesh_iter->ItemAs<Baikal::Shape>();
            Baikal::Mesh const* mesh = dynamic_cast<Baikal::Mesh*>(shape.get());

            if (mesh == nullptr)
                continue;

            mesh_transforms_.push_back(mesh->GetTransform());
        }

        if (out.shapes_count < num_shapes)
        {
            out.mesh_transforms.reset();

            out.mesh_transforms = memory_manager_.CreateBuffer( sizeof(RadeonRays::matrix) * num_shapes,
                                                                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                                                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, nullptr);

            out.shapes_count = num_shapes;
        }

        memory_manager_.WriteBuffer(out.mesh_transforms.get(), 0u, sizeof(RadeonRays::matrix) * num_shapes, &mesh_transforms_[0]);
    }

    void VkwSceneController::UpdateCurrentScene(Scene1 const& scene, VkwScene& out) const
    {
    }

    void VkwSceneController::UpdateMaterials(Scene1 const& scene, Collector& mat_collector, Collector& tex_collector, VkwScene& out) const
    {
    }

    void VkwSceneController::UpdateVolumes(Scene1 const& scene, Collector& volume_collector, Collector& tex_collector, VkwScene& out) const
    {
    }


    void VkwSceneController::UpdateTextures(Scene1 const& scene, Collector& mat_collector, Collector& tex_collector, VkwScene& out) const
    {
        // Get new buffer size
        std::size_t tex_buffer_size = tex_collector.GetNumItems();
return;
        if (tex_buffer_size == 0)
        {
            return;
        }

        // Resize texture cache if needed
        if (out.textures.size() < tex_buffer_size)
        {
            out.textures.resize(tex_buffer_size);
        }

        // Create texture iterator
        std::unique_ptr<Iterator> tex_iter(tex_collector.CreateIterator());

        // Iterate and serialize
        std::size_t num_textures_written = 0;
        for (; tex_iter->IsValid(); tex_iter->Next())
        {
            auto tex = tex_iter->ItemAs<Texture>();

            WriteTexture(*tex, out.textures[num_textures_written]);

            ++num_textures_written;
        }
    }

    void VkwSceneController::WriteMaterial(Material const& material, Collector& mat_collector, Collector& tex_collector, void* data) const
    {
    }

    void VkwSceneController::WriteLight(Scene1 const& scene, Light const& light, Collector& tex_collector, void* data) const
    {
    }

    void VkwSceneController::UpdateLights(Scene1 const& scene, Collector& mat_collector, Collector& tex_collector, VkwScene& out) const
    {
        auto num_lights = scene.GetNumLights();

        std::vector<VkwScene::Light> lights_internal(static_cast<size_t>(num_lights));

        std::unique_ptr<Iterator> light_iter(scene.CreateLightIterator());
        
        for (; light_iter->IsValid(); light_iter->Next())
        {
            auto light = light_iter->ItemAs<Light>();

            auto light_type = GetLightType(*light);

            switch (light_type)
            {
                case VkwScene::kSpot:
                {
                    VkwScene::Light spot_light = { light->GetPosition(), light->GetDirection(), light->GetEmittedRadiance() };
                    lights_internal.push_back(spot_light);

                    break;
                };

                default:
                {
                    break;
                };
            }
        }

        if (out.lights == VK_NULL_HANDLE || out.light_count < num_lights)
        {
            out.lights.reset();

            out.lights = memory_manager_.CreateBuffer( sizeof(VkwScene::Light) * num_lights,
                                                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                                        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, &lights_internal);
        }

        out.light_count = num_lights;

        memory_manager_.WriteBuffer(out.lights.get(), 0u, sizeof(VkwScene::Light) * num_lights, &lights_internal);
    }


    VkFormat GetTextureFormat(const Texture &texture)
    {
        switch (texture.GetFormat())
        {
            case Texture::Format::kRgba8: return VK_FORMAT_R8G8B8A8_UNORM;
            case Texture::Format::kRgba16: return VK_FORMAT_R16G16B16A16_UNORM;
            case Texture::Format::kRgba32: return VK_FORMAT_R32G32B32A32_SFLOAT;
            default: return VK_FORMAT_R8G8B8A8_UNORM;
        }
    }

    void VkwSceneController::WriteTexture(Texture const& texture, vkw::Texture &vk_texture) const
    {
        auto sz = texture.GetSize();
        VkExtent3D size;
        size.width = sz.x;
        size.height = sz.y;
        size.depth = sz.z;

        VkFormat fmt = GetTextureFormat(texture);

        vk_texture.SetTexture(&memory_manager_, size, fmt, texture.GetData());
    }

    void VkwSceneController::WriteVolume(VolumeMaterial const& volume, Collector& tex_collector, void* data) const
    {
    }

    void VkwSceneController::UpdateSceneAttributes(Scene1 const& scene, Collector& tex_collector, VkwScene& out) const
    {
    }

    void Baikal::VkwSceneController::UpdateInputMaps(const Baikal::Scene1& scene, Baikal::Collector& input_map_collector, Collector& input_map_leafs_collector, VkwScene& out) const
    {
    }

    void Baikal::VkwSceneController::UpdateLeafsData(Scene1 const& scene, Collector& input_map_leafs_collector, Collector& tex_collector, VkwScene& out) const
    {
    }

    void Baikal::VkwSceneController::WriteInputMapLeaf(InputMap const& leaf, Collector& tex_collector, void* data) const
    {
    }
    
}
