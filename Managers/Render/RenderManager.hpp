//
// Created by Pixeluted on 03/05/2025.
//
#pragma once
#include <d3d11.h>
#include <dxgi.h>
#include <mutex>

struct DrawingObj;

#include "lua.h"
#include "../../ApplicationContext.hpp"
#include "../../Environment/ExploitObject.hpp"
#include "../../Environment/ExploitObjects/Drawing.hpp"
#include "Imgui/imgui.h"

using present_t = int32_t(__stdcall *)(IDXGISwapChain* swapChain, uint32_t syncInterval, uint32_t flags);
using resize_buffers_t = int32_t(__stdcall *)(IDXGISwapChain* swapChain, uint32_t bufferCount, uint32_t width, uint32_t height, int32_t newFormat, uint32_t swapChainFlags);

static ImFont* SystemFont;
static ImFont* PlexFont;
static ImFont* MonospaceFont;

class RenderManager final : public Service {
public:
    ImGuiIO* imguiIo{};
    IDXGISwapChain* currentSwapChain{};
    ID3D11Device* device{};
    ID3D11DeviceContext* deviceContext{};
    ID3D11RenderTargetView* targetView{};

    present_t originalPresent{};
    resize_buffers_t originalResizeBuffers{};
    WNDPROC originalWndProc{};

    std::vector<std::shared_ptr<DrawingObj>> activeDrawingObjects;
    std::mutex activeObjectsMutex;

    bool debugMenuOpen;

    RenderManager();

    void Draw();
    void PushDrawObject(const std::shared_ptr<DrawingObj>& renderObject);
    void RemoveDrawObject(const std::shared_ptr<DrawingObj>& renderObject);
    void ClearAllObjects();

    ImTextureID CreateImageTexture(const std::vector<uint8_t>& data, int width, int height) const;

    static int32_t __stdcall presentHook(IDXGISwapChain* swapChain, uint32_t syncInterval, uint32_t flags);
    static int32_t __stdcall resizeBuffersHook(IDXGISwapChain* swapChain, uint32_t bufferCount, uint32_t width, uint32_t height, int32_t newFormat, uint32_t swapChainFlags);
    static intptr_t wndProc(HWND window, uint32_t msg, uintptr_t uParam, intptr_t iParam);
};
