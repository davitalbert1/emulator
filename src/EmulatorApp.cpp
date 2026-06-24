#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_sdlrenderer2.h>

#include "EmulatorApp.h"
#include "GameBoyAdvance.h"
#include "GameBoy.h"
#include "GameBoyColor.h"
#include "NintendoDS.h"
#include "Nintendo2DS.h"
#include "Nintendo3DS.h"
#include "Atari2600.h"
#include "NES.h"
#include "SNES.h"
#include "SegaGenesis.h"

#include <iostream>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#include <SDL_syswm.h>
#endif

#ifdef _WIN32
static HWND getHWNDFromSDLWindow(SDL_Window* window) {
    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    if (SDL_GetWindowWMInfo(window, &wmInfo)) {
        return wmInfo.info.win.window;
    }
    return NULL;
}
#endif

static std::string openFileDialog(HWND hwndOwner, const char* filter) {
    #ifdef _WIN32
    char filename[1024] = "";
    OPENFILENAMEA ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwndOwner;
    ofn.lpstrFile = filename;
    ofn.nMaxFile = sizeof(filename);
    ofn.lpstrFilter = filter;
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameA(&ofn)) {
        return std::string(filename);
    }
    #else
    (void)hwndOwner;
    (void)filter;
    #endif
    return "";
}

EmulatorApp::EmulatorApp() 
    : window(nullptr), renderer(nullptr), lcdTexture(nullptr), audioDevice(0),
      activeConsole(nullptr), activeConsoleIndex(0),
      appRunning(false), emulationPlaying(false), showConsoleSelector(true), executionSpeed(1) {
    romPathInput[0] = '\0';
}

EmulatorApp::~EmulatorApp() {
    shutdown();
}

bool EmulatorApp::init() {
    // Initialize SDL2
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_AUDIO) < 0) {
        std::cerr << "Erro ao inicializar o SDL2: " << SDL_GetError() << "\n";
        return false;
    }

    // Initialize SDL Audio Device
    SDL_AudioSpec desiredSpec;
    SDL_zero(desiredSpec);
    desiredSpec.freq = 44100;
    desiredSpec.format = AUDIO_F32SYS;
    desiredSpec.channels = 1;
    desiredSpec.samples = 512;
    desiredSpec.callback = nullptr;

    audioDevice = SDL_OpenAudioDevice(nullptr, 0, &desiredSpec, nullptr, 0);
    if (audioDevice == 0) {
        std::cerr << "Erro ao abrir dispositivo de audio SDL: " << SDL_GetError() << "\n";
    } else {
        SDL_PauseAudioDevice(audioDevice, 0);
    }

    // Create desktop window
    window = SDL_CreateWindow(
        "Antigravity Multi-Console Emulator",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        1280, 720,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );
    if (!window) {
        std::cerr << "Erro ao criar janela: " << SDL_GetError() << "\n";
        return false;
    }

    // Create hardware-accelerated renderer
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        std::cerr << "Erro ao criar renderizador: " << SDL_GetError() << "\n";
        return false;
    }

    // Initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    
    // Apply premium dark styling
    ImGui::StyleColorsDark();
    
    // Setup Platform/Renderer backends
    ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer2_Init(renderer);

    // Load Game Boy Advance as default console index 0
    loadConsole(0);
    
    appRunning = true;
    return true;
}

void EmulatorApp::loadConsole(int index) {
    activeConsoleIndex = index;
    emulationPlaying = false;

    // Reset console instance based on index
    if (index == 0) {
        activeConsole = std::make_unique<GameBoyAdvance>();
    } else if (index == 1) {
        activeConsole = std::make_unique<GameBoy>();
    } else if (index == 2) {
        activeConsole = std::make_unique<GameBoyColor>();
    } else if (index == 3) {
        activeConsole = std::make_unique<NintendoDS>();
    } else if (index == 4) {
        activeConsole = std::make_unique<Nintendo2DS>();
    } else if (index == 5) {
        activeConsole = std::make_unique<Nintendo3DS>();
    } else if (index == 6) {
        activeConsole = std::make_unique<Atari2600>();
    } else if (index == 7) {
        activeConsole = std::make_unique<NES>();
    } else if (index == 8) {
        activeConsole = std::make_unique<SNES>();
    } else if (index == 9) {
        activeConsole = std::make_unique<SegaGenesis>();
    }

    // Recreate SDL texture matching the console's native resolution
    if (lcdTexture) {
        SDL_DestroyTexture(lcdTexture);
        lcdTexture = nullptr;
    }

    int w = activeConsole->getScreenWidth();
    int h = activeConsole->getScreenHeight();

    pixelBuffer.assign(w * h, 0xFF000000); // Black clear
    
    lcdTexture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        w, h
    );

    if (!lcdTexture) std::cerr << "Erro ao criar textura do LCD: " << SDL_GetError() << "\n";

    // Sync input buffer text
    romPathInput[0] = '\0';
}

