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

#ifndef CORRERENDER_DIAGRAMBASE_HPP
#define CORRERENDER_DIAGRAMBASE_HPP

#include <set>
#include <sstream>
#include <functional>

#include <Graphics/Window.hpp>
#include <Graphics/Vector/VectorWidget.hpp>

struct NVGcontext;
typedef struct NVGcontext NVGcontext;
struct NVGcolor;

struct HEBNode {
    glm::vec2 normalizedPosition;
};

class DiagramBase : public sgl::VectorWidget {
public:
    DiagramBase();
    virtual void initialize();
    void update(float dt) override;
    [[nodiscard]] bool getIsMouseOverDiagramImGui() const;
    void setIsMouseGrabbedByParent(bool _isMouseGrabbedByParent);
    virtual void updateSizeByParent();
    void setImGuiWindowOffset(int offsetX, int offsetY);
    void setClearColor(const sgl::Color& clearColor);
    [[nodiscard]] inline bool getNeedsReRender() { bool tmp = needsReRender; needsReRender = false; return tmp; }
    [[nodiscard]] inline bool getIsMouseGrabbed() const { return isMouseGrabbed; }

    [[nodiscard]] inline bool getSelectedVariablesChanged() const { return selectedVariablesChanged; };
    [[nodiscard]] inline const std::set<size_t>& getSelectedVariableIndices() const { return selectedVariableIndices; };
    inline void getSelectedVariableIndices(const std::set<size_t>& newSelectedVariableIndices) {
        selectedVariableIndices = newSelectedVariableIndices;
    };

protected:
    void onBackendCreated() override;
    void onBackendDestroyed() override;

    // Widget move/resize events.
    void mouseMoveEvent(const glm::ivec2& mousePositionPx, const glm::vec2& mousePositionScaled);
    void mouseMoveEventParent(const glm::ivec2& mousePositionPx, const glm::vec2& mousePositionScaled);
    void mousePressEventResizeWindow(const glm::ivec2& mousePositionPx, const glm::vec2& mousePositionScaled);
    void mousePressEventMoveWindow(const glm::ivec2& mousePositionPx, const glm::vec2& mousePositionScaled);
    virtual void onUpdatedWindowSize() {}

    // NanoVG backend.
    virtual void renderBaseNanoVG();
    void getNanoVGContext();
    NVGcontext* vg = nullptr;

    // Test code.
    void renderChordDiagramNanoVG();
    int numLinesTotal = 0;
    int MAX_NUM_LINES = 100;
    const int NUM_SUBDIVISIONS = 50;
    float beta = 0.75f;
    float curveThickness = 1.5f;
    float curveOpacity = 0.1f;
    std::vector<glm::vec2> curvePoints;
    float chartRadius{};
    float totalRadius{};

    void renderRings();
    bool showRing = true;
    float outerRingOffset = 3.0f;
    float outerRingWidth = 0.0f; //< Determined automatically.
    float outerRingSizePct = 0.1f;
    sgl::Color ringStrokeColorSelected = sgl::Color(255, 255, 130);
    int limitedFieldIdx = -1;
    int selectedLineIdx = -1;
    bool separateColorVarianceAndCorrelation = true;

    float pointRadiusBase = 1.5f;
    sgl::Color circleFillColor = sgl::Color(180, 180, 180, 255);
    sgl::Color circleFillColorSelected0 = sgl::Color(180, 80, 80, 255); //< Overwritten in setUseNeonSelectionColors.
    sgl::Color circleFillColorSelected1 = sgl::Color(50, 100, 180, 255); //< Overwritten in setUseNeonSelectionColors.
    sgl::Color circleStrokeColorDark = sgl::Color(255, 255, 255, 255);
    sgl::Color circleStrokeColorBright = sgl::Color(0, 0, 0, 255);
    int selectedPointIndices[2] = { -1, -1 };
    std::vector<HEBNode> nodesList;

    // Scale factor used for rendering.
    float s = 1.0f;

    /// Removes decimal points if more than maxDigits digits are used.
    static std::string getNiceNumberString(float number, int digits);
    /// Conversion to and from string
    template <class T>
    static std::string toString(
            T obj, int precision, bool fixed = true, bool noshowpoint = false, bool scientific = false) {
        std::ostringstream ostr;
        ostr.precision(precision);
        if (fixed) {
            ostr << std::fixed;
        }
        if (noshowpoint) {
            ostr << std::noshowpoint;
        }
        if (scientific) {
            ostr << std::scientific;
        }
        ostr << obj;
        return ostr.str();
    }

    float textSize = 8.0f;

    bool needsReRender = false;
    float borderSizeX = 0, borderSizeY = 0;
    const float borderWidth = 1.0f;
    const float borderRoundingRadius = 4.0f;
    float backgroundOpacity = 1.0f;
    float textSizeLegend = 12.0f;

    // Color palette.
    bool isDarkMode = true;
    sgl::Color backgroundFillColorDark = sgl::Color(20, 20, 20, 255);
    //sgl::Color backgroundFillColorBright = sgl::Color(230, 230, 230, 255);
    sgl::Color backgroundFillColorBright = sgl::Color(245, 245, 245, 255);
    sgl::Color backgroundStrokeColorDark = sgl::Color(60, 60, 60, 255);
    sgl::Color backgroundStrokeColorBright = sgl::Color(190, 190, 190, 255);
    bool renderBackgroundStroke = true;

    enum ResizeDirection {
        NONE = 0, LEFT = 1, RIGHT = 2, BOTTOM = 4, TOP = 8,
        BOTTOM_LEFT = BOTTOM | LEFT, BOTTOM_RIGHT = BOTTOM | RIGHT, TOP_LEFT = TOP | LEFT, TOP_RIGHT = TOP | RIGHT
    };
    [[nodiscard]] inline ResizeDirection getResizeDirection() const { return resizeDirection; }

    // Dragging the window.
    bool isDraggingWindow = false;
    int mouseDragStartPosX = 0;
    int mouseDragStartPosY = 0;
    float windowOffsetXBase = 0.0f;
    float windowOffsetYBase = 0.0f;

    // Resizing the window.
    bool isResizingWindow = false;
    ResizeDirection resizeDirection = ResizeDirection::NONE;
    const float resizeMarginBase = 4;
    float resizeMargin = resizeMarginBase; // including scale factor
    int lastResizeMouseX = 0;
    int lastResizeMouseY = 0;
    sgl::CursorType cursorShape = sgl::CursorType::DEFAULT;

    // Offset for deducing mouse position.
    void checkWindowMoveOrResizeJustFinished(const glm::ivec2& mousePositionPx);
    int imGuiWindowOffsetX = 0, imGuiWindowOffsetY = 0;
    bool isMouseGrabbedByParent = false;
    bool isMouseGrabbed = false;
    bool windowMoveOrResizeJustFinished = false;
    bool isWindowFixed = false; //< Is resize and grabbing disabled?

    // Variables can be selected by clicking on them.
    size_t numVariables = 0;
    std::set<size_t> selectedVariableIndices;
    bool selectedVariablesChanged = false;
};

#endif //CORRERENDER_DIAGRAMBASE_HPP
