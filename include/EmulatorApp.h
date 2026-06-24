#ifndef EMULATORAPP_H
#define EMULATORAPP_H

#include "Console.h"
#include <memory>
#include <vector>
#include <string>

// Forward declarations of SDL structures to keep compilation fast and avoid header pollution
struct SDL_Window;
struct SDL_Renderer;
struct SDL_Texture;

class EmulatorApp {
public:
    EmulatorApp();
    ~EmulatorApp();

    bool init();
    void run();
    void shutdown();

private:
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Texture* lcdTexture;
    std::vector<uint32_t> pixelBuffer;
    uint32_t audioDevice;
    
    std::unique_ptr<Console> activeConsole;
    int activeConsoleIndex; // 0=GBA, 1=GB, 2=GBC, 3=NDS, 4=N2DS, 5=N3DS, 6=Atari, 7=NES, 8=SNES, 9=Genesis
    
    bool appRunning;
    bool emulationPlaying;
    bool showConsoleSelector;
    int executionSpeed; // Number of steps per frame
    
    char romPathInput[512];

    void handleEvents();
    void updateEmulation();
    void renderImGui();
    void updateLCDTexture();
    
    void loadConsole(int index);
    void loadROM(const std::string& path);
};

#endif // EMULATORAPP_H
