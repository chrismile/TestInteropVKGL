/*
 * BSD 2-Clause License
 *
 * Copyright (c) 2021-2023, Christoph Neuhauser
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

#include <iostream>
#include <random>

#ifdef SUPPORT_SKIA
#include <core/SkCanvas.h>
#include <core/SkPaint.h>
#endif
#ifdef SUPPORT_VKVG
#include <vkvg.h>
#endif

#include <Math/Geometry/AABB2.hpp>
#include <Utils/AppSettings.hpp>
#include <Input/Mouse.hpp>
#include <Math/Math.hpp>
#include <Graphics/Vector/VectorBackendNanoVG.hpp>
#include <Graphics/Vector/nanovg/nanovg.h>
#include <ImGui/ImGuiWrapper.hpp>

#include "BSpline.hpp"
#include "DiagramBase.hpp"

DiagramBase::DiagramBase() {
    sgl::NanoVGSettings nanoVgSettings{};
    nanoVgSettings.renderBackend = sgl::RenderSystem::OPENGL;
    registerRenderBackendIfSupported<sgl::VectorBackendNanoVG>([this]() { this->renderBaseNanoVG(); }, nanoVgSettings);
}

void DiagramBase::initialize() {
    borderSizeX = 10;
    borderSizeY = 10;
    windowWidth = (200 + borderSizeX) * 2.0f;
    windowHeight = (200 + borderSizeY) * 2.0f;
    _initialize();

    const int numPoints = 25;
    nodesList.resize(numPoints);
    for (int i = 0; i < numPoints; i++) {
        float angle = sgl::TWO_PI * (float)i / (float)numPoints;
        nodesList[i].normalizedPosition = glm::vec2(std::cos(angle), std::sin(angle));
    }

    std::vector<glm::vec2> controlPoints;
    numLinesTotal = numPoints * numPoints;
    curvePoints.resize(numLinesTotal * NUM_SUBDIVISIONS);
    for (int lineIdx = 0; lineIdx < numLinesTotal; lineIdx++) {
        int i = lineIdx / numPoints;
        int j = lineIdx % numPoints;
        glm::vec2 pt0 = nodesList[i].normalizedPosition;
        glm::vec2 pt1 = nodesList[j].normalizedPosition;
        glm::vec2 ptx = glm::vec2(-0.1f, 0.1f);
        glm::vec2 pty = glm::vec2(0.1f, -0.1f);
        controlPoints.clear();
        controlPoints.push_back(pt0);
        controlPoints.push_back(ptx);
        controlPoints.push_back(pty);
        controlPoints.push_back(pt1);

        for (int ptIdx = 0; ptIdx < NUM_SUBDIVISIONS; ptIdx++) {
            float t = float(ptIdx) / float(NUM_SUBDIVISIONS - 1);
            int k = 4;
            if (controlPoints.size() == 3) {
                k = 3;
            }
            curvePoints.at(lineIdx * NUM_SUBDIVISIONS + ptIdx) = evaluateBSpline(t, k, controlPoints);
        }
    }
}

void DiagramBase::onBackendCreated() {
}

void DiagramBase::onBackendDestroyed() {
    vg = nullptr;
}

void DiagramBase::setImGuiWindowOffset(int offsetX, int offsetY) {
    imGuiWindowOffsetX = offsetX;
    imGuiWindowOffsetY = offsetY;
}

void DiagramBase::setClearColor(const sgl::Color& clearColor) {
    float r = clearColor.getFloatR();
    float g = clearColor.getFloatG();
    float b = clearColor.getFloatB();
    float clearColorLuminance = 0.2126f * r + 0.7152f * g + 0.0722f * b;
    isDarkMode = clearColorLuminance <= 0.5f;
}

void DiagramBase::setIsMouseGrabbedByParent(bool _isMouseGrabbedByParent) {
    isMouseGrabbedByParent = _isMouseGrabbedByParent;
}

void DiagramBase::updateSizeByParent() {
    auto [parentWidth, parentHeight] = getBlitTargetSize();
    auto ssf = float(blitTargetSupersamplingFactor);
    windowOffsetX = 0;
    windowOffsetY = 0;
    windowWidth = float(parentWidth) / (scaleFactor * float(ssf));
    windowHeight = float(parentHeight) / (scaleFactor * float(ssf));
    onUpdatedWindowSize();
    onWindowSizeChanged();
}

void DiagramBase::update(float dt) {
    glm::ivec2 mousePositionPx(sgl::Mouse->getX(), sgl::Mouse->getY());
    glm::vec2 mousePosition(sgl::Mouse->getX(), sgl::Mouse->getY());
    if (sgl::ImGuiWrapper::get()->getUseDockSpaceMode()) {
        mousePosition -= glm::vec2(imGuiWindowOffsetX, imGuiWindowOffsetY);
        mousePositionPx -= glm::ivec2(imGuiWindowOffsetX, imGuiWindowOffsetY);
    }
    mousePosition -= glm::vec2(getWindowOffsetX(), getWindowOffsetY());
    mousePosition /= getScaleFactor();

    bool isMouseOverDiagram = getIsMouseOverDiagram(mousePositionPx) && !isMouseGrabbedByParent;
    windowMoveOrResizeJustFinished = false;

    // Mouse press event.
    if (isMouseOverDiagram && !isWindowFixed) {
        if (sgl::Mouse->buttonPressed(1)) {
            isMouseGrabbed = true;
        }
        mousePressEventResizeWindow(mousePositionPx, mousePosition);
        mousePressEventMoveWindow(mousePositionPx, mousePosition);
    }

    // Mouse move event.
    if (sgl::Mouse->mouseMoved()) {
        if (isMouseOverDiagram || isMouseGrabbed) {
            mouseMoveEvent(mousePositionPx, mousePosition);
        } else {
            mouseMoveEventParent(mousePositionPx, mousePosition);
        }
    }

    // Mouse release event.
    if (sgl::Mouse->buttonReleased(1)) {
        checkWindowMoveOrResizeJustFinished(mousePositionPx);
        resizeDirection = ResizeDirection::NONE;
        isDraggingWindow = false;
        isResizingWindow = false;
        isMouseGrabbed =  false;
    }
}

void DiagramBase::checkWindowMoveOrResizeJustFinished(const glm::ivec2& mousePositionPx) {
    bool dragFinished =
            isDraggingWindow && (mousePositionPx.x - mouseDragStartPosX || mousePositionPx.y - mouseDragStartPosY);
    bool resizeFinished = isResizingWindow;
    if (dragFinished || resizeFinished) {
        windowMoveOrResizeJustFinished = true;
    }
}

bool DiagramBase::getIsMouseOverDiagramImGui() const {
    glm::ivec2 mousePositionPx(sgl::Mouse->getX(), sgl::Mouse->getY());
    if (sgl::ImGuiWrapper::get()->getUseDockSpaceMode()) {
        mousePositionPx -= glm::ivec2(imGuiWindowOffsetX, imGuiWindowOffsetY);
    }
    return getIsMouseOverDiagram(mousePositionPx);
}

void DiagramBase::mouseMoveEvent(const glm::ivec2& mousePositionPx, const glm::vec2& mousePositionScaled) {
    if (sgl::Mouse->buttonReleased(1)) {
        checkWindowMoveOrResizeJustFinished(mousePositionPx);
        resizeDirection = ResizeDirection::NONE;
        isDraggingWindow = false;
        isResizingWindow = false;
    }

    if (resizeDirection != ResizeDirection::NONE) {
        auto diffX = float(mousePositionPx.x - lastResizeMouseX);
        auto diffY = float(mousePositionPx.y - lastResizeMouseY);
        if ((resizeDirection & ResizeDirection::LEFT) != 0) {
            windowOffsetX += diffX;
            windowWidth -= diffX / scaleFactor;
        }
        if ((resizeDirection & ResizeDirection::RIGHT) != 0) {
            windowWidth += diffX / scaleFactor;
        }
        if ((resizeDirection & ResizeDirection::BOTTOM) != 0) {
            windowOffsetY += diffY;
            windowHeight -= diffY / scaleFactor;
        }
        if ((resizeDirection & ResizeDirection::TOP) != 0) {
            windowHeight += diffY / scaleFactor;
        }
        lastResizeMouseX = mousePositionPx.x;
        lastResizeMouseY = mousePositionPx.y;
        needsReRender = true;
        syncRendererWithCpu();
        onWindowSizeChanged();
        onUpdatedWindowSize();
    } else {
        glm::vec2 mousePosition(float(mousePositionPx.x), float(mousePositionPx.y));

        sgl::AABB2 leftAabb;
        leftAabb.min = glm::vec2(windowOffsetX, windowOffsetY);
        leftAabb.max = glm::vec2(windowOffsetX + resizeMargin, windowOffsetY + float(fboHeightDisplay));
        sgl::AABB2 rightAabb;
        rightAabb.min = glm::vec2(windowOffsetX + float(fboWidthDisplay) - resizeMargin, windowOffsetY);
        rightAabb.max = glm::vec2(windowOffsetX + float(fboWidthDisplay), windowOffsetY + float(fboHeightDisplay));
        sgl::AABB2 bottomAabb;
        bottomAabb.min = glm::vec2(windowOffsetX, windowOffsetY);
        bottomAabb.max = glm::vec2(windowOffsetX + float(fboWidthDisplay), windowOffsetY + resizeMargin);
        sgl::AABB2 topAabb;
        topAabb.min = glm::vec2(windowOffsetX, windowOffsetY + float(fboHeightDisplay) - resizeMargin);
        topAabb.max = glm::vec2(windowOffsetX + float(fboWidthDisplay), windowOffsetY + float(fboHeightDisplay));

        ResizeDirection resizeDirectionCurr = ResizeDirection::NONE;
        if (leftAabb.contains(mousePosition)) {
            resizeDirectionCurr = ResizeDirection(resizeDirectionCurr | ResizeDirection::LEFT);
        }
        if (rightAabb.contains(mousePosition)) {
            resizeDirectionCurr = ResizeDirection(resizeDirectionCurr | ResizeDirection::RIGHT);
        }
        if (bottomAabb.contains(mousePosition)) {
            resizeDirectionCurr = ResizeDirection(resizeDirectionCurr | ResizeDirection::BOTTOM);
        }
        if (topAabb.contains(mousePosition)) {
            resizeDirectionCurr = ResizeDirection(resizeDirectionCurr | ResizeDirection::TOP);
        }

        sgl::CursorType newCursorShape = sgl::CursorType::DEFAULT;
        if (resizeDirectionCurr == ResizeDirection::LEFT
                || resizeDirectionCurr == ResizeDirection::RIGHT) {
            newCursorShape = sgl::CursorType::SIZEWE;
        } else if (resizeDirectionCurr == ResizeDirection::BOTTOM
                || resizeDirectionCurr == ResizeDirection::TOP) {
            newCursorShape = sgl::CursorType::SIZENS;
        } else if (resizeDirectionCurr == ResizeDirection::BOTTOM_LEFT
                || resizeDirectionCurr == ResizeDirection::TOP_RIGHT) {
            newCursorShape = sgl::CursorType::SIZENESW;
        } else if (resizeDirectionCurr == ResizeDirection::TOP_LEFT
                || resizeDirectionCurr == ResizeDirection::BOTTOM_RIGHT) {
            newCursorShape = sgl::CursorType::SIZENWSE;
        } else {
            newCursorShape = sgl::CursorType::DEFAULT;
        }

        if (newCursorShape != cursorShape) {
            sgl::Window* window = sgl::AppSettings::get()->getMainWindow();
            cursorShape = newCursorShape;
            window->setCursorType(cursorShape);
        }
    }

    if (isDraggingWindow) {
        windowOffsetX = windowOffsetXBase + float(mousePositionPx.x - mouseDragStartPosX);
        windowOffsetY = windowOffsetYBase + float(mousePositionPx.y - mouseDragStartPosY);
        needsReRender = true;
    }
}

void DiagramBase::mouseMoveEventParent(const glm::ivec2& mousePositionPx, const glm::vec2& mousePositionScaled) {
    if (sgl::Mouse->isButtonUp(1)) {
        checkWindowMoveOrResizeJustFinished(mousePositionPx);
        resizeDirection = ResizeDirection::NONE;
        isDraggingWindow = false;
        isResizingWindow = false;
    }

    if (resizeDirection != ResizeDirection::NONE) {
        float diffX = float(mousePositionPx.x - lastResizeMouseX);
        float diffY = float(mousePositionPx.y - lastResizeMouseY);
        if ((resizeDirection & ResizeDirection::LEFT) != 0) {
            windowOffsetX += diffX;
            windowWidth -= diffX / scaleFactor;
        }
        if ((resizeDirection & ResizeDirection::RIGHT) != 0) {
            windowWidth += diffX / scaleFactor;
        }
        if ((resizeDirection & ResizeDirection::BOTTOM) != 0) {
            windowOffsetY += diffY;
            windowHeight -= diffY / scaleFactor;
        }
        if ((resizeDirection & ResizeDirection::TOP) != 0) {
            windowHeight += diffY / scaleFactor;
        }
        lastResizeMouseX = mousePositionPx.x;
        lastResizeMouseY = mousePositionPx.y;
        needsReRender = true;
        syncRendererWithCpu();
        onWindowSizeChanged();
        onUpdatedWindowSize();
    } else {
        if (cursorShape != sgl::CursorType::DEFAULT) {
            sgl::Window* window = sgl::AppSettings::get()->getMainWindow();
            cursorShape = sgl::CursorType::DEFAULT;
            window->setCursorType(cursorShape);
        }
    }

    if (isDraggingWindow) {
        windowOffsetX = windowOffsetXBase + float(mousePositionPx.x - mouseDragStartPosX);
        windowOffsetY = windowOffsetYBase + float(mousePositionPx.y - mouseDragStartPosY);
        needsReRender = true;
    }
}

void DiagramBase::mousePressEventResizeWindow(const glm::ivec2& mousePositionPx, const glm::vec2& mousePositionScaled) {
    if (sgl::Mouse->buttonPressed(1)) {
        // First, check if a resize event was started.
        glm::vec2 mousePosition(float(mousePositionPx.x), float(mousePositionPx.y));

        sgl::AABB2 leftAabb;
        leftAabb.min = glm::vec2(windowOffsetX, windowOffsetY);
        leftAabb.max = glm::vec2(windowOffsetX + resizeMargin, windowOffsetY + float(fboHeightDisplay));
        sgl::AABB2 rightAabb;
        rightAabb.min = glm::vec2(windowOffsetX + float(fboWidthDisplay) - resizeMargin, windowOffsetY);
        rightAabb.max = glm::vec2(windowOffsetX + float(fboWidthDisplay), windowOffsetY + float(fboHeightDisplay));
        sgl::AABB2 bottomAabb;
        bottomAabb.min = glm::vec2(windowOffsetX, windowOffsetY);
        bottomAabb.max = glm::vec2(windowOffsetX + float(fboWidthDisplay), windowOffsetY + resizeMargin);
        sgl::AABB2 topAabb;
        topAabb.min = glm::vec2(windowOffsetX, windowOffsetY + float(fboHeightDisplay) - resizeMargin);
        topAabb.max = glm::vec2(windowOffsetX + float(fboWidthDisplay), windowOffsetY + float(fboHeightDisplay));

        resizeDirection = ResizeDirection::NONE;
        if (leftAabb.contains(mousePosition)) {
            resizeDirection = ResizeDirection(resizeDirection | ResizeDirection::LEFT);
        }
        if (rightAabb.contains(mousePosition)) {
            resizeDirection = ResizeDirection(resizeDirection | ResizeDirection::RIGHT);
        }
        if (bottomAabb.contains(mousePosition)) {
            resizeDirection = ResizeDirection(resizeDirection | ResizeDirection::BOTTOM);
        }
        if (topAabb.contains(mousePosition)) {
            resizeDirection = ResizeDirection(resizeDirection | ResizeDirection::TOP);
        }

        if (resizeDirection != ResizeDirection::NONE) {
            isResizingWindow = true;
            lastResizeMouseX = mousePositionPx.x;
            lastResizeMouseY = mousePositionPx.y;
        }
    }
}

void DiagramBase::mousePressEventMoveWindow(const glm::ivec2& mousePositionPx, const glm::vec2& mousePositionScaled) {
    if (resizeDirection == ResizeDirection::NONE && sgl::Mouse->buttonPressed(1)) {
        isDraggingWindow = true;
        windowOffsetXBase = windowOffsetX;
        windowOffsetYBase = windowOffsetY;
        mouseDragStartPosX = mousePositionPx.x;
        mouseDragStartPosY = mousePositionPx.y;
    }
}


void DiagramBase::getNanoVGContext() {
    vg = static_cast<sgl::VectorBackendNanoVG*>(vectorBackend)->getContext();
}

void DiagramBase::renderBaseNanoVG() {
    getNanoVGContext();

    sgl::Color backgroundFillColor = isDarkMode ? backgroundFillColorDark : backgroundFillColorBright;
    sgl::Color backgroundStrokeColor = isDarkMode ? backgroundStrokeColorDark : backgroundStrokeColorBright;
    NVGcolor backgroundFillColorNvg = nvgRGBA(
            backgroundFillColor.getR(), backgroundFillColor.getG(),
            backgroundFillColor.getB(), std::clamp(int(backgroundOpacity * 255), 0, 255));
    NVGcolor backgroundStrokeColorNvg = nvgRGBA(
            backgroundStrokeColor.getR(), backgroundStrokeColor.getG(),
            backgroundStrokeColor.getB(), std::clamp(int(backgroundOpacity * 255), 0, 255));

    // Render the render target-filling widget rectangle.
    nvgBeginPath(vg);
    nvgRoundedRect(
            vg, borderWidth, borderWidth, windowWidth - 2.0f * borderWidth, windowHeight - 2.0f * borderWidth,
            borderRoundingRadius);
    nvgFillColor(vg, backgroundFillColorNvg);
    nvgFill(vg);
    if (renderBackgroundStroke) {
        nvgStrokeColor(vg, backgroundStrokeColorNvg);
        nvgStroke(vg);
    }

    /*NVGcolor testColor = nvgRGBA(255, 0, 0, 255);
    nvgBeginPath(vg);
    nvgRoundedRect(
            vg, borderWidth, borderWidth, windowWidth - 2.0f * borderWidth, windowHeight - 2.0f * borderWidth,
            borderRoundingRadius);
    nvgFillColor(vg, testColor);
    nvgFill(vg);*/
    renderChordDiagramNanoVG();
}


