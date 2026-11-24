#include "core/log.h"
#include "game/race.h"
#include "gfx/capture.h"
#include "gfx/gpu.h"
#include "gfx/renderer.h"
#include "gfx/swapchain.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {

void logGpuStats(const gfx::Renderer& renderer) {
    const gfx::GpuStats& st = renderer.stats;
    if (st.samples == 0) return;
    logInfo("GPU par image: %.2f ms (reflet %.2f, scène %.2f, bloom %.2f, composition %.2f), "
            "scène en %ux%u",
            st.averageTotalMs(), st.averageMs(0), st.averageMs(1), st.averageMs(2),
            st.averageMs(3), renderer.sceneExtent.width, renderer.sceneExtent.height);
}

struct Options {
    bool headless = false;
    bool validation = false;
    bool vsync = true;
    int width = 1600;
    int height = 900;
    int frames = 0;      // 0 : boucle jusqu'à la fermeture
    std::string out;     // chemin PNG ; contient %d pour écrire chaque image
    float fps = 60.0f;   // pas de temps du mode sans fenêtre
    float startTime = 0.0f;
    bool hud = true;
    bool autopilot = false;
    bool stats = false;
    int gridStart = -1;  // -1 : selon le mode, 0 : départ lancé, 1 : grille
    gfx::RenderSettings render;
};

Options parseOptions(int argc, char** argv) {
    Options o;
#ifndef NDEBUG
    o.validation = true;
#endif
    for (int i = 1; i < argc; i++) {
        const char* a = argv[i];
        auto value = [&]() -> const char* {
            if (i + 1 >= argc) fatal("Option %s: valeur manquante", a);
            return argv[++i];
        };
        if (!std::strcmp(a, "--headless")) o.headless = true;
        else if (!std::strcmp(a, "--validation")) o.validation = true;
        else if (!std::strcmp(a, "--no-validation")) o.validation = false;
        else if (!std::strcmp(a, "--no-vsync")) o.vsync = false;
        else if (!std::strcmp(a, "--frames")) o.frames = std::atoi(value());
        else if (!std::strcmp(a, "--out")) o.out = value();
        else if (!std::strcmp(a, "--fps")) o.fps = static_cast<float>(std::atof(value()));
        else if (!std::strcmp(a, "--time")) o.startTime = static_cast<float>(std::atof(value()));
        else if (!std::strcmp(a, "--scale")) o.render.renderScale = static_cast<float>(std::atof(value()));
        else if (!std::strcmp(a, "--no-hud")) o.hud = false;
        else if (!std::strcmp(a, "--autopilot")) o.autopilot = true;
        else if (!std::strcmp(a, "--stats")) o.stats = true;
        else if (!std::strcmp(a, "--grid-start")) o.gridStart = 1;
        else if (!std::strcmp(a, "--rolling-start")) o.gridStart = 0;
        else if (!std::strcmp(a, "--no-bloom")) o.render.bloom = false;
        else if (!std::strcmp(a, "--no-reflections")) o.render.reflections = false;
        else if (!std::strcmp(a, "--size")) {
            if (std::sscanf(value(), "%dx%d", &o.width, &o.height) != 2)
                fatal("Option --size: format attendu LxH");
        } else fatal("Option inconnue: %s", a);
    }
    if (o.headless && o.frames <= 0) o.frames = 1;
    return o;
}

using gfx::kFramesInFlight;

struct FrameData {
    VkCommandPool pool = VK_NULL_HANDLE;
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    VkFence fence = VK_NULL_HANDLE;
    VkSemaphore acquired = VK_NULL_HANDLE;
};

