/*
 * BSD 2-Clause License
 *
 * Copyright (c) 2025, Christoph Neuhauser
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <cstring>
#include <Utils/AppSettings.hpp>
#include <Utils/File/FileUtils.hpp>
#include <Utils/File/Logfile.hpp>
#include <Graphics/Vulkan/Utils/Device.hpp>
#include <Graphics/Vulkan/Utils/Swapchain.hpp>

#include <Graphics/OpenGL/Context/OffscreenContext.hpp>
#ifdef _WIN32
#include <Graphics/OpenGL/Context/DeviceSelectionWGL.hpp>
#include <Graphics/OpenGL/Context/DeviceSelectionWGLGlobals.hpp>
#endif

#include "MainApp.hpp"

int main(int argc, char *argv[]) {
    sgl::FileUtils::get()->initialize("TestInteropVKGL", argc, argv);
#ifdef DATA_PATH
    if (!sgl::FileUtils::get()->directoryExists("Data") && !sgl::FileUtils::get()->directoryExists("../Data")) {
        sgl::AppSettings::get()->setDataDirectory(DATA_PATH);
    }
#endif
    sgl::AppSettings::get()->initializeDataDirectory();

    std::string settingsFile = sgl::FileUtils::get()->getConfigDirectory() + "settings.txt";
    sgl::AppSettings::get()->loadSettings(settingsFile.c_str());
    sgl::AppSettings::get()->getSettings().addKeyValue("window-multisamples", 0);
    sgl::AppSettings::get()->getSettings().addKeyValue("window-debugContext", true);
    sgl::AppSettings::get()->getSettings().addKeyValue("window-vSync", true);
    sgl::AppSettings::get()->getSettings().addKeyValue("window-resizable", true);
    sgl::AppSettings::get()->getSettings().addKeyValue("window-savePosition", true);
    sgl::AppSettings::get()->setLoadGUI(nullptr, true, false);
    sgl::AppSettings::get()->setRenderSystem(sgl::RenderSystem::VULKAN);

    sgl::AppSettings::get()->enableVulkanOffscreenOpenGLContextInteropSupport();
    auto* window = sgl::AppSettings::get()->createWindow();

    std::vector<const char*> optionalDeviceExtensions;
    if (sgl::AppSettings::get()->getInstanceSupportsVulkanOpenGLInterop()) {
        std::vector<const char*> interopDeviceExtensions =
                sgl::AppSettings::get()->getVulkanOpenGLInteropDeviceExtensions();
        for (const char* extensionName : interopDeviceExtensions) {
            bool foundExtension = false;
            for (size_t i = 0; i < optionalDeviceExtensions.size(); i++) {
                if (strcmp(extensionName, optionalDeviceExtensions.at(i)) == 0) {
                    foundExtension = true;
                    break;
                }
            }
            if (!foundExtension) {
                optionalDeviceExtensions.push_back(extensionName);
            }
        }
    }

    sgl::vk::Instance* instance = sgl::AppSettings::get()->getVulkanInstance();
    auto* device = new sgl::vk::Device;
    sgl::vk::DeviceFeatures requestedDeviceFeatures{};
    device->setUseAppDeviceSelector();
    device->createDeviceSwapchain(
            instance, window, {
                    VK_EXT_SCALAR_BLOCK_LAYOUT_EXTENSION_NAME, VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME
            },
            optionalDeviceExtensions, requestedDeviceFeatures);

    sgl::OffscreenContext* offscreenContext = nullptr;
    sgl::OffscreenContextParams params{};
#ifdef _WIN32
    sgl::attemptForceWglContextForVulkanDevice(
            device, &NvOptimusEnablement, &AmdPowerXpressRequestHighPerformance);
#endif
    offscreenContext = sgl::createOffscreenContext(device, params, false);
    if (offscreenContext && offscreenContext->getIsInitialized()) {
        sgl::AppSettings::get()->setOffscreenContext(offscreenContext);
    }

    auto* swapchain = new sgl::vk::Swapchain(device);
    swapchain->create(window);
    sgl::AppSettings::get()->setSwapchain(swapchain);

    sgl::AppSettings::get()->setPrimaryDevice(device);
    sgl::AppSettings::get()->initializeSubsystems();

    auto app = new MainApp();
    app->run();
    delete app;

    sgl::AppSettings::get()->release();

    return 0;
}