void EmulatorApp::loadROM(const std::string& path) {
    if (activeConsole && activeConsole->loadROM(path)) {
        std::strncpy(romPathInput, path.c_str(), sizeof(romPathInput) - 1);
        romPathInput[sizeof(romPathInput) - 1] = '\0';
        emulationPlaying = false; // Pause upon new load
    }
}

void EmulatorApp::handleEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL2_ProcessEvent(&event);

        if (event.type == SDL_QUIT) appRunning = false;
        
        if (event.type == SDL_WINDOWEVENT) {
            if (event.window.event == SDL_WINDOWEVENT_CLOSE && 
                event.window.windowID == SDL_GetWindowID(window)) {
                appRunning = false;
            }
        }
    }
}

void EmulatorApp::updateEmulation() {
    // If playing, step the emulator engine (executionSpeed acts as a speed multiplier)
    if (emulationPlaying && activeConsole) {
        for (int i = 0; i < executionSpeed; ++i) activeConsole->step();
    } else if (activeConsole && !activeConsole->isRunning()) {
        // Even when paused, run a graphics update for fallback animation if ROM is not running
        activeConsole->step();
    }

    // Fill screen texture with console display pixels
    if (activeConsole && lcdTexture) {
        activeConsole->drawLCD(pixelBuffer.data());
        updateLCDTexture();

        // Fetch and queue audio samples
        std::vector<float> samples;
        activeConsole->getAudioSamples(samples);
        if (!samples.empty() && audioDevice != 0) {
            // Limit audio queue size to prevent buffer bloat and lag (keep it under ~100ms)
            uint32_t queuedSize = SDL_GetQueuedAudioSize(audioDevice);
            uint32_t maxQueuedSize = 44100 * 0.10f * sizeof(float); // 100ms
            if (queuedSize < maxQueuedSize) {
                SDL_QueueAudio(audioDevice, samples.data(), samples.size() * sizeof(float));
            }
        }
    }
}

void EmulatorApp::updateLCDTexture() {
    if (lcdTexture && activeConsole) {
        int w = activeConsole->getScreenWidth();
        SDL_UpdateTexture(lcdTexture, nullptr, pixelBuffer.data(), w * sizeof(uint32_t));
    }
}

