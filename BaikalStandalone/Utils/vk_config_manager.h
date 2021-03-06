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
#ifndef VK_CONFIG_MANAGER_H
#define VK_CONFIG_MANAGER_H

#include "config_manager.h"

#include "VKW.h"

#include "RenderFactory/vkw_render_factory.h"
#include "Renderers/renderer.h"

#include <vector>
#include <memory>

class VkConfigManager : public ConfigManager
{
public:

    struct VkConfig
    {
        DeviceType type;

        VkConfig() = default;
        VkConfig(VkConfig&& cfg) = default;

        vkw::VkScopedObject<VkInstance> instance_;
        vkw::VkScopedObject<VkDevice>   device_;

        std::unique_ptr<vkw::MemoryAllocator>       memory_allocator_;
        std::unique_ptr<vkw::MemoryManager>         memory_manager_;
        std::unique_ptr<vkw::RenderTargetManager>   render_target_manager_;
        std::unique_ptr<vkw::ShaderManager>         shader_manager_;
        std::unique_ptr<vkw::DescriptorManager>     descriptor_manager_;
        std::unique_ptr<vkw::PipelineManager>       pipeline_manager_;
        std::unique_ptr<vkw::Utils>                 utils_;


        std::unique_ptr<Baikal::RenderFactory<Baikal::VkwScene>> factory_;
        std::unique_ptr<Baikal::Renderer<Baikal::VkwScene>> renderer_;
        std::unique_ptr<Baikal::SceneController<Baikal::VkwScene>> controller_;

        VkPhysicalDevice                physical_device_;

        uint32_t                        compute_queue_family_idx_;
        uint32_t                        graphics_queue_family_idx_;
    };

    static void CreateConfig(VkConfig& renderers, const std::vector<const char*> &requested_extensions);

private:
    static vkw::VkScopedObject<VkInstance> CreateInstance(const std::vector<const char*> &requested_extensions);

    static vkw::VkScopedObject<VkDevice> CreateDevice(VkInstance instance
        , std::uint32_t& compute_queue_family_index
        , std::uint32_t& graphics_queue_family_index
        , VkPhysicalDevice* opt_physical_device = nullptr);
};

#endif // VK_CONFIG_MANAGER_H
