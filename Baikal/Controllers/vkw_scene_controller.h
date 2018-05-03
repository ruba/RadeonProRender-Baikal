/**********************************************************************
 Copyright (c) 2016 Advanced Micro Devices, Inc. All rights reserved.

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 ********************************************************************/

/**
 \file vkw_scene_controller.h
 \author Dmytro Rubalskyi
 \version 1.0
 \brief Contains VkSceneController class implementation.
 */
#pragma once

#include "scene_controller.h"

#include "SceneGraph/vkwscene.h"

#include "math/float3.h"

#include "VKW.h"

namespace Baikal
{
    class Scene1;
    class Collector;
    class Bundle;
    class Material;
    class Light;
    class Texture;


    /**
     \brief Tracks changes of a scene and serialized data into GPU memory when needed.

     VkSceneController class is intended to keep track of CPU side scene changes and update all
     necessary GPU buffers. It essentially establishes a mapping between Scene class and
     corresponding VkwScene class. It also pre-caches VkwScenes and speeds up loading for
     already compiled scenes.
     */
    class VkwSceneController : public SceneController<VkwScene>
    {
    public:
        // Constructor
        VkwSceneController(VkDevice device, VkPhysicalDevice physical_device, int queue_family_index);
        // Destructor
        virtual ~VkwSceneController();

    public:
        // Update camera data only.
        void UpdateCamera(Scene1 const& scene, Collector& mat_collector, Collector& tex_collector, Collector& vol_collector, VkwScene& out) const override;
        // Update shape data only.
        void UpdateShapes(Scene1 const& scene, Collector& mat_collector, Collector& tex_collector, Collector& vol_collector, VkwScene& out) const override;
        // Update transform data only
        void UpdateShapeProperties(Scene1 const& scene, Collector& mat_collector, Collector& tex_collector, Collector& volume_collector, VkwScene& out) const override;
        // Update lights data only.
        void UpdateLights(Scene1 const& scene, Collector& mat_collector, Collector& tex_collector, VkwScene& out) const override;
        // Update material data.
        void UpdateMaterials(Scene1 const& scene, Collector& mat_collector, Collector& tex_collector, VkwScene& out) const override;
        // Update texture data only.
        void UpdateTextures(Scene1 const& scene, Collector& mat_collector, Collector& tex_collector, VkwScene& out) const override;
        // Update input maps only
        void UpdateInputMaps(Scene1 const& scene, Collector& input_map_collector, Collector& input_map_leafs_collector, VkwScene& out) const;
        // Update input map leafs only
        void UpdateLeafsData(Scene1 const& scene, Collector& input_map_leafs_collector, Collector& tex_collector, VkwScene& out) const;
        // Get default material
        Material::Ptr GetDefaultMaterial() const override;
        // If m_current_scene changes
        void UpdateCurrentScene(Scene1 const& scene, VkwScene& out) const override;
        // Update volume materiuals
        void UpdateVolumes(Scene1 const& scene, Collector& volume_collector, Collector& tex_collector, VkwScene& out) const override;
        // If scene attributes changed
        void UpdateSceneAttributes(Scene1 const& scene, Collector& tex_collector, VkwScene& out) const override;
        // Update intersection API
        void UpdateIntersector(Scene1 const& scene, VkwScene& out) const;
        void UpdateIntersectorTransforms(Scene1 const& scene, VkwScene& out) const;
        // Write out single material at data pointer.
        // Collectors are required to convert texture and material pointers into indices.
        void WriteMaterial(Material const& material, Collector& mat_collector, Collector& tex_collector, void* data) const;
        // Write out single light at data pointer.
        // Collector is required to convert texture pointers into indices.
        void WriteLight(Scene1 const& scene, Light const& light, Collector& tex_collector, void* data) const;
        // Write out single texture header at data pointer.
        // Header requires texture data offset, so it is passed in.
        void WriteTexture(Texture const& texture, std::size_t data_offset, void* data) const;
        // Write out texture data at data pointer.
        void WriteTextureData(Texture const& texture, void* data) const;
        // Write single volume at data pointer
        void WriteVolume(VolumeMaterial const& volume, Collector& tex_collector, void* data) const;
        // Write single input map leaf at data pointer
        // Collectore is required to convert texture pointers into indices.
        void WriteInputMapLeaf(InputMap const& leaf, Collector& tex_collector, void* data) const;
    protected:
        // Memory allocator
        vkw::MemoryAllocator    m_memory_allocator;
        // Memory manager
        vkw::MemoryManager      m_memory_manager;
        // Vulkan logical device
        VkDevice                m_device;
        // Vulkan physical device
        VkPhysicalDevice        m_physical_device;
        // Default material
        Material::Ptr           m_default_material;
    };
}
