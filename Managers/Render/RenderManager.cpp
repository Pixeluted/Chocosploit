//
// Created by Pixeluted on 03/05/2025.
//

#include "RenderManager.hpp"

#include <algorithm>
#include <format>

#include "Fonts.hpp"
#include "ldebug.h"
#include "lualib.h"
#include "../../Logger.hpp"
#include "../../OffsetsAndFunctions.hpp"
#include "../../Execution/InternalTaskScheduler.hpp"
#include "../../Roblox/TaskScheduler.hpp"
#include "Imgui/imgui_impl_dx11.h"
#include "Imgui/imgui_impl_win32.h"
#include "stb/stb_image.hpp"
#include "stb/stb_image_resize2.hpp"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWindow, UINT uMsg, WPARAM wParam, LPARAM lParam);


int32_t RenderManager::presentHook(IDXGISwapChain *swapChain, uint32_t syncInterval, uint32_t flags) {
    const auto renderManager = ApplicationContext::GetService<RenderManager>();
    if (!renderManager->targetView) {
        ID3D11Texture2D *back_buffer{};
        if (FAILED(renderManager->currentSwapChain->GetBuffer( 0, IID_PPV_ARGS( &back_buffer ) )))
            return renderManager->originalPresent(swapChain, syncInterval, flags);

        D3D11_RENDER_TARGET_VIEW_DESC dsc;
        ZeroMemory(&dsc, sizeof(dsc));
        dsc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        dsc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
        if (FAILED(renderManager->device->CreateRenderTargetView( back_buffer, &dsc, &renderManager->targetView )))
            return renderManager->originalPresent(swapChain, syncInterval, flags);

        back_buffer->Release();
    }

    const auto currentDeviceContext = renderManager->deviceContext;
    const auto ourTargetView = renderManager->targetView;

    ID3D11RenderTargetView *originalTargetView{};
    currentDeviceContext->OMGetRenderTargets(1, &originalTargetView, nullptr);
    currentDeviceContext->OMSetRenderTargets(1, &ourTargetView, nullptr);

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    renderManager->Draw();

    ImGui::Render();

    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    currentDeviceContext->OMSetRenderTargets(1, &originalTargetView, nullptr);

    return renderManager->originalPresent(swapChain, syncInterval, flags);
}

int32_t RenderManager::resizeBuffersHook(IDXGISwapChain *swapChain, uint32_t bufferCount, uint32_t width,
                                         uint32_t height, int32_t newFormat, uint32_t swapChainFlags) {
    const auto renderManager = ApplicationContext::GetService<RenderManager>();
    if (renderManager->targetView) {
        renderManager->targetView->Release();
        renderManager->targetView = nullptr;
    }

    return renderManager->originalResizeBuffers(swapChain, bufferCount, width, height, newFormat, swapChainFlags);
}

intptr_t RenderManager::wndProc(HWND window, uint32_t msg, uintptr_t uParam, intptr_t iParam) {
    const auto renderManager = ApplicationContext::GetService<RenderManager>();

    if (msg == WM_KEYDOWN && uParam == VK_INSERT)
        renderManager->debugMenuOpen = !renderManager->debugMenuOpen;

    ImGui_ImplWin32_WndProcHandler(window, msg, uParam, iParam);
    return CallWindowProc(renderManager->originalWndProc, window, msg, uParam, iParam);
}

