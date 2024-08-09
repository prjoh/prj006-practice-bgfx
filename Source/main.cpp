#include <iostream>
#include <memory>

#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_syswm.h>

#include <bgfx/bgfx.h>
#include <bgfx/platform.h>
#include <bx/math.h>
#include <bx/timer.h>

#include <imgui.h>
#include <backends/imgui_impl_sdl2.h>
#include <imgui_impl_bgfx.h>

#include <Camera.h>
#include <Geometries.h>
#include <Materials.h>
#include <Input.h>
#include <Loading.h>
#include <Mesh.h>
#include <Types.h>
#include <Utils.h>


using namespace zv;


int main(int argc, char* argv[])
{
    ///////////////////
    // Init Window

    LoadingManager::init();

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cout << "SDL could not initialize. SDL_Error: " << SDL_GetError() << "\n";
        return 1;
    }

    const s32 width = 1280;
    const s32 height = 720;
    SDL_Window* window = SDL_CreateWindow(
        "bgfx starter", 
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 
        width, height,
        SDL_WINDOW_SHOWN);

    if (window == nullptr) {
        std::cout << "Window could not be created. SDL_Error: " << SDL_GetError() << "\n";
        return 1;
    }

    //SDL_SetRelativeMouseMode(SDL_TRUE);  // https://github.com/libsdl-org/SDL/issues/5992
    //SDL_ShowCursor(SDL_FALSE);

#if !BX_PLATFORM_EMSCRIPTEN
    SDL_SysWMinfo wmi;
    SDL_VERSION(&wmi.version);
    if (!SDL_GetWindowWMInfo(window, &wmi)) {
        std::cout << "SDL_SysWMinfo could not be retrieved. SDL_Error: " << SDL_GetError() << "\n";
        return 1;
    }
    bgfx::renderFrame(); // single threaded mode
#endif

    bgfx::PlatformData pd{};
#if BX_PLATFORM_WINDOWS
    pd.nwh = wmi.info.win.window;
#elif BX_PLATFORM_OSX
    pd.nwh = wmi.info.cocoa.window;
#elif BX_PLATFORM_LINUX
    pd.ndt = wmi.info.x11.display;
    pd.nwh = (void*)(us32ptr_t)wmi.info.x11.window;
#elif BX_PLATFORM_EMSCRIPTEN
    pd.nwh = (void*)"#canvas";
#endif

    bgfx::Init bgfx_init;
    bgfx_init.type = bgfx::RendererType::Count; // auto choose renderer
    bgfx_init.resolution.width = width;
    bgfx_init.resolution.height = height;
    bgfx_init.resolution.reset = BGFX_RESET_VSYNC | BGFX_RESET_MSAA_X16 | BGFX_RESET_MAXANISOTROPY;
    bgfx_init.platformData = pd;
    bgfx::init(bgfx_init);

    bgfx::setViewClear(
        0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x909090FF, 1.0f, 0);
    bgfx::setViewRect(0, 0, 0, width, height);

    ImGui::CreateContext();

    ImGui_Implbgfx_Init(255);
#if BX_PLATFORM_WINDOWS
    ImGui_ImplSDL2_InitForD3D(window);
#elif BX_PLATFORM_OSX
    ImGui_ImplSDL2_InitForMetal(window);
#elif BX_PLATFORM_LINUX || BX_PLATFORM_EMSCRIPTEN
    ImGui_ImplSDL2_InitForOpenGL(window, nullptr);
#endif

    ///////////////////
    // Load resources

    // Load textures
    bgfx::TextureHandle textureColor = LoadingManager::loadTexture("Assets/Textures/fieldstone-rgba.dds");
    bgfx::TextureHandle textureNormal = LoadingManager::loadTexture("Assets/Textures/fieldstone-n.dds");

    // Load shaders
    bgfx::ProgramHandle program = LoadingManager::loadProgram("Assets/Shaders/test_v.bin", "Assets/Shaders/test_f.bin");

    ///////////////////
    // Setup scene
    const bx::Vec3 at = { 0.0f, 0.0f,  0.0f };
    const bx::Vec3 eye = { 0.0f, 0.0f, -7.0f };
    Camera camera{ eye, at, g_WorldUp, (f32)width / (f32)height, 60.0f, 0.01f, 1000.0f };

    f32 time = 0.0f;

    Mesh testPlane(
        std::make_unique<PlaneGeometry>(5.0f, 5.0f),
        std::make_unique<TestMaterial>(program, textureColor, textureNormal, &time)
    );

    Mesh testCube(
        std::make_unique<CubeGeometry>(2.0f, 2.0f, 2.0f),
        std::make_unique<TestMaterial>(program, textureColor, textureNormal, &time)
    );

    Mesh testCylinder(
        std::make_unique<CylinderGeometry>(3.0f, 3.0f, 6.0f),
        std::make_unique<TestMaterial>(program, textureColor, textureNormal, &time)
    );

    ///////////////////
    // Main Loop

    while (!Input::quitEvent())
    {
        Input::update();

        s64 now = bx::getHPCounter();
        static s64 last = now;
        const s64 frameTime = now - last;
        last = now;
        const f64 freq = f64(bx::getHPFrequency());
        const f32 deltaTimeS = f32(frameTime / freq);

        ImGui_Implbgfx_NewFrame();
        ImGui_ImplSDL2_NewFrame();

        ImGui::NewFrame();
        ImGui::ShowDemoWindow(); // your drawing here
        ImGui::Render();
        ImGui_Implbgfx_RenderDrawLists(ImGui::GetDrawData());

        camera.update(deltaTimeS);

        // Set view and projection matrix for view 0.
        {
            bgfx::setViewTransform(0, camera.viewMatrix(), camera.projectionMatrix());

            // Set view 0 default viewport.
            bgfx::setViewRect(0, 0, 0, u16(width), u16(height));
        }

        // Update primitives
        time += deltaTimeS;
        testPlane.render();
        testCube.render();
        testCylinder.render();

        // Advance to next frame. Rendering thread will be kicked to
        // process submitted rendering primitives.
        bgfx::frame();
    }

    ///////////////////
    // Cleanup

    ImGui_ImplSDL2_Shutdown();
    ImGui_Implbgfx_Shutdown();

    ImGui::DestroyContext();

    // Destroy scene objects
    testCylinder.cleanup();
    testCube.cleanup();
    testPlane.cleanup();

    // Destroy resources
    bgfx::destroy(program);
    bgfx::destroy(textureColor);
    bgfx::destroy(textureNormal);

    // Shutdown
    bgfx::shutdown();
    SDL_DestroyWindow(window);
    SDL_Quit();

    LoadingManager::quit();

    return 0;
}