/// Removes trailing zeros and unnecessary decimal points.
std::string removeTrailingZeros(const std::string& numberString) {
    size_t lastPos = numberString.size();
    for (int i = int(numberString.size()) - 1; i > 0; i--) {
        char c = numberString.at(i);
        if (c == '.') {
            lastPos--;
            break;
        }
        if (c != '0') {
            break;
        }
        lastPos--;
    }
    return numberString.substr(0, lastPos);
}

/// Removes decimal points if more than maxDigits digits are used.
std::string DiagramBase::getNiceNumberString(float number, int digits) {
    int maxDigits = digits + 2; // Add 2 digits for '.' and one digit afterwards.
    std::string outString = removeTrailingZeros(sgl::toString(number, digits, true));

    // Can we remove digits after the decimal point?
    size_t dotPos = outString.find('.');
    if (int(outString.size()) > maxDigits && dotPos != std::string::npos) {
        size_t substrSize = dotPos;
        if (int(dotPos) < maxDigits - 1) {
            substrSize = maxDigits;
        }
        outString = outString.substr(0, substrSize);
    }

    // Still too large?
    if (int(outString.size()) > maxDigits || (outString == "0" && number > std::numeric_limits<float>::epsilon())) {
        outString = sgl::toString(number, std::max(digits - 2, 1), false, false, true);
    }
    return outString;
}