RenderManager::RenderManager(): debugMenuOpen(false) {
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    imguiIo = &ImGui::GetIO();
    SystemFont = imguiIo->Fonts->AddFontFromMemoryTTF(
        SystemFontData,
        SystemFontLength,
        16.0f
    );
    PlexFont = imguiIo->Fonts->AddFontFromMemoryTTF(
        PlexFontData,
        PlexFontLength,
        16.0f
    );
    MonospaceFont = imguiIo->Fonts->AddFontFromMemoryTTF(
        MonoSpaceData,
        MonoSpaceLength,
        16.0f
    );


    imguiIo->LogFilename = imguiIo->IniFilename = nullptr;
    imguiIo->ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange | ImGuiConfigFlags_NavNoCaptureKeyboard;

    const auto robloxTaskScheduler = ApplicationContext::GetService<TaskScheduler>();
    const auto renderJobOpt = robloxTaskScheduler->FindRenderJob();
    if (!renderJobOpt.has_value()) {
        MessageBoxA(nullptr, "Failed to initialize drawing (0x1)", "Drawing Error", MB_OK);
        _exit(0);
    }
    const auto &renderJob = renderJobOpt.value();
    const auto renderView = *reinterpret_cast<uintptr_t *>(
        renderJob.jobAddress + ChocoSploit::StructOffsets::TaskScheduler::Job::RenderJob::RenderView);
    const auto deviceD3D11 = *reinterpret_cast<uintptr_t *>(
        renderView + ChocoSploit::StructOffsets::TaskScheduler::Job::RenderJob::DeviceD3D11);
    const auto swapChain = *reinterpret_cast<uintptr_t *>(
        deviceD3D11 + ChocoSploit::StructOffsets::TaskScheduler::Job::RenderJob::SwapChain);
    const auto theSwapChain = reinterpret_cast<IDXGISwapChain *>(swapChain);
    currentSwapChain = theSwapChain;
    if (FAILED(theSwapChain->GetDevice(IID_PPV_ARGS(&device)))) {
        MessageBoxA(nullptr, "Failed to initialize drawing (0x2)", "Drawing Error", MB_OK);
        _exit(0);
    }

    device->GetImmediateContext(&deviceContext);
    DXGI_SWAP_CHAIN_DESC backBufferDesc{};
    if (FAILED(theSwapChain->GetDesc(&backBufferDesc))) {
        MessageBoxA(nullptr, "Failed to initialize drawing (0x3)", "Drawing Error", MB_OK);
        _exit(0);
    }

    ID3D11Texture2D *backBuffer{};
    if (FAILED(theSwapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer)))) {
        MessageBoxA(nullptr, "Failed to initialize drawing (0x4)", "Drawing Error", MB_OK);
        _exit(0);
    }

    D3D11_RENDER_TARGET_VIEW_DESC dsc;
    ZeroMemory(&dsc, sizeof(dsc));
    dsc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    dsc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    if (FAILED(device->CreateRenderTargetView(backBuffer, &dsc, &targetView))) {
        MessageBoxA(nullptr, "Failed to initialize drawing (0x5)", "Drawing Error", MB_OK);
        _exit(0);
    }

    backBuffer->Release();

    if (!ImGui_ImplWin32_Init(backBufferDesc.OutputWindow)) {
        MessageBoxA(nullptr, "Failed to initialize drawing (0x6)", "Drawing Error", MB_OK);
        _exit(0);
    }

    if (!ImGui_ImplDX11_Init(device, deviceContext)) {
        MessageBoxA(nullptr, "Failed to initialize drawing (0x7)", "Drawing Error", MB_OK);
        _exit(0);
    }

    originalWndProc = reinterpret_cast<WNDPROC>(SetWindowLongPtrA(backBufferDesc.OutputWindow, GWLP_WNDPROC,
                                                                  reinterpret_cast<LONG_PTR>(&wndProc)));

    const auto swapChainVTable = *reinterpret_cast<uintptr_t **>(swapChain);
    const auto newVTable = new uintptr_t[0x200];
    memcpy(newVTable, swapChainVTable, 0x200);

    originalPresent = reinterpret_cast<present_t>(swapChainVTable[8]);
    newVTable[8] = reinterpret_cast<uintptr_t>(&presentHook);

    originalResizeBuffers = reinterpret_cast<resize_buffers_t>(swapChainVTable[13]);
    newVTable[13] = reinterpret_cast<uintptr_t>(&resizeBuffersHook);

    *reinterpret_cast<uintptr_t **>(swapChain) = static_cast<uintptr_t *>(newVTable);
    isInitialized = true;
    ApplicationContext::GetService<Logger>()->Log(Info, "Initialized rendering!");
}

void RenderManager::PushDrawObject(const std::shared_ptr<DrawingObj> &renderObject) {
    std::lock_guard drawListLock{activeObjectsMutex};
    activeDrawingObjects.push_back(renderObject);
}

void RenderManager::RemoveDrawObject(const std::shared_ptr<DrawingObj> &renderObject) {
    std::lock_guard drawListLock{activeObjectsMutex};
    if (const auto it = std::ranges::find(activeDrawingObjects, renderObject); it != activeDrawingObjects.end()) {
        activeDrawingObjects.erase(it);
    }
}