void createFrames(gfx::Gpu& gpu, FrameData* frames) {
    for (int i = 0; i < kFramesInFlight; i++) {
        VkCommandPoolCreateInfo pci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        pci.queueFamilyIndex = gpu.queueFamily;
        VK_CHECK(vkCreateCommandPool(gpu.device, &pci, nullptr, &frames[i].pool));

        VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        ai.commandPool = frames[i].pool;
        ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        ai.commandBufferCount = 1;
        VK_CHECK(vkAllocateCommandBuffers(gpu.device, &ai, &frames[i].cmd));

        VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        VK_CHECK(vkCreateFence(gpu.device, &fci, nullptr, &frames[i].fence));

        VkSemaphoreCreateInfo sci{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        VK_CHECK(vkCreateSemaphore(gpu.device, &sci, nullptr, &frames[i].acquired));
    }
}

void destroyFrames(gfx::Gpu& gpu, FrameData* frames) {
    for (int i = 0; i < kFramesInFlight; i++) {
        vkDestroySemaphore(gpu.device, frames[i].acquired, nullptr);
        vkDestroyFence(gpu.device, frames[i].fence, nullptr);
        vkDestroyCommandPool(gpu.device, frames[i].pool, nullptr);
    }
}

VkCommandBuffer beginFrame(gfx::Gpu& gpu, FrameData& frame) {
    VK_CHECK(vkWaitForFences(gpu.device, 1, &frame.fence, VK_TRUE, UINT64_MAX));
    VK_CHECK(vkResetCommandPool(gpu.device, frame.pool, 0));
    VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(frame.cmd, &bi));
    return frame.cmd;
}

void submitFrame(gfx::Gpu& gpu, FrameData& frame, VkSemaphore wait, VkSemaphore signal) {
    VK_CHECK(vkEndCommandBuffer(frame.cmd));

    VkCommandBufferSubmitInfo cmdInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO};
    cmdInfo.commandBuffer = frame.cmd;
    VkSemaphoreSubmitInfo waitInfo{VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO};
    waitInfo.semaphore = wait;
    waitInfo.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSemaphoreSubmitInfo signalInfo{VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO};
    signalInfo.semaphore = signal;

    VkSubmitInfo2 submit{VK_STRUCTURE_TYPE_SUBMIT_INFO_2};
    submit.commandBufferInfoCount = 1;
    submit.pCommandBufferInfos = &cmdInfo;
    if (wait) {
        submit.waitSemaphoreInfoCount = 1;
        submit.pWaitSemaphoreInfos = &waitInfo;
    }
    if (signal) {
        submit.signalSemaphoreInfoCount = 1;
        submit.pSignalSemaphoreInfos = &signalInfo;
    }
    VK_CHECK(vkResetFences(gpu.device, 1, &frame.fence));
    VK_CHECK(vkQueueSubmit2(gpu.queue, 1, &submit, frame.fence));
}

