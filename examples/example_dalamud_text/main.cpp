#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include "example_text.h"
#include <d3d11.h>

// From: imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace {
    ID3D11Device* s_pd3dDevice = nullptr;
    ID3D11DeviceContext* s_pd3dDeviceContext = nullptr;
    IDXGISwapChain* s_pSwapChain = nullptr;
    bool s_swapChainOccluded = false;
    UINT s_resizeWidth = 0, s_resizeHeight = 0;
    ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;

    bool CreateDeviceD3D(HWND hWnd);
    void CleanupDeviceD3D();
    void CreateRenderTarget();
    void CleanupRenderTarget();
    LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void ShowTextInterfaceWindow(bool* pOpen);
}

int main() {
    const auto font = ExampleText::LoadFont(R"(C:\Windows\Fonts\segoeui.ttf)");

    ImGui_ImplWin32_EnableDpiAwareness();

    const WNDCLASSEXW wc{.cbSize = sizeof(wc), .style = CS_CLASSDC, .lpfnWndProc = WndProc, .cbClsExtra = 0L, .cbWndExtra = 0L, .hInstance = GetModuleHandle(nullptr), .hIcon = nullptr, .hCursor = nullptr, .hbrBackground = nullptr, .lpszMenuName = nullptr, .lpszClassName = L"ImGui Example", .hIconSm = nullptr};
    RegisterClassExW(&wc);
    const auto hwnd = CreateWindowExW(0, wc.lpszClassName, L"Dear ImGui Dalamud Text Interface", WS_OVERLAPPEDWINDOW, 100, 100, 1280, 800, nullptr, nullptr, wc.hInstance, nullptr);
    const auto main_scale = ImGui_ImplWin32_GetDpiScaleForMonitor(MonitorFromWindow(hwnd, 0));

    if (!CreateDeviceD3D(hwnd)) {
        CleanupDeviceD3D();
        UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad | ImGuiConfigFlags_DockingEnable;

    ExampleText::Install();
    io.FontDefault = ExampleText::ToImFont(font);

    auto& style = ImGui::GetStyle();
    ImGui::StyleColorsDark();
    style.FontSizeBase = 16.0f;
    style.FrameRounding = 3.0f;
    style.GrabRounding = 3.0f;
    style.ScaleAllSizes(style.FontScaleDpi = main_scale);

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(s_pd3dDevice, s_pd3dDeviceContext);
    SetWindowPos(hwnd, 0, 0, 0, static_cast<int>(1280 * main_scale), static_cast<int>(800 * main_scale), SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);

    auto show_demo_window = true, show_text_window = true;
    constexpr ImVec4 clearColor{0.45f, 0.55f, 0.60f, 1.00f};

    for (auto done = false;;) {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;

        if (s_swapChainOccluded && s_pSwapChain->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED) {
            Sleep(10);
            continue;
        }

        s_swapChainOccluded = false;

        if (s_resizeWidth != 0 && s_resizeHeight != 0) {
            CleanupRenderTarget();
            if (FAILED(s_pSwapChain->ResizeBuffers(0, s_resizeWidth, s_resizeHeight, DXGI_FORMAT_UNKNOWN, 0)))
                break;

            s_resizeWidth = s_resizeHeight = 0;
            CreateRenderTarget();
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        if (show_demo_window)
            ImGui::ShowDemoWindow(&show_demo_window);
        if (show_text_window)
            ShowTextInterfaceWindow(&show_text_window);

        ImGui::Render();
        constexpr float clearColorPremul[]{clearColor.x * clearColor.w, clearColor.y * clearColor.w, clearColor.z * clearColor.w, clearColor.w};
        s_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        s_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clearColorPremul);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        s_swapChainOccluded = s_pSwapChain->Present(1, 0) == DXGI_STATUS_OCCLUDED;
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ExampleText::DestroyFont(font);
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    DestroyWindow(hwnd);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);

    return 0;
}

namespace {
    void ShowTextInterfaceWindow(bool* pOpen) {
        if (!ImGui::Begin("Text interface", pOpen)) {
            ImGui::End();
            return;
        }

        const auto m = ImGui::GetFontMetrics();
        ImGui::Text("Metrics: ascent %.1f, descent %.1f, line height %.1f", m.Ascent, m.Descent, m.LineHeight);
        ImGui::Separator();

        ImGui::TextWrapped("Wrapped text: Lorem ipsum dolor sit amet, consectetur adipiscing elit. Quisque eu neque at odio dapibus porta. Proin tincidunt justo sollicitudin, mollis velit quis, rutrum nulla. Sed imperdiet dapibus leo et aliquet. In eros mauris, feugiat eget ipsum at, elementum eleifend ex. Cras malesuada sapien sed justo congue, a tempus est consequat. Nam eu ligula fringilla, congue sem vitae, ullamcorper justo. Aliquam sit amet dolor arcu. Donec nec metus aliquam, faucibus mauris eget, auctor tellus. Aliquam consectetur elit malesuada lectus rhoncus, id laoreet erat lacinia. Aliquam dictum, sem eget malesuada auctor, libero nisl malesuada velit, ut malesuada tortor neque eu massa. Nam vestibulum nisl semper lacus fringilla sodales. Nam quis ipsum sodales, venenatis odio non, pulvinar nunc. Nullam vehicula eros leo, vulputate suscipit magna porttitor at. Fusce a maximus felis. Cras finibus commodo pretium.");

        static char password[1024] = "password";
        ImGui::InputText("Password", password, sizeof(password), ImGuiInputTextFlags_Password);
        static char multiline[1024] = "Lorem ipsum dolor sit amet, consectetur adipiscing elit.\nQuisque eu neque at odio dapibus porta.\nProin tincidunt justo sollicitudin, mollis velit quis, rutrum nulla.\nSed imperdiet dapibus leo et aliquet.\nIn eros mauris, feugiat eget ipsum at, elementum eleifend ex.\nCras malesuada sapien sed justo congue, a tempus est consequat.\nNam eu ligula fringilla, congue sem vitae, ullamcorper justo.\nAliquam sit amet dolor arcu.\nDonec nec metus aliquam, faucibus mauris eget, auctor tellus.\nAliquam consectetur elit malesuada lectus rhoncus, id laoreet erat lacinia.\nAliquam dictum, sem eget malesuada auctor, libero nisl malesuada velit, ut malesuada tortor neque eu massa.\nNam vestibulum nisl semper lacus fringilla sodales.\nNam quis ipsum sodales, venenatis odio non, pulvinar nunc.\nNullam vehicula eros leo, vulputate suscipit magna porttitor at.\nFusce a maximus felis.\nCras finibus commodo pretium.\n";
        ImGui::InputTextMultiline("Multiline", multiline, sizeof(multiline), ImVec2(0, ImGui::GetTextLineHeight() * 4));
        static char wrapped[1024] = "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Quisque eu neque at odio dapibus porta. Proin tincidunt justo sollicitudin, mollis velit quis, rutrum nulla. Sed imperdiet dapibus leo et aliquet. In eros mauris, feugiat eget ipsum at, elementum eleifend ex. Cras malesuada sapien sed justo congue, a tempus est consequat. Nam eu ligula fringilla, congue sem vitae, ullamcorper justo. Aliquam sit amet dolor arcu. Donec nec metus aliquam, faucibus mauris eget, auctor tellus. Aliquam consectetur elit malesuada lectus rhoncus, id laoreet erat lacinia. Aliquam dictum, sem eget malesuada auctor, libero nisl malesuada velit, ut malesuada tortor neque eu massa. Nam vestibulum nisl semper lacus fringilla sodales. Nam quis ipsum sodales, venenatis odio non, pulvinar nunc. Nullam vehicula eros leo, vulputate suscipit magna porttitor at. Fusce a maximus felis. Cras finibus commodo pretium.";
        ImGui::InputTextMultiline("Wrapped", wrapped, sizeof(wrapped), ImVec2(0, ImGui::GetTextLineHeight() * 4), ImGuiInputTextFlags_WordWrap);

        if (ImGui::BeginTable("##ellipsis", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingFixedFit)) {
            ImGui::TableSetupColumn("Long long long long long long long header", ImGuiTableColumnFlags_WidthFixed, 80.0f);
            ImGui::TableSetupColumn("Not as long header", ImGuiTableColumnFlags_WidthFixed, 60.0f);
            ImGui::TableSetupColumn("Short");
            ImGui::TableHeadersRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("1");
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("2");
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("3");
            ImGui::EndTable();
        }

        {
            static auto angle = 0.3f;
            ImGui::SliderAngle("Rotation", &angle);
            const auto pos = ImGui::GetCursorScreenPos() + ImVec2(20.0f, 10.0f);
            ImTextDesc desc;
            ImGui::GetTextDesc(&desc);
            const ImTextTransform t{ImVec2(ImCos(angle), ImSin(angle)), ImVec2(-ImSin(angle), ImCos(angle))};
            ImGui::AddTextEx(ImGui::GetWindowDrawList(), NULL, &desc, pos, ImGui::GetColorU32(ImGuiCol_Text), "Transformed text", NULL, NULL, 0, &t);
            ImGui::Dummy(ImVec2(200.0f, ImGui::GetTextLineHeight() * 3));
        }

        {
            ImGui::SeparatorText("Selection");
            static auto sel_anchor = 0, sel_caret = 0;
            constexpr auto text = "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Quisque eu neque at odio dapibus porta.\nProin tincidunt justo sollicitudin, mollis velit quis, rutrum nulla.\nSed imperdiet dapibus leo et aliquet.\n\nIn eros mauris, feugiat eget ipsum at, elementum eleifend ex. Cras malesuada sapien sed justo congue, a tempus est consequat. Nam eu ligula fringilla, congue sem vitae, ullamcorper justo. Aliquam sit amet dolor arcu. Donec nec metus aliquam, faucibus mauris eget, auctor tellus. Aliquam consectetur elit malesuada lectus rhoncus, id laoreet erat lacinia. Aliquam dictum, sem eget malesuada auctor, libero nisl malesuada velit, ut malesuada tortor neque eu massa. Nam vestibulum nisl semper lacus fringilla sodales. Nam quis ipsum sodales, venenatis odio non, pulvinar nunc. Nullam vehicula eros leo, vulputate suscipit magna porttitor at. Fusce a maximus felis. Cras finibus commodo pretium.";
            const auto wrap_width = ImMax(ImGui::GetContentRegionAvail().x, 50.0f);
            ImTextDesc desc;
            ImGui::GetTextDesc(&desc, wrap_width);
            ImTextLayout* layout = NULL;
            if (ImGui::CreateTextLayout(ImGui::GetFont(), &desc, text, NULL, &layout) >= 0) {
                ImVec4 box;
                layout->lpVtbl->GetBoundingBox(layout, &box, NULL);
                const ImVec2 pos = ImGui::GetCursorScreenPos();
                ImGui::InvisibleButton("##selection", ImVec2(wrap_width, box.w));
                ImTextCaret hit;
                layout->lpVtbl->HitTest(layout, ImGui::GetIO().MousePos - pos, &hit);
                if (ImGui::IsItemActivated())
                    sel_anchor = sel_caret = hit.TextOffset;
                else if (ImGui::IsItemActive())
                    sel_caret = hit.TextOffset;

                const auto drawList = ImGui::GetWindowDrawList();
                ImVec4 rects[16];
                const int count = layout->lpVtbl->GetRangeRects(layout, ImMin(sel_anchor, sel_caret), ImMax(sel_anchor, sel_caret), rects, IM_ARRAYSIZE(rects));
                for (int n = 0; n < count && n < IM_ARRAYSIZE(rects); n++)
                    drawList->AddRectFilled(pos + ImVec2(rects[n].x, rects[n].y), pos + ImVec2(rects[n].z, rects[n].w), ImGui::GetColorU32(ImGuiCol_TextSelectedBg));
                ImGui::AddTextLayout(drawList, layout, pos, ImGui::GetColorU32(ImGui::IsItemHovered() ? ImGuiCol_Text : ImGuiCol_TextDisabled));
                ImTextCaret caret;
                layout->lpVtbl->GetCaret(layout, sel_caret, false, &caret);
                drawList->AddLine(pos + caret.Pos, pos + caret.Pos + ImVec2(0.0f, caret.Length), ImGui::GetColorU32(ImGuiCol_Text));
                layout->lpVtbl->Release(layout);
            }
            ImGui::Text("Selection: %d..%d", ImMin(sel_anchor, sel_caret), ImMax(sel_anchor, sel_caret));
        }

        ImGui::End();
    }

    bool CreateDeviceD3D(HWND hWnd) {
        const DXGI_SWAP_CHAIN_DESC sd{
            .BufferDesc = {
                .Width = 0,
                .Height = 0,
                .RefreshRate = {
                    .Numerator = 60,
                    .Denominator = 1,
                },
                .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
                .ScanlineOrdering = {},
                .Scaling = {},
            },
            .SampleDesc = {
                .Count = 1,
                .Quality = 0,
            },
            .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
            .BufferCount = 2,
            .OutputWindow = hWnd,
            .Windowed = TRUE,
            .SwapEffect = DXGI_SWAP_EFFECT_DISCARD,
            .Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH,
        };

        D3D_FEATURE_LEVEL featureLevel;
        constexpr UINT createDeviceFlags = 0;
        constexpr D3D_FEATURE_LEVEL featureLevelArray[]{D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0,};
        if (D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &s_pSwapChain, &s_pd3dDevice, &featureLevel, &s_pd3dDeviceContext) != S_OK) {
            if (D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &s_pSwapChain, &s_pd3dDevice, &featureLevel, &s_pd3dDeviceContext) != S_OK)
                return false;
        }

        if (IDXGIFactory* pSwapChainFactory; SUCCEEDED(s_pSwapChain->GetParent(IID_PPV_ARGS(&pSwapChainFactory)))) {
            const auto failed = FAILED(pSwapChainFactory->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER));
            pSwapChainFactory->Release();
            if (failed)
                return false;
        }

        CreateRenderTarget();
        return true;
    }

    void CleanupDeviceD3D() {
        CleanupRenderTarget();
        if (s_pSwapChain) {
            s_pSwapChain->Release();
            s_pSwapChain = nullptr;
        }
        if (s_pd3dDeviceContext) {
            s_pd3dDeviceContext->Release();
            s_pd3dDeviceContext = nullptr;
        }
        if (s_pd3dDevice) {
            s_pd3dDevice->Release();
            s_pd3dDevice = nullptr;
        }
    }

    void CreateRenderTarget() {
        ID3D11Texture2D* pBackBuffer;
        s_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
        s_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
        pBackBuffer->Release();
    }

    void CleanupRenderTarget() {
        if (g_mainRenderTargetView) {
            g_mainRenderTargetView->Release();
            g_mainRenderTargetView = nullptr;
        }
    }

    LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
            return true;

        switch (msg) {
            case WM_SIZE:
                if (wParam == SIZE_MINIMIZED)
                    return 0;
                s_resizeWidth = static_cast<UINT>(LOWORD(lParam));
                s_resizeHeight = static_cast<UINT>(HIWORD(lParam));
                return 0;
            case WM_DESTROY:
                PostQuitMessage(0);
                return 0;
        }
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
}
