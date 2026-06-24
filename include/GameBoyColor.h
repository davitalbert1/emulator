#ifndef GAMEBOYCOLOR_H
#define GAMEBOYCOLOR_H

#include "GameBoy.h"

class GameBoyColor : public GameBoy {
public:
    GameBoyColor();
    ~GameBoyColor() override = default;

    std::string getName() const override;
    void reset() override;
    void step() override;
    
    void drawLCD(uint32_t* pixelBuffer) override;
    void drawImGuiPanels() override;

    bool isDoubleSpeed() const;

private:
    bool doubleSpeedMode;
    
    // GBC VRAM & WRAM expansions
    std::vector<uint8_t> wramExtra; // 32 KB expanded WRAM
    std::vector<uint8_t> vramExtra; // 16 KB expanded VRAM

    // GBC Color palettes (8 background palettes + 8 sprite palettes, each containing 4 colors of 16-bit RGB555)
    uint16_t bgPalettes[32]; 
    uint16_t spritePalettes[32];

    void parseHeader() override;
    void executeNextInstruction() override;
    
    // Helpers
    uint32_t convertRGB555ToARGB(uint16_t rgb555) const;
};

#endif // GAMEBOYCOLOR_H