// Rendu dans une image hors écran, sans fenêtre. Le temps avance d'un pas fixe,
// donc deux exécutions produisent les mêmes images.
int runHeadless(const Options& opt) {
    gfx::Gpu gpu;
    gpu.init(nullptr, opt.validation);

    const VkFormat format = VK_FORMAT_R8G8B8A8_SRGB;
    const VkExtent2D extent{static_cast<uint32_t>(opt.width), static_cast<uint32_t>(opt.height)};

    gfx::ImageDesc desc;
    desc.extent = extent;
    desc.format = format;
    desc.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    desc.name = "headless target";
    gfx::Image target = gpu.createImage(desc);

    gfx::Capture capture;
    capture.init(gpu, extent);

    game::Race race;
    race.init(opt.gridStart == 1);
    race.autopilot = true;
    // Avance jusqu'à l'instant demandé avant la première image.
    for (float t = 0.0f; t < opt.startTime; t += sim::kFixedDt)
        race.update(sim::kFixedDt, game::Controls{});

    gfx::Renderer renderer;
    renderer.init(gpu, format, race.track);
    renderer.settings = opt.render;

    FrameData frames[kFramesInFlight];
    createFrames(gpu, frames);

    const bool everyFrame = opt.out.find('%') != std::string::npos;
    int written = 0;
    Uint64 start = SDL_GetTicksNS();

    for (int i = 0; i < opt.frames; i++) {
        FrameData& frame = frames[i % kFramesInFlight];
        VkCommandBuffer cmd = beginFrame(gpu, frame);

        gfx::transition(cmd, target.image, VK_IMAGE_LAYOUT_UNDEFINED,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        gfx::FrameTarget ft{target.view, extent, extent, 0};
        gfx::FrameParams params;
        params.slot = i % kFramesInFlight;
        race.update(1.0f / opt.fps, game::Controls{});
        race.fill(renderer.fonts,
                  {static_cast<float>(extent.width), static_cast<float>(extent.height)}, params);
        if (!opt.hud) params.hud = nullptr;
        renderer.draw(cmd, ft, params);

        const bool save = !opt.out.empty() && (everyFrame || i == opt.frames - 1);
        if (save) {
            gfx::transition(cmd, target.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
            capture.recordCopy(cmd, target);
        }
        submitFrame(gpu, frame, VK_NULL_HANDLE, VK_NULL_HANDLE);

        if (save) {
            VK_CHECK(vkWaitForFences(gpu.device, 1, &frame.fence, VK_TRUE, UINT64_MAX));
            char path[1024];
            std::snprintf(path, sizeof(path), opt.out.c_str(), i);
            if (!capture.writePng(path)) fatal("Écriture impossible: %s", path);
            written++;
        }
    }

    VK_CHECK(vkDeviceWaitIdle(gpu.device));
    double seconds = static_cast<double>(SDL_GetTicksNS() - start) * 1e-9;
    logInfo("%d images rendues en %.3f s, %d écrites, %u erreurs de validation", opt.frames,
            seconds, written, gpu.validationErrors);

    logGpuStats(renderer);

    uint32_t errors = gpu.validationErrors;
    destroyFrames(gpu, frames);
    renderer.shutdown();
    capture.shutdown(gpu);
    gpu.destroyImage(target);
    gpu.shutdown();
    return errors == 0 ? 0 : 2;
}

int runWindowed(const Options& opt) {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) fatal("SDL_Init: %s", SDL_GetError());

    SDL_WindowFlags flags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE |
                            SDL_WINDOW_HIGH_PIXEL_DENSITY;
#ifdef __ANDROID__
    flags |= SDL_WINDOW_FULLSCREEN;
#endif
    SDL_Window* window = SDL_CreateWindow("Antigrav", opt.width, opt.height, flags);
    if (!window) fatal("SDL_CreateWindow: %s", SDL_GetError());

    gfx::Gpu gpu;
    gpu.init(window, opt.validation);

    auto pixelSize = [&](uint32_t& w, uint32_t& h) {
        int pw = 0, ph = 0;
        SDL_GetWindowSizeInPixels(window, &pw, &ph);
        w = static_cast<uint32_t>(pw);
        h = static_cast<uint32_t>(ph);
    };

    gfx::Swapchain swapchain;
    uint32_t w = 0, h = 0;
    pixelSize(w, h);
    bool haveSwapchain = swapchain.create(gpu, w, h, opt.vsync);
    if (!haveSwapchain) fatal("Surface de taille nulle au démarrage");

    game::Race race;
    race.init(opt.gridStart != 0);
    race.autopilot = opt.autopilot;
    game::InputReader input;

    gfx::Renderer renderer;
    renderer.init(gpu, swapchain.format, race.track);
    renderer.settings = opt.render;

    FrameData frames[kFramesInFlight];
    createFrames(gpu, frames);

    bool running = true;
    bool background = false;
    bool needResize = false;
    uint64_t frameIndex = 0;
    Uint64 start = SDL_GetTicksNS();
    Uint64 lastTick = start;
    Uint64 statsTick = 0;
    double cpuMsSmoothed = 16.0;
    char statsText[160] = "";

    while (running) {
        glm::vec2 screen{static_cast<float>(swapchain.logicalExtent.width),
                         static_cast<float>(swapchain.logicalExtent.height)};
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            input.handleEvent(event, screen);
            switch (event.type) {
            case SDL_EVENT_QUIT:
                running = false;
                break;
            case SDL_EVENT_KEY_DOWN:
                if (event.key.key == SDLK_ESCAPE) running = false;
                break;
            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                needResize = true;
                break;
            case SDL_EVENT_WILL_ENTER_BACKGROUND:
                // Android détruit la fenêtre native : la surface et la
                // swapchain doivent disparaître avant.
                if (!background) {
                    VK_CHECK(vkDeviceWaitIdle(gpu.device));
                    swapchain.destroy(gpu);
                    gpu.destroySurface();
                    haveSwapchain = false;
                    background = true;
                }
                break;
            case SDL_EVENT_DID_ENTER_FOREGROUND:
                if (background) {
                    gpu.createSurface(window);
                    background = false;
                    needResize = true;
                }
                break;
            default:
                break;
            }
        }
        if (!running) break;
        if (background) {
            SDL_Delay(50);
            continue;
        }

        if (needResize || !haveSwapchain) {
            VK_CHECK(vkDeviceWaitIdle(gpu.device));
            pixelSize(w, h);
            haveSwapchain = swapchain.create(gpu, w, h, opt.vsync);
            needResize = false;
            if (!haveSwapchain) {
                SDL_Delay(50);
                continue;
            }
        }

        FrameData& frame = frames[frameIndex % kFramesInFlight];
        VK_CHECK(vkWaitForFences(gpu.device, 1, &frame.fence, VK_TRUE, UINT64_MAX));

        uint32_t imageIndex = 0;
        VkResult acquire = vkAcquireNextImageKHR(gpu.device, swapchain.handle, UINT64_MAX,
                                                 frame.acquired, VK_NULL_HANDLE, &imageIndex);
        if (acquire == VK_ERROR_OUT_OF_DATE_KHR || acquire == VK_ERROR_SURFACE_LOST_KHR) {
            needResize = true;
            continue;
        }
        if (acquire != VK_SUCCESS && acquire != VK_SUBOPTIMAL_KHR)
            gfx::vkFail("vkAcquireNextImageKHR", acquire, __FILE__, __LINE__);

        VkCommandBuffer cmd = beginFrame(gpu, frame);
        gfx::transition(cmd, swapchain.images[imageIndex], VK_IMAGE_LAYOUT_UNDEFINED,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        gfx::FrameTarget ft{swapchain.views[imageIndex], swapchain.extent,
                            swapchain.logicalExtent, swapchain.rotation};
        Uint64 now = SDL_GetTicksNS();
        float dt = static_cast<float>(static_cast<double>(now - lastTick) * 1e-9);
        lastTick = now;
        screen = {static_cast<float>(swapchain.logicalExtent.width),
                  static_cast<float>(swapchain.logicalExtent.height)};
        race.update(dt, input.read(screen));

        // Mesures lissées, rafraîchies deux fois par seconde.
        cpuMsSmoothed += (static_cast<double>(dt) * 1000.0 - cpuMsSmoothed) * 0.05;
        if (opt.stats && now - statsTick > 500000000ull) {
            statsTick = now;
            std::snprintf(statsText, sizeof(statsText),
                          "GPU %.1f ms   image %.1f ms   %.0f i/s   3D %ux%u",
                          renderer.stats.lastTotalMs(), cpuMsSmoothed, 1000.0 / cpuMsSmoothed,
                          renderer.sceneExtent.width, renderer.sceneExtent.height);
        }
        race.statsLine = statsText;

        gfx::FrameParams params;
        params.slot = static_cast<int>(frameIndex % kFramesInFlight);
        race.fill(renderer.fonts, screen, params);
        if (!opt.hud) params.hud = nullptr;
        renderer.draw(cmd, ft, params);

        gfx::transition(cmd, swapchain.images[imageIndex],
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
        submitFrame(gpu, frame, frame.acquired, swapchain.renderDone[imageIndex]);

        VkPresentInfoKHR present{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
        present.waitSemaphoreCount = 1;
        present.pWaitSemaphores = &swapchain.renderDone[imageIndex];
        present.swapchainCount = 1;
        present.pSwapchains = &swapchain.handle;
        present.pImageIndices = &imageIndex;
        VkResult presented = vkQueuePresentKHR(gpu.queue, &present);
        if (presented == VK_ERROR_OUT_OF_DATE_KHR || presented == VK_SUBOPTIMAL_KHR ||
            presented == VK_ERROR_SURFACE_LOST_KHR)
            needResize = true;
        else if (presented != VK_SUCCESS)
            gfx::vkFail("vkQueuePresentKHR", presented, __FILE__, __LINE__);

        frameIndex++;
        if (opt.frames > 0 && frameIndex >= static_cast<uint64_t>(opt.frames)) running = false;
    }

    VK_CHECK(vkDeviceWaitIdle(gpu.device));
    double seconds = static_cast<double>(SDL_GetTicksNS() - start) * 1e-9;
    logInfo("%llu images en %.2f s, %.1f images par seconde",
            static_cast<unsigned long long>(frameIndex), seconds,
            seconds > 0.0 ? static_cast<double>(frameIndex) / seconds : 0.0);
    logGpuStats(renderer);

    destroyFrames(gpu, frames);
    renderer.shutdown();
    swapchain.destroy(gpu);
    gpu.shutdown();
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    Options opt = parseOptions(argc, argv);
    return opt.headless ? runHeadless(opt) : runWindowed(opt);
}