void EmulatorApp::renderImGui() {
    if (showConsoleSelector) {
        // Display launcher selector window in the center of the screen
        ImGui::SetNextWindowPos(ImVec2(320, 180), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(640, 360), ImGuiCond_Always);
        ImGui::Begin("Inicializador do Emulador Multi-Consoles", nullptr, 
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
        
        ImGui::Spacing();
        ImGui::Text("Bem-vindo! Selecione o console que deseja emular para iniciar:");
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::Columns(2, "launcher_columns", false);
        ImVec2 buttonSize(280, 35);

        // Column 1: Handhelds
        ImGui::Text("Consoles Portateis:");
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("Game Boy Advance (GBA)", buttonSize)) {
            loadConsole(0);
            showConsoleSelector = false;
        }
        ImGui::Spacing();
        if (ImGui::Button("Game Boy Classico (GB)", buttonSize)) {
            loadConsole(1);
            showConsoleSelector = false;
        }
        ImGui::Spacing();
        if (ImGui::Button("Game Boy Color (GBC)", buttonSize)) {
            loadConsole(2);
            showConsoleSelector = false;
        }
        ImGui::Spacing();
        if (ImGui::Button("Nintendo DS (NDS)", buttonSize)) {
            loadConsole(3);
            showConsoleSelector = false;
        }
        ImGui::Spacing();
        if (ImGui::Button("Nintendo 2DS", buttonSize)) {
            loadConsole(4);
            showConsoleSelector = false;
        }
        ImGui::Spacing();
        if (ImGui::Button("Nintendo 3DS", buttonSize)) {
            loadConsole(5);
            showConsoleSelector = false;
        }

        ImGui::NextColumn();

        // Column 2: Home Consoles
        ImGui::Text("Consoles de Mesa:");
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("Atari 2600", buttonSize)) {
            loadConsole(6);
            showConsoleSelector = false;
        }
        ImGui::Spacing();
        if (ImGui::Button("Nintendo (NES)", buttonSize)) {
            loadConsole(7);
            showConsoleSelector = false;
        }
        ImGui::Spacing();
        if (ImGui::Button("Super Nintendo (SNES)", buttonSize)) {
            loadConsole(8);
            showConsoleSelector = false;
        }
        ImGui::Spacing();
        if (ImGui::Button("Sega Genesis / Mega Drive", buttonSize)) {
            loadConsole(9);
            showConsoleSelector = false;
        }

        ImGui::Columns(1);
        ImGui::End();
        return; // Block rendering of other windows
    }

    // 1. Menu Bar
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("Arquivo")) {
            if (ImGui::MenuItem("Mudar de Console", "Ctrl+N")) showConsoleSelector = true;
            if (ImGui::MenuItem("Sair", "Alt+F4")) appRunning = false;
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Consoles")) {
            if (ImGui::MenuItem("Game Boy Advance", nullptr, activeConsoleIndex == 0)) loadConsole(0);
            if (ImGui::MenuItem("Game Boy", nullptr, activeConsoleIndex == 1)) loadConsole(1);
            if (ImGui::MenuItem("Game Boy Color", nullptr, activeConsoleIndex == 2)) loadConsole(2);
            if (ImGui::MenuItem("Nintendo DS", nullptr, activeConsoleIndex == 3)) loadConsole(3);
            if (ImGui::MenuItem("Nintendo 2DS", nullptr, activeConsoleIndex == 4)) loadConsole(4);
            if (ImGui::MenuItem("Nintendo 3DS", nullptr, activeConsoleIndex == 5)) loadConsole(5);
            if (ImGui::MenuItem("Atari 2600", nullptr, activeConsoleIndex == 6)) loadConsole(6);
            if (ImGui::MenuItem("NES", nullptr, activeConsoleIndex == 7)) loadConsole(7);
            if (ImGui::MenuItem("SNES", nullptr, activeConsoleIndex == 8)) loadConsole(8);
            if (ImGui::MenuItem("Sega Genesis", nullptr, activeConsoleIndex == 9)) loadConsole(9);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Ajuda")) {
            if (ImGui::MenuItem("Sobre o Emulador")) ImGui::OpenPopup("SobrePopup");
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    // Handle About modal
    if (ImGui::BeginPopupModal("SobrePopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Emulador Multi-Consoles C++");
        ImGui::Separator();
        ImGui::Text("Desenvolvido para Game Boy Advance, Game Boy e Game Boy Color.");
        ImGui::Text("Suporta emulação real de CPU/Vídeo/Áudio no Game Boy!");
        ImGui::Text("Interface Grafica construida com Dear ImGui + SDL2.");
        ImGui::Spacing();
        if (ImGui::Button("Fechar", ImVec2(120, 0))) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    // 2. ROM Loader Panel
    ImGui::SetNextWindowPos(ImVec2(10, 545), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(640, 140), ImGuiCond_Always);
    ImGui::Begin("Gerenciador de Arquivos ROM", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
    ImGui::Text("Carregue uma ROM compatível para o console ativo:");
    
    // We adjust input text width to fit the "Buscar..." button on the same line
    ImGui::PushItemWidth(450.0f);
    ImGui::InputText("##rompath", romPathInput, sizeof(romPathInput));
    ImGui::PopItemWidth();
    ImGui::SameLine();
    
    if (ImGui::Button("Buscar...")) {
        const char* filter = "";
        if (activeConsoleIndex == 0) filter = "GBA ROMs (*.gba)\0*.gba\0All Files (*.*)\0*.*\0";
        else if (activeConsoleIndex == 1) filter = "Game Boy ROMs (*.gb)\0*.gb\0All Files (*.*)\0*.*\0";
        else if (activeConsoleIndex == 2) filter = "Game Boy Color ROMs (*.gbc)\0*.gbc\0All Files (*.*)\0*.*\0";
        else if (activeConsoleIndex == 3) filter = "Nintendo DS ROMs (*.nds)\0*.nds\0All Files (*.*)\0*.*\0";
        else if (activeConsoleIndex == 4 || activeConsoleIndex == 5) filter = "3DS/2DS ROMs (*.3ds;*.cia)\0*.3ds;*.cia\0All Files (*.*)\0*.*\0";
        else if (activeConsoleIndex == 6) filter = "Atari 2600 ROMs (*.a26;*.bin)\0*.a26;*.bin\0All Files (*.*)\0*.*\0";
        else if (activeConsoleIndex == 7) filter = "NES ROMs (*.nes)\0*.nes\0All Files (*.*)\0*.*\0";
        else if (activeConsoleIndex == 8) filter = "SNES ROMs (*.sfc;*.smc)\0*.sfc;*.smc\0All Files (*.*)\0*.*\0";
        else if (activeConsoleIndex == 9) filter = "Sega Genesis ROMs (*.gen;*.md)\0*.gen;*.md\0All Files (*.*)\0*.*\0";
        
        HWND hwnd = NULL;
        #ifdef _WIN32
        hwnd = getHWNDFromSDLWindow(window);
        #endif
        std::string path = openFileDialog(hwnd, filter);
        if (!path.empty()) {
            loadROM(path);
        }
    }
    
    ImGui::Spacing();
    if (ImGui::Button("Carregar ROM")) loadROM(romPathInput);
    ImGui::SameLine();
    if (ImGui::Button("Resetar Console")) {
        if (activeConsole) {
            activeConsole->reset();
            emulationPlaying = false;
        }
    }
    
    ImGui::Separator();
    ImGui::Text("Console Atual: %s", activeConsole->getName().c_str());
    ImGui::End();

    // 3. LCD Screen Display Panel
    ImGui::SetNextWindowPos(ImVec2(10, 35), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(640, 500), ImGuiCond_Always);
    ImGui::Begin("Tela do Console (Simulado)", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
    if (lcdTexture && activeConsole) {
        int nativeW = activeConsole->getScreenWidth();
        int nativeH = activeConsole->getScreenHeight();
        
        // Dynamic scaling factor based on panel size, maintaining aspect ratio
        float scale = 3.0f; // Default 3x scale
        ImVec2 contentSize = ImGui::GetContentRegionAvail();
        if (contentSize.x > 0 && contentSize.y > 0) {
            float scaleX = contentSize.x / nativeW;
            float scaleY = contentSize.y / nativeH;
            scale = std::min(scaleX, scaleY);
            if (scale < 1.0f) scale = 1.0f;
        }

        ImVec2 textureSize(nativeW * scale, nativeH * scale);
        
        // Center texture inside the window
        ImVec2 cursorPadding((contentSize.x - textureSize.x) * 0.5f, (contentSize.y - textureSize.y) * 0.5f);
        if (cursorPadding.x < 0) cursorPadding.x = 0;
        if (cursorPadding.y < 0) cursorPadding.y = 0;
        ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX() + cursorPadding.x, ImGui::GetCursorPosY() + cursorPadding.y));

        ImGui::Image(
            reinterpret_cast<ImTextureID>(reinterpret_cast<intptr_t>(lcdTexture)),
            textureSize
        );
    } else {
        ImGui::Text("Nenhuma textura de tela instanciada.");
    }
    ImGui::End();

    // 4. Emulation Controls Panel
    ImGui::SetNextWindowPos(ImVec2(660, 35), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(300, 300), ImGuiCond_Always);
    ImGui::Begin("Painel de Controle da CPU", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
    
    if (emulationPlaying) {
        if (ImGui::Button("Pausar (||)")) emulationPlaying = false;
    } else {
        if (ImGui::Button("Executar (>)")) emulationPlaying = true;
    }
    
    ImGui::SameLine();
    if (ImGui::Button("Passo Unico (->)")) {
        emulationPlaying = false;
        if (activeConsole) activeConsole->step();
    }

    ImGui::SliderInt("Multiplicador de Velocidade", &executionSpeed, 1, 10);

    ImGui::Separator();
    
    // Output status information
    if (activeConsole) {
        std::string status = activeConsole->getStatusString();
        ImGui::TextWrapped("%s", status.c_str());
    }
    
    ImGui::End();

    // 5. Console Specific Debugger Panels (Registers, ROM header information)
    if (activeConsole) activeConsole->drawImGuiPanels();
}

void EmulatorApp::run() {
    while (appRunning) {
        handleEvents();
        updateEmulation();

        // New ImGui frame
        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        renderImGui();

        // Render ImGui data onto SDL screen
        SDL_SetRenderDrawColor(renderer, 30, 30, 35, 255);
        SDL_RenderClear(renderer);

        ImGui::Render();
        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);

        SDL_RenderPresent(renderer);
    }
}

void EmulatorApp::shutdown() {
    // Shutdown ImGui
    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    // Destroy SDL textures
    if (lcdTexture) {
        SDL_DestroyTexture(lcdTexture);
        lcdTexture = nullptr;
    }

    // Destroy SDL windows
    if (renderer) {
        SDL_DestroyRenderer(renderer);
        renderer = nullptr;
    }
    if (window) {
        SDL_DestroyWindow(window);
        window = nullptr;
    }

    // Close audio device
    if (audioDevice != 0) {
        SDL_CloseAudioDevice(audioDevice);
        audioDevice = 0;
    }
    
    SDL_Quit();
}