void RenderManager::ClearAllObjects() {
    std::lock_guard drawListLock{activeObjectsMutex};
    activeDrawingObjects.clear();
}

ImTextureID RenderManager::CreateImageTexture(const std::vector<uint8_t> &data, const int width,
                                              const int height) const {
    D3D11_TEXTURE2D_DESC desc;
    ZeroMemory(&desc, sizeof(desc));
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;

    ID3D11Texture2D *texture;
    D3D11_SUBRESOURCE_DATA subResource;
    subResource.pSysMem = data.data();
    subResource.SysMemPitch = width * 4;
    subResource.SysMemSlicePitch = 0;
    device->CreateTexture2D(&desc, &subResource, &texture);

    ID3D11ShaderResourceView *textureView;
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
    ZeroMemory(&srvDesc, sizeof(srvDesc));
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = desc.MipLevels;
    srvDesc.Texture2D.MostDetailedMip = 0;

    device->CreateShaderResourceView(texture, &srvDesc, &textureView);
    texture->Release();

    return reinterpret_cast<ImTextureID>(textureView);
}

void RenderManager::Draw() {
    if (debugMenuOpen) {
        ImGui::SetNextWindowSize(ImVec2{300.0f, 235.0f});
        ImGui::Begin("ChocoSploit | Debug Panel", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

        const float oldScale = ImGui::GetFont()->Scale;
        ImGui::GetFont()->Scale *= 1.1;
        ImGui::PushFont(ImGui::GetFont());

        const auto taskScheduler = ApplicationContext::GetService<InternalTaskScheduler>();

        ImGui::Text("Status: %s", taskScheduler->currentState->isInitializedOnGame ? "In Game" : "In Menu");
        ImGui::Spacing();
        ImGui::Text(
            std::format("WaitingHybridScriptsJob: 0x{:x}", taskScheduler->currentState->lastWHSJ.jobAddress).c_str());
        ImGui::Text(std::format("Script Context: 0x{:x}", taskScheduler->currentState->currentScriptContext).c_str());
        ImGui::Text(std::format("DataModel: 0x{:x}", taskScheduler->currentState->lastKnownDataModel).c_str());
        ImGui::Text(std::format("Global Lua State: 0x{:x}",
                                reinterpret_cast<uintptr_t>(taskScheduler->luaStates->robloxLuaState)).c_str());
        ImGui::Text(std::format("Executor Lua State: 0x{:x}",
                                reinterpret_cast<uintptr_t>(taskScheduler->luaStates->executorLuaState)).c_str());
        ImGui::Spacing();
        ImGui::Text("Active Yield Threads: %d", taskScheduler->currentState->activeYieldThreads.load());
        ImGui::Text("Active Drawing Objects: %d", activeDrawingObjects.size());
        ImGui::Text("Queued Teleport Scripts: %d", taskScheduler->scheduledItems->scheduledTeleportScripts.size());

        ImGui::GetFont()->Scale = oldScale;
        ImGui::PopFont();

        ImGui::End();
    }

    if (!activeObjectsMutex.try_lock())
        return;

    if (activeDrawingObjects.empty()) {
        activeObjectsMutex.unlock();
        return;
    }

    const auto drawList = ImGui::GetForegroundDrawList();

    std::vector<DrawingObj *> sortedObjects;
    sortedObjects.reserve(activeDrawingObjects.size());

    for (const auto &object: activeDrawingObjects) {
        if (object->isVisible) {
            sortedObjects.push_back(object.get());
        }
    }

    std::ranges::sort(sortedObjects,
                      [](const DrawingObj *a, const DrawingObj *b) {
                          return a->zIndex < b->zIndex;
                      });

    for (const auto &renderObject: sortedObjects) {
        auto finalColor = renderObject->color;
        finalColor.w *= renderObject->transparency;
        const auto color = ImGui::ColorConvertFloat4ToU32(finalColor);

        switch (renderObject->objectType) {
            case Line:
                drawList->AddLine(
                    renderObject->line.from,
                    renderObject->line.to,
                    color,
                    renderObject->line.thickness
                );
                break;
            case Circle:
                if (renderObject->circle.filled) {
                    drawList->AddCircleFilled(renderObject->circle.position, renderObject->circle.radius, color,
                                              renderObject->circle.numsides);
                } else {
                    drawList->AddCircle(renderObject->circle.position, renderObject->circle.radius, color,
                                        renderObject->circle.numsides, renderObject->circle.thickness);
                }
                break;
            case Square:
                if (renderObject->square.filled) {
                    drawList->AddRectFilled(renderObject->square.position, ImVec2{
                                                renderObject->square.position.x + renderObject->square.size.x,
                                                renderObject->square.position.y + renderObject->square.size.y,
                                            }, color);
                } else {
                    drawList->AddRect(renderObject->square.position, ImVec2{
                                          renderObject->square.position.x + renderObject->square.size.x,
                                          renderObject->square.position.y + renderObject->square.size.y,
                                      }, color, 0.0f, 0, renderObject->square.thickness);
                }
                break;
            case Quad:
                if (renderObject->quad.filled) {
                    drawList->AddQuadFilled(renderObject->quad.pointA, renderObject->quad.pointB,
                                            renderObject->quad.pointC, renderObject->quad.pointD, color);
                } else {
                    drawList->AddQuad(renderObject->quad.pointA, renderObject->quad.pointB, renderObject->quad.pointC,
                                      renderObject->quad.pointD, color, renderObject->quad.thickness);
                }
                break;
            case Triangle:
                if (renderObject->triangle.filled) {
                    drawList->AddTriangleFilled(renderObject->triangle.pointA, renderObject->triangle.pointB,
                                                renderObject->triangle.pointC, color);
                } else {
                    drawList->AddTriangle(renderObject->triangle.pointA, renderObject->triangle.pointB,
                                          renderObject->triangle.pointC, color, renderObject->triangle.thickness);
                }
                break;
            case Text: {
                const auto drawFont = renderObject->GetDrawFont();
                auto textPosition = renderObject->text.position;
                if (renderObject->text.center) {
                    textPosition.x -= renderObject->text.textBounds.x * 0.5f;
                }

                if (renderObject->text.outline) {
                    const auto outlineColor = ImGui::ColorConvertFloat4ToU32(renderObject->text.outlineColor);
                    constexpr ImVec2 outlineOffsets[4] = {
                        {0, -1.0f},
                        {0, 1.0f},
                        {-1.0f, 0},
                        {1.0f, 0}
                    };

                    for (const auto &offset: outlineOffsets) {
                        auto outlinePos = ImVec2(textPosition.x + offset.x, textPosition.y + offset.y);
                        drawList->AddText(
                            drawFont,
                            renderObject->text.size,
                            outlinePos,
                            outlineColor,
                            renderObject->text.text.c_str()
                        );
                    }
                }

                drawList->AddText(
                    drawFont,
                    renderObject->text.size,
                    textPosition,
                    color,
                    renderObject->text.text.c_str()
                );
                break;
            }
            case Image: {
                if (!renderObject->image.textureCreated && !renderObject->image.data.empty()) {
                    const auto width = static_cast<int>(renderObject->image.size.x);
                    const auto height = static_cast<int>(renderObject->image.size.y);

                    if (width > 0 && height > 0) {
                        renderObject->image.textureId = CreateImageTexture(
                            renderObject->image.data,
                            width,
                            height
                        );
                        renderObject->image.textureCreated = true;
                    }
                }

                if (renderObject->image.textureCreated && renderObject->image.textureId) {
                    const auto endPosition = ImVec2{
                        renderObject->image.position.x + renderObject->image.size.x,
                        renderObject->image.position.y + renderObject->image.size.y
                    };

                    if (renderObject->image.rounding > 0) {
                        drawList->AddImageRounded(
                            renderObject->image.textureId,
                            renderObject->image.position,
                            endPosition,
                            ImVec2{0, 0},
                            ImVec2{1, 1},
                            color,
                            renderObject->image.rounding
                        );
                    } else {
                        drawList->AddImage(
                            renderObject->image.textureId,
                            renderObject->image.position,
                            endPosition,
                            ImVec2{0, 0},
                            ImVec2{1, 1},
                            color
                        );
                    }
                }
                break;
            }

            default:
                break;
        }
    }

    activeObjectsMutex.unlock();
}