void DiagramBase::renderChordDiagramNanoVG() {
    if (windowWidth < 360.0f || windowHeight < 360.0f) {
        borderSizeX = borderSizeY = 10.0f;
    } else {
        borderSizeX = borderSizeY = std::min(windowWidth, windowHeight) / 36.0f;
    }
    float minDim = std::min(windowWidth - 2.0f * borderSizeX, windowHeight - 2.0f * borderSizeY);
    totalRadius = std::round(0.5f * minDim);
    if (showRing) {
        chartRadius = totalRadius * (1.0f - outerRingSizePct);
    } else {
        chartRadius = totalRadius;
    }
    outerRingWidth = totalRadius - chartRadius - outerRingOffset;
    chartRadius = totalRadius * (1.0f - outerRingSizePct);

    // Draw the B-spline curves.
    NVGcolor curveStrokeColor = nvgRGBA(
            100, 255, 100, uint8_t(std::clamp(int(std::ceil(curveOpacity * 255.0f)), 0, 255)));
    if (!curvePoints.empty()) {
        nvgStrokeWidth(vg, curveThickness);
        for (int lineIdx = 0; lineIdx < numLinesTotal; lineIdx++) {
            if (lineIdx == selectedLineIdx) {
                continue;
            }
            nvgBeginPath(vg);
            glm::vec2 pt0 = curvePoints.at(lineIdx * NUM_SUBDIVISIONS + 0);
            pt0.x = windowWidth / 2.0f + pt0.x * chartRadius;
            pt0.y = windowHeight / 2.0f + pt0.y * chartRadius;
            nvgMoveTo(vg, pt0.x, pt0.y);
            for (int ptIdx = 1; ptIdx < NUM_SUBDIVISIONS; ptIdx++) {
                glm::vec2 pt = curvePoints.at(lineIdx * NUM_SUBDIVISIONS + ptIdx);
                pt.x = windowWidth / 2.0f + pt.x * chartRadius;
                pt.y = windowHeight / 2.0f + pt.y * chartRadius;
                nvgLineTo(vg, pt.x, pt.y);
            }

            nvgStrokeColor(vg, curveStrokeColor);
            nvgStroke(vg);
        }

        if (selectedLineIdx >= 0) {
            // Background color outline.
            sgl::Color outlineColor = isDarkMode ? backgroundFillColorDark : backgroundFillColorBright;
            nvgStrokeWidth(vg, curveThickness * 3.0f);
            nvgBeginPath(vg);
            glm::vec2 pt0 = curvePoints.at(selectedLineIdx * NUM_SUBDIVISIONS + 0);
            pt0.x = windowWidth / 2.0f + pt0.x * chartRadius;
            pt0.y = windowHeight / 2.0f + pt0.y * chartRadius;
            nvgMoveTo(vg, pt0.x, pt0.y);
            for (int ptIdx = 1; ptIdx < NUM_SUBDIVISIONS; ptIdx++) {
                glm::vec2 pt = curvePoints.at(selectedLineIdx * NUM_SUBDIVISIONS + ptIdx);
                pt.x = windowWidth / 2.0f + pt.x * chartRadius;
                pt.y = windowHeight / 2.0f + pt.y * chartRadius;
                nvgLineTo(vg, pt.x, pt.y);
            }
            nvgStrokeColor(vg, nvgRGBA(
                    outlineColor.getR(), outlineColor.getG(), outlineColor.getB(), outlineColor.getA()));
            nvgStroke(vg);

            // Line itself.
            nvgStrokeWidth(vg, curveThickness * 2.0f);
            nvgBeginPath(vg);
            nvgMoveTo(vg, pt0.x, pt0.y);
            for (int ptIdx = 1; ptIdx < NUM_SUBDIVISIONS; ptIdx++) {
                glm::vec2 pt = curvePoints.at(selectedLineIdx * NUM_SUBDIVISIONS + ptIdx);
                pt.x = windowWidth / 2.0f + pt.x * chartRadius;
                pt.y = windowHeight / 2.0f + pt.y * chartRadius;
                nvgLineTo(vg, pt.x, pt.y);
            }
            curveStrokeColor.a = 1.0f;
            nvgStrokeColor(vg, curveStrokeColor);
            nvgStroke(vg);
        }
    }

    // Draw the point circles.
    float pointRadius = curveThickness * pointRadiusBase;
    nvgBeginPath(vg);
    for (int leafIdx = int(0); leafIdx < int(nodesList.size()); leafIdx++) {
        const auto& leaf = nodesList.at(leafIdx);
        int pointIdx = leafIdx - int(0);
        if (pointIdx == selectedPointIndices[0] || pointIdx == selectedPointIndices[1]) {
            continue;
        }
        float pointX = windowWidth / 2.0f + leaf.normalizedPosition.x * chartRadius;
        float pointY = windowHeight / 2.0f + leaf.normalizedPosition.y * chartRadius;
        nvgCircle(vg, pointX, pointY, pointRadius);
    }
    NVGcolor circleFillColorNvg = nvgRGBA(
            circleFillColor.getR(), circleFillColor.getG(),
            circleFillColor.getB(), circleFillColor.getA());
    nvgFillColor(vg, circleFillColorNvg);
    nvgFill(vg);

    int numPointsSelected = selectedPointIndices[0] < 0 ? 0 : (selectedPointIndices[1] < 0 ? 1 : 2);
    NVGcolor circleFillColorSelectedNvg = nvgRGBA(
            circleFillColorSelected0.getR(), circleFillColorSelected0.getG(),
            circleFillColorSelected0.getB(), circleFillColorSelected0.getA());
    for (int idx = 0; idx < numPointsSelected; idx++) {
        const auto& leaf = nodesList.at(int(0) + selectedPointIndices[idx]);
        float pointX = windowWidth / 2.0f + leaf.normalizedPosition.x * chartRadius;
        float pointY = windowHeight / 2.0f + leaf.normalizedPosition.y * chartRadius;
        nvgBeginPath(vg);
        nvgCircle(vg, pointX, pointY, pointRadius * 1.5f);
        nvgFillColor(vg, circleFillColorSelectedNvg);
        nvgFill(vg);
    }

    if (showRing) {
        renderRings();
    }
}

