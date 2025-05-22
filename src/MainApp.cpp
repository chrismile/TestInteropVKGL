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

#include <Graphics/Vulkan/Utils/Device.hpp>
#include <Graphics/Vulkan/Image/Image.hpp>
#include <Graphics/Vulkan/Render/Renderer.hpp>

#include "DiagramBase.hpp"
#include "MainApp.hpp"

MainApp::MainApp() {
    useDockSpaceMode = false;
    useLinearRGB = false;
    diagram = new DiagramBase;
    diagram->setRendererVk(rendererVk);
    diagram->initialize();
    diagram->onWindowSizeChanged();
    resolutionChanged(sgl::EventPtr());
}

MainApp::~MainApp() {
    device->waitIdle();
    delete diagram;
}

void MainApp::render() {
    SciVisApp::preRender();
    SciVisApp::prepareReRender();
    diagram->render();
    diagram->setBlitTargetSupersamplingFactor(1);
    diagram->blitToTargetVk();
    SciVisApp::postRender();
}

void MainApp::renderGui() {
    if (ImGui::Begin("Info")) {
        renderGuiFpsCounter();
        ImGui::End();
    }
}

void MainApp::update(float dt) {
    sgl::SciVisApp::update(dt);
    int mouseHoverWindowIndex = -1;
    ImGuiIO &io = ImGui::GetIO();
    bool hasGrabbedMouse = io.WantCaptureMouse && mouseHoverWindowIndex < 0;
    diagram->setIsMouseGrabbedByParent(hasGrabbedMouse);
    diagram->update(dt);
}

void MainApp::resolutionChanged(sgl::EventPtr event) {
    SciVisApp::resolutionChanged(event);
    diagram->setBlitTargetVk(
            sceneTextureVk->getImageView(),
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    bool alignWithParentWindow = true;
    if (alignWithParentWindow) {
        diagram->setBlitTargetSupersamplingFactor(1);
        diagram->updateSizeByParent();
    }
}
