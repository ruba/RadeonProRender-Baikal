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
#ifndef CL_CONFIG_MANAGER_H
#define CL_CONFIG_MANAGER_H

#include "config_manager.h"

#include "CLW.h"

#include "RenderFactory/clw_render_factory.h"
#include "Renderers/renderer.h"

#include <vector>
#include <memory>

class ClConfigManager : public ConfigManager
{
public:
    struct ClConfig
    {
        DeviceType type;

        ClConfig() = default;
        ClConfig(ClConfig&& cfg) = default;

        std::unique_ptr<Baikal::RenderFactory<Baikal::ClwScene>> factory_;
        std::unique_ptr<Baikal::Renderer<Baikal::ClwScene>> renderer_;
        std::unique_ptr<Baikal::SceneController<Baikal::ClwScene>> controller_;

        CLWContext context;
        bool caninterop;
    };

    static void CreateConfigs(
        Mode mode,
        bool interop,
        std::vector<ClConfig>& renderers,
        int initial_num_bounces,
        int req_platform_index = -1,
        int req_device_index = -1);

private:

};

#endif // CL_CONFIG_MANAGER_H