void DiagramBase::renderRings() {
    /*glm::vec2 center(windowWidth / 2.0f, windowHeight / 2.0f);
    auto numFields = int(fieldDataArray.size());
    auto numFieldsReal = 0;
    for (int i = 0; i < numFields; i++) {
        numFieldsReal += fieldDataArray.at(i)->useTwoFields ? 2 : 1;
    }
    int limitedFieldDataIdx = -1;
    if (limitedFieldIdx >= 0) {
        for (int i = 0; i < numFields; i++) {
            if (fieldDataArray.at(i)->selectedFieldIdx == limitedFieldIdx) {
                limitedFieldDataIdx = i;
                break;
            }
        }
    }
    auto numFieldsSize = 1;
    int i = 0;
    bool secondField = false;
    for (int ix = 0; ix < numFieldsReal; ix++) {
        auto* fieldData = fieldDataArray.at(i).get();
        std::vector<float>* leafStdDevArray = nullptr;
        if (!fieldData->useTwoFields || !secondField) {
            leafStdDevArray = &fieldData->leafStdDevArray;
        } else {
            leafStdDevArray = &fieldData->leafStdDevArray2;
        }
        int ip = ix;
        if (limitedFieldIdx >= 0) {
            if (limitedFieldIdx != fieldData->selectedFieldIdx) {
                i++;
                secondField = false;
                continue;
            } else {
                ip = secondField ? 1 : 0;
            }
        }
        std::pair<float, float> stdDevRange;
        if (useGlobalStdDevRange) {
            if (!fieldData->useTwoFields || !secondField) {
                stdDevRange = getGlobalStdDevRange(fieldData->selectedFieldIdx, 0);
            } else {
                stdDevRange = getGlobalStdDevRange(fieldData->selectedFieldIdx, 1);
            }
        } else {
            stdDevRange = std::make_pair(fieldData->minStdDev, fieldData->maxStdDev);
        }
        float pctLower = float(ip) / float(numFieldsSize);
        float pctMiddle = (float(ip) + 0.5f) / float(numFieldsSize);
        float pctUpper = float(ip + 1) / float(numFieldsSize);
        float rlo = std::max(chartRadius + outerRingOffset + pctLower * outerRingWidth, 1e-6f);
        float rmi = std::max(chartRadius + outerRingOffset + pctMiddle * outerRingWidth, 1e-6f);
        float rhi = std::max(chartRadius + outerRingOffset + pctUpper * outerRingWidth, 1e-6f);
        bool isSaturated =
                (separateColorVarianceAndCorrelation && getIsGrayscaleColorMap(colorMapVariance))
                || !separateColorVarianceAndCorrelation || selectedLineIdx < 0
                || lineFieldIndexArray.at(selectedLineIdx) == i;

        for (int leafIdx = int(leafIdxOffset); leafIdx < int(nodesList.size()); leafIdx++) {
            bool isSingleElementRegion = false;
            if (!regionsEqual) {
                if (leafIdx == int(leafIdxOffset1) - 1) {
                    if (leafIdxOffset + 1 == leafIdxOffset1) {
                        isSingleElementRegion = true;
                    } else {
                        continue;
                    }
                }
                if (leafIdx == int(nodesList.size()) - 1) {
                    if (int(leafIdxOffset1) + 1 == int(nodesList.size())) {
                        isSingleElementRegion = true;
                    } else {
                        continue;
                    }
                }
            }
            //if (!regionsEqual && (leafIdx == int(leafIdxOffset1) - 1 || leafIdx == int(nodesList.size()) - 1)) {
            //    continue;
            //}
            int numLeaves = int(nodesList.size()) - int(leafIdxOffset);
            int nextIdx = (leafIdx + 1 - int(leafIdxOffset)) % numLeaves + int(leafIdxOffset);
            if (isSingleElementRegion) {
                nextIdx = leafIdx;
            }
            const auto &leafCurr = nodesList.at(leafIdx);
            const auto &leafNext = nodesList.at(nextIdx);
            bool isAnglePositive = regionsEqual || leafNext.angle - leafCurr.angle > 0.0f;
            float deltaAngleSign = isAnglePositive ? 1.0f : -1.0f;
            float angle0 = leafCurr.angle;
            float angle1 = leafNext.angle + deltaAngleSign * 0.005f;
            if (isSingleElementRegion) {
                const float angleRangeHalf = sgl::PI * 0.92f;
                angle0 = leafCurr.angle - 0.5f * angleRangeHalf;
                angle1 = leafCurr.angle + 0.5f * angleRangeHalf;
            }
            float angleMid0 = angle0;
            float angleMid1 = angle1;
            bool isStartSegment = false;
            bool isEndSegment = false;
            if (!regionsEqual && (leafIdx == int(leafIdxOffset) || leafIdx == int(leafIdxOffset1))) {
                float deltaAngle = angle1 - angle0;
                if (std::abs(deltaAngle) > 0.1f) {
                    deltaAngle = deltaAngleSign * 0.1f;
                }
                angle0 -= deltaAngle * 0.5f;
                if (leafIdx == int(leafIdxOffset)) {
                    fieldData->a00 = angle0;
                } else {
                    fieldData->a10 = angle0;
                }
                isStartSegment = true;
            }
            if (!regionsEqual && (nextIdx == int(leafIdxOffset1) - 1 || nextIdx == int(nodesList.size()) - 1)) {
                float deltaAngle = angle1 - angle0;
                if (std::abs(deltaAngle) > 0.1f) {
                    deltaAngle = deltaAngleSign * 0.1f;
                }
                angle1 += deltaAngle * 0.5f;
                if (nextIdx == int(leafIdxOffset1) - 1) {
                    fieldData->a01 = angle1;
                } else {
                    fieldData->a11 = angle1;
                }
                isEndSegment = true;
            }
            float cos0 = std::cos(angle0), sin0 = std::sin(angle0);
            float cos1 = std::cos(angle1), sin1 = std::sin(angle1);
            float cosMid0 = std::cos(angleMid0), sinMid0 = std::sin(angleMid0);
            float cosMid1 = std::cos(angleMid1), sinMid1 = std::sin(angleMid1);
            //glm::vec2 lo0 = center + rlo * glm::vec2(cos0, sin0);
            glm::vec2 lo1 = center + rlo * glm::vec2(cos1, sin1);
            glm::vec2 hi0 = center + rhi * glm::vec2(cos0, sin0);
            //glm::vec2 hi1 = center + rhi * glm::vec2(cos1, sin1);
            glm::vec2 mi0 = center + rmi * glm::vec2(cosMid0, sinMid0);
            glm::vec2 mi1 = center + rmi * glm::vec2(cosMid1, sinMid1);

            float stdev0 = leafStdDevArray->at(leafIdx - int(leafIdxOffset));
            float t0 = stdDevRange.first == stdDevRange.second ? 0.0f : (stdev0 - stdDevRange.first) / (stdDevRange.second - stdDevRange.first);
            float stdev1 = leafStdDevArray->at(nextIdx - int(leafIdxOffset));
            float t1 = stdDevRange.first == stdDevRange.second ? 0.0f : (stdev1 - stdDevRange.first) / (stdDevRange.second - stdDevRange.first);

            if (vg) {
                glm::vec4 rgbColor0 = fieldData->evalColorMapVec4Variance(t0, isSaturated);
                rgbColor0.w = 1.0f;
                glm::vec4 rgbColor1 = fieldData->evalColorMapVec4Variance(t1, isSaturated);
                rgbColor1.w = 1.0f;
                NVGcolor fillColor0 = nvgRGBAf(rgbColor0.x, rgbColor0.y, rgbColor0.z, rgbColor0.w);
                NVGcolor fillColor1 = nvgRGBAf(rgbColor1.x, rgbColor1.y, rgbColor1.z, rgbColor1.w);

                nvgBeginPath(vg);
                nvgArc(vg, center.x, center.y, rlo, angle1, angle0, isAnglePositive ? NVG_CCW : NVG_CW);
                if (isStartSegment) {
                    float angleMid = angle0 + arrowAngleRad * deltaAngleSign;
                    nvgLineTo(vg, center.x + rmi * std::cos(angleMid), center.y + rmi * std::sin(angleMid));
                }
                nvgLineTo(vg, hi0.x, hi0.y);
                nvgArc(vg, center.x, center.y, rhi, angle0, angle1, isAnglePositive ? NVG_CW : NVG_CCW);
                if (isEndSegment) {
                    float angleMid = angle1 + arrowAngleRad * deltaAngleSign;
                    nvgLineTo(vg, center.x + rmi * std::cos(angleMid), center.y + rmi * std::sin(angleMid));
                }
                nvgLineTo(vg, lo1.x, lo1.y);
                nvgClosePath(vg);

                NVGpaint paint = nvgLinearGradient(vg, mi0.x, mi0.y, mi1.x, mi1.y, fillColor0, fillColor1);
                nvgFillPaint(vg, paint);
                nvgFill(vg);
            }
        }

        if (fieldData->useTwoFields) {
            if (secondField) {
                i++;
                secondField = false;
            } else {
                secondField = true;
            }
        } else {
            i++;
        }
    }

    int selectedLineFieldPos = 0;
    if (selectedLineIdx >= 0) {
        int selectedLineFieldIdx = lineFieldIndexArray.at(selectedLineIdx);
        for (int j = 0; j < numFields; j++) {
            if (j == selectedLineFieldIdx) {
                break;
            }
            selectedLineFieldPos += fieldDataArray.at(selectedLineFieldIdx)->useTwoFields ? 2 : 1;
        }
    }

    sgl::Color circleStrokeColor = isDarkMode ? circleStrokeColorDark : circleStrokeColorBright;
    sgl::Color currentCircleColor;
    i = 0;
    secondField = false;
    for (int ix = 0; ix < numFieldsReal; ix++) {
        currentCircleColor = circleStrokeColor;
        int fieldIdx = i;
        int ip = ix;
        bool isSelectedRing = false;
        if (separateColorVarianceAndCorrelation && getIsGrayscaleColorMap(colorMapVariance) && numFieldsSize > 1
                && selectedLineIdx >= 0 && limitedFieldIdx < 0) {
            int selectedLineFieldIdx = lineFieldIndexArray.at(selectedLineIdx);
            if (i == numFields - 1) {
                fieldIdx = selectedLineFieldIdx;
                ip = selectedLineFieldPos + (secondField ? 1 : 0);
                currentCircleColor = ringStrokeColorSelected;
                isSelectedRing = true;
            } else if (i >= selectedLineFieldIdx) {
                fieldIdx++;
                ip += fieldDataArray.at(selectedLineFieldIdx)->useTwoFields ? 2 : 1;
            }
        }
        auto* fieldData = fieldDataArray.at(fieldIdx).get();
        if (limitedFieldIdx >= 0) {
            if (limitedFieldIdx != fieldData->selectedFieldIdx) {
                i++;
                secondField = false;
                continue;
            } else {
                ip = 0;
            }
        }
        float pctLower = float(ip) / float(numFieldsSize);
        float pctMiddle = (float(ip) + 0.5f) / float(numFieldsSize);
        float pctUpper = float(ip + 1) / float(numFieldsSize);
        float rlo = std::max(chartRadius + outerRingOffset + pctLower * outerRingWidth, 1e-6f);
        float rmi = std::max(chartRadius + outerRingOffset + pctMiddle * outerRingWidth, 1e-6f);
        float rhi = std::max(chartRadius + outerRingOffset + pctUpper * outerRingWidth, 1e-6f);

        if (vg) {
            nvgBeginPath(vg);
            if (regionsEqual) {
                nvgCircle(vg, center.x, center.y, rlo);
                if (ip == numFieldsSize - 1 || isSelectedRing || limitedFieldIdx >= 0) {
                    nvgCircle(vg, center.x, center.y, rhi);
                }
            } else {
                bool isAnglePos0 = fieldData->a01 - fieldData->a00 > 0.0f;
                nvgArc(vg, center.x, center.y, rlo, fieldData->a00, fieldData->a01, isAnglePos0 ? NVG_CW : NVG_CCW);
                if (useRingArrows) {
                    float angleMid = fieldData->a01 + arrowAngleRad * (isAnglePos0 ? 1.0f : -1.0f);
                    nvgLineTo(vg, center.x + rmi * std::cos(angleMid), center.y + rmi * std::sin(angleMid));
                }
                nvgLineTo(vg, center.x + rhi * std::cos(fieldData->a01), center.y + rhi * std::sin(fieldData->a01));
                nvgArc(vg, center.x, center.y, rhi, fieldData->a01, fieldData->a00, isAnglePos0 ? NVG_CCW : NVG_CW);
                if (useRingArrows) {
                    float angleMid = fieldData->a00 + arrowAngleRad * (isAnglePos0 ? 1.0f : -1.0f);
                    nvgLineTo(vg, center.x + rmi * std::cos(angleMid), center.y + rmi * std::sin(angleMid));
                }
                nvgLineTo(vg, center.x + rlo * std::cos(fieldData->a00), center.y + rlo * std::sin(fieldData->a00));

                bool isAnglePos1 = fieldData->a11 - fieldData->a10 > 0.0f;
                nvgMoveTo(vg, center.x + rlo * std::cos(fieldData->a10), center.y + rlo * std::sin(fieldData->a10));
                nvgArc(vg, center.x, center.y, rlo, fieldData->a10, fieldData->a11, isAnglePos1 ? NVG_CW : NVG_CCW);
                if (useRingArrows) {
                    float angleMid = fieldData->a11 + arrowAngleRad * (isAnglePos1 ? 1.0f : -1.0f);
                    nvgLineTo(vg, center.x + rmi * std::cos(angleMid), center.y + rmi * std::sin(angleMid));
                }
                nvgLineTo(vg, center.x + rhi * std::cos(fieldData->a11), center.y + rhi * std::sin(fieldData->a11));
                nvgArc(vg, center.x, center.y, rhi, fieldData->a11, fieldData->a10, isAnglePos1 ? NVG_CCW : NVG_CW);
                if (useRingArrows) {
                    float angleMid = fieldData->a10 + arrowAngleRad * (isAnglePos1 ? 1.0f : -1.0f);
                    nvgLineTo(vg, center.x + rmi * std::cos(angleMid), center.y + rmi * std::sin(angleMid));
                }
                nvgLineTo(vg, center.x + rlo * std::cos(fieldData->a10), center.y + rlo * std::sin(fieldData->a10));
            }
            nvgStrokeWidth(vg, 1.0f);
            NVGcolor currentCircleColorNvg = nvgRGBA(
                    currentCircleColor.getR(), currentCircleColor.getG(),
                    currentCircleColor.getB(), currentCircleColor.getA());
            nvgStrokeColor(vg, currentCircleColorNvg);
            nvgStroke(vg);
        }

        if (fieldData->useTwoFields) {
            if (secondField) {
                i++;
                secondField = false;
            } else {
                secondField = true;
            }
        } else {
            i++;
        }
    }*/
}
