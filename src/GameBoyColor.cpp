#include "GameBoyColor.h"
#include <imgui.h>
#include <cstring>
#include <cmath>

GameBoyColor::GameBoyColor() : GameBoy(), doubleSpeedMode(false) {
    reset();
}

std::string GameBoyColor::getName() const {
    return "Game Boy Color";
}

void GameBoyColor::reset() {
    GameBoy::reset();
    
    // GBC Hardware identifier boot value: A register must be 0x11
    A = 0x11;
    doubleSpeedMode = false;

    // Allocate extra GBC memory
    wramExtra.assign(32 * 1024, 0); // 32 KB WRAM
    vramExtra.assign(16 * 1024, 0); // 16 KB VRAM

    // Initialize background and sprite palettes with mock RGB555 values
    // RGB555 format: 0bbbbbgggggrrrrr
    // Palette 0: Red scale
    bgPalettes[0] = 0x001F; // Bright Red
    bgPalettes[1] = 0x000F; // Darker Red
    bgPalettes[2] = 0x0005; // Dark Red
    bgPalettes[3] = 0x0000; // Black
    
    // Palette 1: Green scale
    bgPalettes[4] = 0x03E0; // Bright Green
    bgPalettes[5] = 0x0200; // Medium Green
    bgPalettes[6] = 0x00C0; // Dark Green
    bgPalettes[7] = 0x0000; // Black

    // Palette 2: Blue scale
    bgPalettes[8] = 0x7C00;  // Bright Blue
    bgPalettes[9] = 0x4000;  // Medium Blue
    bgPalettes[10] = 0x1C00; // Dark Blue
    bgPalettes[11] = 0x7FFF; // White

    // Palette 3: Cyan/Yellow/Magenta mix
    bgPalettes[12] = 0x7DE0; // Cyan
    bgPalettes[13] = 0x03FF; // Yellow
    bgPalettes[14] = 0x7C1F; // Magenta
    bgPalettes[15] = 0x3DEF; // Grey

    // Fill remaining palettes with default gradients
    for (int i = 16; i < 32; ++i) {
        bgPalettes[i] = ((i % 4) == 0) ? 0x7FFF : (0x7FFF - (i * 800));
        spritePalettes[i - 16] = 0x7C00 + (i * 200);
        spritePalettes[i] = bgPalettes[i];
    }
}

void GameBoyColor::step() {
    if (romData.empty()) {
        pulseTime += 0.05f;
        // Bouncing ball physics
        ballX += ballDX;
        ballY += ballDY;
        if (ballX <= 5 || ballX >= 155) ballDX = -ballDX;
        if (ballY <= 5 || ballY >= 135) ballDY = -ballDY;
        return;
    }

    running = true;
    
    // Execute instruction
    executeNextInstruction();
    
    // If GBC Double Speed Mode is active, run an extra CPU cycle step!
    if (doubleSpeedMode) executeNextInstruction();

    pulseTime += 0.05f;
    ballX += ballDX;
    ballY += ballDY;
    if (ballX <= 5 || ballX >= 155) ballDX = -ballDX;
    if (ballY <= 5 || ballY >= 135) ballDY = -ballDY;
}

void GameBoyColor::executeNextInstruction() {
    GameBoy::executeNextInstruction();
    // Simulate updating double speed registers or speed switch
    // GBC speed register KEY1 is mapped to IO port 0xFF4D
}

void GameBoyColor::parseHeader() {
    GameBoy::parseHeader();
    // Check double validity of GBC support from flag
    // Byte 0x143 has GBC support flag:
    // Bit 7 set (0x80) means CGB compatibility; 0xC0 means CGB exclusive.
    if (romData.size() >= 0x150) {
        header.cgbFlag = romData[0x143];
        if (header.cgbFlag == 0x80 || header.cgbFlag == 0xC0) header.isValid = true;
    }
}

uint32_t GameBoyColor::convertRGB555ToARGB(uint16_t rgb555) const {
    // Extract 5-bit channels
    uint32_t r = (rgb555 & 0x001F);
    uint32_t g = ((rgb555 >> 5) & 0x001F);
    uint32_t b = ((rgb555 >> 10) & 0x001F);

    // Expand to 8-bit channels: (val * 255 / 31)
    r = (r * 255) / 31;
    g = (g * 255) / 31;
    b = (b * 255) / 31;

    // Pack into ARGB: Alpha (0xFF), R, G, B
    return (0xFF000000) | (r << 16) | (g << 8) | b;
}

void GameBoyColor::drawLCD(uint32_t* pixelBuffer) {
    int w = getScreenWidth();
    int h = getScreenHeight();
    if (romData.empty()) {
        // Flat dark background (zinc-900)
        for (int i = 0; i < w * h; ++i) pixelBuffer[i] = 0xFF18181B;
    } else {
        // Copy the emulated framebuffer (monochrome rendering mode)
        std::memcpy(pixelBuffer, frameBuffer.data(), w * h * sizeof(uint32_t));
    }
}

void GameBoyColor::drawImGuiPanels() {
    // Render standard Sharp LR35902 registers and GBA ROM stats
    GameBoy::drawImGuiPanels();

    // Render GBC specific controls and Palette inspector
    ImGui::Begin("Controles GBC");
    ImGui::Checkbox("Modo Double-Speed (Velocidade Dobrada)", &doubleSpeedMode);
    
    ImGui::Separator();
    ImGui::Text("Paletas de Cores CGB (Fundo - Background)");
    
    for (int pal = 0; pal < 8; ++pal) {
        ImGui::Text("Paleta %d:", pal);
        ImGui::SameLine();
        for (int colorIdx = 0; colorIdx < 4; ++colorIdx) {
            uint16_t rgb555 = bgPalettes[pal * 4 + colorIdx];
            uint32_t colorARGB = convertRGB555ToARGB(rgb555);
            
            // Convert ARGB to ImVec4 for ImGui ColorButton
            float r = ((colorARGB >> 16) & 0xFF) / 255.0f;
            float g = ((colorARGB >> 8) & 0xFF) / 255.0f;
            float b = (colorARGB & 0xFF) / 255.0f;
            
            char label[32];
            std::sprintf(label, "P%dC%d", pal, colorIdx);
            ImGui::ColorButton(label, ImVec4(r, g, b, 1.0f), ImGuiColorEditFlags_NoTooltip, ImVec2(20, 20));
            if (colorIdx < 3) ImGui::SameLine();
        }
    }
    
    ImGui::End();
}

bool GameBoyColor::isDoubleSpeed() const {
    return doubleSpeedMode;
}
