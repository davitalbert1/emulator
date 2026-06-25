#include "Nintendo3DS.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <cstring>
#include <algorithm>
#include <cmath>
#include <imgui.h>

Nintendo3DS::Nintendo3DS() : running(false), arm11Cpsr(0x00000010), arm9Cpsr(0x00000013) {
    std::memset(arm11Registers, 0, sizeof(arm11Registers));
    std::memset(arm9Registers, 0, sizeof(arm9Registers));

    // Initial PC/SP for ARM11 and ARM9
    arm11Registers[13] = 0x0FFFC000; // SP
    arm11Registers[15] = 0x00100000; // PC
    arm9Registers[13] = 0x08003F00; // SP
    arm9Registers[15] = 0xFFFF0000; // PC

    topFrameBuffer.resize(400 * 240, 0xFF18181B); // Zinc-900
    bottomFrameBuffer.resize(320 * 240, 0xFF18181B); // Zinc-900

    std::memset(&header, 0, sizeof(header));
    header.isValid = false;
}

std::string Nintendo3DS::getName() const {
    return "Nintendo 3DS";
}

bool Nintendo3DS::loadROM(const std::string& romPath) {
    std::ifstream file(romPath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "Erro ao abrir ROM de 3DS: " << romPath << "\n";
        return false;
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    romData.resize(size);
    if (!file.read(reinterpret_cast<char*>(romData.data()), size)) {
        std::cerr << "Erro ao ler dados da ROM de 3DS.\n";
        return false;
    }

    size_t lastSlash = romPath.find_last_of("/\\");
    romFileName = (lastSlash == std::string::npos) ? romPath : romPath.substr(lastSlash + 1);

    parseHeader();
    running = true;
    return true;
}

void Nintendo3DS::parseHeader() {
    std::memset(&header, 0, sizeof(header));

    // Try to find the NCCH header (usually offset 0x100 or 0x1000 in .3ds, or 0x0 in .cia)
    size_t ncchOffset = std::string::npos;
    if (romData.size() >= 0x200) {
        // Search at 0x100 first
        if (std::memcmp(romData.data() + 0x100, "NCCH", 4) == 0) {
            ncchOffset = 0x100;
        } else if (std::memcmp(romData.data() + 0x00, "NCCH", 4) == 0) {
            ncchOffset = 0x00;
        }
    }

    if (ncchOffset != std::string::npos && romData.size() >= ncchOffset + 0x160) {
        // Program ID: 8 bytes at ncchOffset + 0x18
        std::memcpy(&header.programId, romData.data() + ncchOffset + 0x18, 8);

        // Product Code: 10 bytes at ncchOffset + 0x50
        std::memcpy(header.productCode, romData.data() + ncchOffset + 0x50, 10);
        header.productCode[10] = '\0';

        // Maker Code: 2 bytes at ncchOffset + 0x60
        std::memcpy(header.makerCode, romData.data() + ncchOffset + 0x60, 2);
        header.makerCode[2] = '\0';

        // Title name is not directly stored inside NCCH in plain ASCII (it's in the ExHeader),
        // so we derive a title name from the filename or product code
        size_t dotIndex = romFileName.find_last_of('.');
        std::string rawTitle = (dotIndex == std::string::npos) ? romFileName : romFileName.substr(0, dotIndex);
        if (rawTitle.length() > 16) rawTitle = rawTitle.substr(0, 16);
        std::strncpy(header.gameTitle, rawTitle.c_str(), 16);
        header.gameTitle[16] = '\0';

        header.sdkVersion = 0x02000000;
        header.systemMode = 0;
        header.isValid = true;
    } else {
        // Fallback simulation headers
        std::strncpy(header.gameTitle, "3DS GAME STUB", sizeof(header.gameTitle) - 1);
        std::strncpy(header.productCode, "CTR-P-CTAP", sizeof(header.productCode) - 1);
        std::strncpy(header.makerCode, "01", sizeof(header.makerCode) - 1);
        header.programId = 0x0004000000030800ULL;
        header.sdkVersion = 0x05040000;
        header.systemMode = 0;
        header.isValid = false;
    }
}

void Nintendo3DS::step() {
    if (running) executeNextInstruction();
}

void Nintendo3DS::executeNextInstruction() {
    // ARM11 instructions (32-bit cycles)
    arm11Registers[15] += 4;
    arm9Registers[15] += 2; // Thumb/ARM PC cycle

    // Animate registers to show activity
    arm11Registers[0] = (arm11Registers[0] + 4) & 0x0FFFFFFF;
    arm11Registers[1] = (arm11Registers[1] + 1) & 0x000FFFFF;
    arm9Registers[0] = (arm9Registers[0] + 1) & 0x0000FFFF;
}

void Nintendo3DS::reset() {
    arm11Registers[13] = 0x0FFFC000;
    arm11Registers[15] = 0x00100000;
    arm9Registers[13] = 0x08003F00;
    arm9Registers[15] = 0xFFFF0000;
    for (int i = 0; i < 13; ++i) {
        arm11Registers[i] = 0;
        arm9Registers[i] = 0;
    }
    
    // Clear screens
    std::fill(topFrameBuffer.begin(), topFrameBuffer.end(), 0xFF18181B);
    std::fill(bottomFrameBuffer.begin(), bottomFrameBuffer.end(), 0xFF18181B);
}

void Nintendo3DS::stop() {
    running = false;
}

bool Nintendo3DS::isRunning() const {
    return running;
}

std::string Nintendo3DS::getStatusString() const {
    std::stringstream ss;
    ss << "Nintendo 3DS Simulator [ARM11 Dual + ARM9]\n";
    ss << "ROM: " << (romFileName.empty() ? "Nenhuma carregada" : romFileName) << "\n";
    if (romFileName.empty()) {
        ss << "Por favor, selecione e carregue uma ROM do Nintendo 3DS (*.3ds ou *.cia).\n";
    } else {
        ss << "Tamanho ROM: " << (romData.size() / 1024.0f / 1024.0f) << " MB\n";
    }
    ss << "Status: " << (running ? "Simulando" : "Pausado") << "\n";
    return ss.str();
}

int Nintendo3DS::getScreenWidth() const {
    return 400; // Top screen width
}

int Nintendo3DS::getScreenHeight() const {
    // 240 (top screen) + 8 (separator gap) + 240 (bottom screen)
    return 488;
}

void Nintendo3DS::drawLCD(uint32_t* pixelBuffer) {
    int w = getScreenWidth();
    int h = getScreenHeight();

    // Clear entire composited buffer to dark padding color first
    for (int i = 0; i < w * h; ++i) pixelBuffer[i] = 0xFF09090B; // Zinc-950

    // Refresh sub-framebuffers
    std::fill(topFrameBuffer.begin(), topFrameBuffer.end(), 0xFF18181B);
    std::fill(bottomFrameBuffer.begin(), bottomFrameBuffer.end(), 0xFF18181B);

    if (running && !romData.empty()) {
        // Draw 3D screen decoration on Top screen
        // Drawing a red 3D wireframe effect (twoOffset lines representing stereo view)
        float animOffset = std::sin(arm11Registers[15] * 0.02f) * 15.0f;
        
        for (int y = 30; y < 210; ++y) {
            for (int x = 40; x < 360; ++x) {
                if ((x + y) % 64 < 2) topFrameBuffer[y * 400 + x] = 0xFF27272A; // Background grid
            }
        }
        
        // Draw stereo offset rings (cyan and red) to simulate 3D display
        int centerX = 200;
        int centerY = 120;
        int radius = 40;
        
        for (int y = 0; y < 240; ++y) {
            for (int x = 0; x < 400; ++x) {
                // Red ring (Left eye offset)
                int rx = x - (centerX - 5 + (int)(animOffset * 0.1f));
                int ry = y - centerY;
                if (std::abs(rx*rx + ry*ry - radius*radius) < 150) topFrameBuffer[y * 400 + x] = 0xFFEF4444;
                // Cyan ring (Right eye offset)
                int cx = x - (centerX + 5 - (int)(animOffset * 0.1f));
                int cy = y - centerY;
                if (std::abs(cx*cx + cy*cy - radius*radius) < 150) topFrameBuffer[y * 400 + x] = 0xFF06B6D4; // Mix cyan or additive coloring
            }
        }

        // Draw Home Menu screen mockup on bottom screen (320x240)
        for (int y = 10; y < 230; ++y) {
            for (int x = 10; x < 310; ++x) {
                // Drawing Home grid icons
                int cellX = (x - 20) / 60;
                int cellY = (y - 30) / 60;
                if (cellX >= 0 && cellX < 4 && cellY >= 0 && cellY < 3) {
                    int innerX = (x - 20) % 60;
                    int innerY = (y - 30) % 60;
                    if (innerX > 10 && innerX < 50 && innerY > 10 && innerY < 50) {
                        bottomFrameBuffer[y * 320 + x] = 0xFF4F46E5; // Indigo icon boxes
                    }
                }
            }
        }
    }

    // Composite:
    // 1. Copy top screen (400x240) into lines 0 to 239 of target buffer
    for (int y = 0; y < 240; ++y) {
        std::memcpy(pixelBuffer + (y * w), topFrameBuffer.data() + (y * 400), 400 * sizeof(uint32_t));
    }

    // 2. Draw black separator gap (lines 240 to 247)
    for (int y = 240; y < 248; ++y) {
        for (int x = 0; x < w; ++x) pixelBuffer[y * w + x] = 0xFF09090B;
    }

    // 3. Copy bottom screen (320x240) centered horizontally at X = 40 (lines 248 to 487)
    int targetXOffset = (400 - 320) / 2; // 40 pixels padding
    for (int y = 0; y < 240; ++y) {
        std::memcpy(pixelBuffer + ((y + 248) * w) + targetXOffset, bottomFrameBuffer.data() + (y * 320), 320 * sizeof(uint32_t));
    }
}

void Nintendo3DS::drawImGuiPanels() {
    // 1. CPU Dual Register Panel (tabbed)
    ImGui::SetNextWindowPos(ImVec2(660, 345), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(300, 340), ImGuiCond_Always);
    ImGui::Begin("Registradores ARM11 / ARM9 (3DS)", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
    
    if (ImGui::BeginTabBar("3DSCPUsTabBar")) {
        if (ImGui::BeginTabItem("ARM11 (Application)")) {
            ImGui::Columns(4, "ctr_arm11_reg_columns", true);
            for (int i = 0; i < 16; ++i) {
                std::string regLabel = "R" + std::to_string(i);
                if (i == 13) regLabel = "SP (R13)";
                else if (i == 14) regLabel = "LR (R14)";
                else if (i == 15) regLabel = "PC (R15)";

                ImGui::Text("%s", regLabel.c_str()); ImGui::NextColumn();
                ImGui::Text("0x%08X", arm11Registers[i]); ImGui::NextColumn();
            }
            ImGui::Columns(1);
            ImGui::Separator();
            ImGui::Text("CPSR: 0x%08X", arm11Cpsr);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("ARM9 (Security)")) {
            ImGui::Columns(4, "ctr_arm9_reg_columns", true);
            for (int i = 0; i < 16; ++i) {
                std::string regLabel = "R" + std::to_string(i);
                if (i == 13) regLabel = "SP (R13)";
                else if (i == 14) regLabel = "LR (R14)";
                else if (i == 15) regLabel = "PC (R15)";

                ImGui::Text("%s", regLabel.c_str()); ImGui::NextColumn();
                ImGui::Text("0x%08X", arm9Registers[i]); ImGui::NextColumn();
            }
            ImGui::Columns(1);
            ImGui::Separator();
            ImGui::Text("CPSR: 0x%08X", arm9Cpsr);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();

    // 2. ROM Header Panel
    ImGui::SetNextWindowPos(ImVec2(970, 35), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(300, 300), ImGuiCond_Always);
    ImGui::Begin("Cabecalho da ROM 3DS", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
    ImGui::Text("Titulo: %s", header.gameTitle);
    ImGui::Text("Codigo: %s", header.productCode);
    ImGui::Text("Fabricante: %s", header.makerCode);
    ImGui::Text("Program ID: 0x%016llX", header.programId);
    ImGui::Text("SDK Version: 0x%08X", header.sdkVersion);
    ImGui::Text("System Mode: %d", header.systemMode);
    ImGui::Text("Status Assinatura: %s", (header.isValid ? "NCCH Valido" : "Simulado"));
    ImGui::End();
}

void Nintendo3DS::getAudioSamples(std::vector<float>& outBuffer) {
    outBuffer.clear();
}

const Nintendo3DS::NCCHHeader& Nintendo3DS::getHeader() const {
    return header;
}

uint32_t Nintendo3DS::getARM11Register(int index) const {
    if (index >= 0 && index < 16) return arm11Registers[index];
    return 0;
}

uint32_t Nintendo3DS::getARM11CPSR() const {
    return arm11Cpsr;
}

uint32_t Nintendo3DS::getARM9Register(int index) const {
    if (index >= 0 && index < 16) return arm9Registers[index];
    return 0;
}

uint32_t Nintendo3DS::getARM9CPSR() const {
    return arm9Cpsr;
}
