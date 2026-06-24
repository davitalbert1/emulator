#ifndef CONSOLE_H
#define CONSOLE_H

#include <string>
#include <cstdint>
#include <vector>

class Console {
public:
    virtual ~Console() = default;
    
    // Core Console interface
    virtual std::string getName() const = 0;
    virtual bool loadROM(const std::string& romPath) = 0;
    virtual void step() = 0;
    virtual void reset() = 0;
    virtual void stop() = 0;
    virtual bool isRunning() const = 0;
    
    // Status/UI helper
    virtual std::string getStatusString() const = 0;

    // Graphical interface additions
    virtual int getScreenWidth() const = 0;
    virtual int getScreenHeight() const = 0;
    
    // Fills a flat array of 32-bit ARGB pixels representing the console display
    virtual void drawLCD(uint32_t* pixelBuffer) = 0;
    
    // Renders custom ImGui windows (e.g. register grids, VRAM viewer)
    virtual void drawImGuiPanels() = 0;

    // Audio interface
    virtual void getAudioSamples(std::vector<float>& outBuffer) = 0;
};

#endif // CONSOLE_H
