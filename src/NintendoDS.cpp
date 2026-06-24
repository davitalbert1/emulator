#include "NintendoDS.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <cstring>
#include <algorithm>
#include <cmath>
#include <imgui.h>

NintendoDS::NintendoDS() : running(false), arm9Cpsr(0x0000001F), arm9Spsr(0), arm7Cpsr(0x0000001F), arm7Spsr(0) {
    std::memset(arm9Registers, 0, sizeof(arm9Registers));
    std::memset(arm7Registers, 0, sizeof(arm7Registers));
    
    // Set typical initial program counter and stack pointer for emulation visualization
    arm9Registers[13] = 0x03002F00; // SP
    arm9Registers[15] = 0x02000800; // PC
    arm7Registers[13] = 0x03803F00; // SP
    arm7Registers[15] = 0x037F8000; // PC

    topFrameBuffer.resize(256 * 192, 0xFF18181B); // Zinc-900
    bottomFrameBuffer.resize(256 * 192, 0xFF18181B); // Zinc-900

    std::memset(&header, 0, sizeof(header));
    header.isValid = false;
}

std::string NintendoDS::getName() const {
    return "Nintendo DS";
}

bool NintendoDS::loadROM(const std::string& romPath) {
    std::ifstream file(romPath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "Erro ao abrir ROM de DS: " << romPath << "\n";
        return false;
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    romData.resize(size);
    if (!file.read(reinterpret_cast<char*>(romData.data()), size)) {
        std::cerr << "Erro ao ler dados da ROM de DS.\n";
        return false;
    }

    size_t lastSlash = romPath.find_last_of("/\\");
    romFileName = (lastSlash == std::string::npos) ? romPath : romPath.substr(lastSlash + 1);

    parseHeader();
    running = true;
    return true;
}

void NintendoDS::parseHeader() {
    std::memset(&header, 0, sizeof(header));
    
    // NDS Header is at the beginning of the ROM
    if (romData.size() >= 512) {
        // Game Title: 12 bytes at 0x00
        std::memcpy(header.gameTitle, romData.data() + 0x00, 12);
        header.gameTitle[12] = '\0';

        // Game Code: 4 bytes at 0x0C
        std::memcpy(header.gameCode, romData.data() + 0x0C, 4);
        header.gameCode[4] = '\0';

        // Maker Code: 2 bytes at 0x10
        std::memcpy(header.makerCode, romData.data() + 0x10, 2);
        header.makerCode[2] = '\0';

        // Unit Code: 1 byte at 0x12
        header.unitCode = romData[0x12];

        // Device Type: 1 byte at 0x13
        header.deviceType = romData[0x13];

        // Device Size: 1 byte at 0x14
        header.deviceSize = romData[0x14];

        // ROM Version: 1 byte at 0x1E
        header.romVersion = romData[0x1E];

        // Checksum: 2 bytes at 0x15C
        header.headerChecksum = romData[0x15C] | (romData[0x15D] << 8);
        header.isValid = true;
    } else {
        // Fallback simulation headers
        std::strncpy(header.gameTitle, "NDS SIMULATOR", sizeof(header.gameTitle) - 1);
        std::strncpy(header.gameCode, "NDSS", sizeof(header.gameCode) - 1);
        std::strncpy(header.makerCode, "01", sizeof(header.makerCode) - 1);
        header.unitCode = 0;
        header.deviceType = 0;
        header.deviceSize = 7;
        header.romVersion = 0;
        header.headerChecksum = 0xAA55;
        header.isValid = false;
    }
}

void NintendoDS::step() {
    if (running) {
        executeNextInstruction();
    }
}

void NintendoDS::executeNextInstruction() {
    // Simulate program counter increment and dummy register cycling for ARM9/ARM7
    arm9Registers[15] += 4;
    arm7Registers[15] += 4;

    // Cycle registers R0-R3 slightly to show active simulation
    arm9Registers[0] = (arm9Registers[0] + 1) & 0x000FFFFF;
    arm9Registers[1] = (arm9Registers[1] + 3) & 0x00FFFFFF;
    arm7Registers[0] = (arm7Registers[0] + 2) & 0x0000FFFF;
    arm7Registers[1] = (arm7Registers[1] + 5) & 0x000FFFFF;
}

void NintendoDS::reset() {
    arm9Registers[13] = 0x03002F00;
    arm9Registers[15] = 0x02000800;
    arm7Registers[13] = 0x03803F00;
    arm7Registers[15] = 0x037F8000;
    for (int i = 0; i < 13; ++i) {
        arm9Registers[i] = 0;
        arm7Registers[i] = 0;
    }
    
    // Clear frames
    std::fill(topFrameBuffer.begin(), topFrameBuffer.end(), 0xFF18181B);
    std::fill(bottomFrameBuffer.begin(), bottomFrameBuffer.end(), 0xFF18181B);
}

void NintendoDS::stop() {
    running = false;
}

bool NintendoDS::isRunning() const {
    return running;
}

std::string NintendoDS::getStatusString() const {
    std::stringstream ss;
    ss << "Nintendo DS Simulator [ARM9 + ARM7]\n";
    ss << "ROM: " << (romFileName.empty() ? "Nenhuma carregada" : romFileName) << "\n";
    if (romFileName.empty()) {
        ss << "Por favor, selecione e carregue uma ROM do Nintendo DS (*.nds).\n";
    } else {
        ss << "Tamanho ROM: " << (romData.size() / 1024.0f / 1024.0f) << " MB\n";
    }
    ss << "Status: " << (running ? "Simulando" : "Pausado") << "\n";
    return ss.str();
}

int NintendoDS::getScreenWidth() const {
    return 256;
}

int NintendoDS::getScreenHeight() const {
    // 192 (top screen) + 8 (separator gap) + 192 (bottom screen)
    return 392;
}

void NintendoDS::drawLCD(uint32_t* pixelBuffer) {
    int w = getScreenWidth();
    
    // Refresh framebuffers with basic styling
    std::fill(topFrameBuffer.begin(), topFrameBuffer.end(), 0xFF18181B);
    std::fill(bottomFrameBuffer.begin(), bottomFrameBuffer.end(), 0xFF18181B);

    if (running && !romData.empty()) {
        // Draw decorative patterns to represent active dual screens
        // Top screen: Stylized Nintendo DS Logo screen (a subtle grid and text placeholder)
        for (int y = 20; y < 172; ++y) {
            for (int x = 20; x < 236; ++x) {
                if ((x + y) % 32 < 2) {
                    topFrameBuffer[y * 256 + x] = 0xFF27272A; // Grid pattern
                }
            }
        }
        
        // Draw a red "DS" mock symbol on the top screen
        for (int y = 80; y < 112; ++y) {
            for (int x = 110; x < 146; ++x) {
                if (x < 122 || x > 134 || y < 86 || y > 106) {
                    topFrameBuffer[y * 256 + x] = 0xFFEF4444; // Red color
                }
            }
        }

        // Bottom screen (Touch screen): Grid pattern + tactile button mocks
        for (int y = 10; y < 182; ++y) {
            for (int x = 10; x < 246; ++x) {
                // Drawing touch pads in the corners
                if ((x < 50 && y < 50) || (x > 206 && y > 142)) {
                    bottomFrameBuffer[y * 256 + x] = 0xFF3F3F46; // Dark touch buttons
                } else if ((x + y) % 16 == 0) {
                    bottomFrameBuffer[y * 256 + x] = 0xFF1E1B4B; // Very subtle grid
                }
            }
        }
        
        // Draw a simulated yellow stylus touch point
        int touchX = 128 + (int)(10 * std::sin(arm9Registers[15] * 0.05f));
        int touchY = 96 + (int)(10 * std::cos(arm9Registers[15] * 0.05f));
        if (touchX >= 0 && touchX < 256 && touchY >= 0 && touchY < 192) {
            for (int dy = -3; dy <= 3; ++dy) {
                for (int dx = -3; dx <= 3; ++dx) {
                    int px = touchX + dx;
                    int py = touchY + dy;
                    if (px >= 0 && px < 256 && py >= 0 && py < 192) {
                        bottomFrameBuffer[py * 256 + px] = 0xFFFBBF24; // Yellow touch point
                    }
                }
            }
        }
    }

    // Compose onto the final display texture (256x392)
    // 1. Copy Top screen
    for (int y = 0; y < 192; ++y) {
        std::memcpy(pixelBuffer + (y * w), topFrameBuffer.data() + (y * 256), 256 * sizeof(uint32_t));
    }
    
    // 2. Draw black separator gap (lines 192 to 199)
    for (int y = 192; y < 200; ++y) {
        for (int x = 0; x < w; ++x) {
            pixelBuffer[y * w + x] = 0xFF09090B; // Deep zinc black gap
        }
    }

    // 3. Copy Bottom screen
    for (int y = 0; y < 192; ++y) {
        std::memcpy(pixelBuffer + ((y + 200) * w), bottomFrameBuffer.data() + (y * 256), 256 * sizeof(uint32_t));
    }
}

void NintendoDS::drawImGuiPanels() {
    // 1. CPU Dual Register Panel (tabbed)
    ImGui::SetNextWindowPos(ImVec2(660, 345), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(300, 340), ImGuiCond_Always);
    ImGui::Begin("Registradores ARM9 / ARM7 (NDS)", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
    
    if (ImGui::BeginTabBar("NDSCPUsTabBar")) {
        if (ImGui::BeginTabItem("ARM9 (Main)")) {
            ImGui::Columns(4, "nds_arm9_reg_columns", true);
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
            ImGui::Text("SPSR: 0x%08X", arm9Spsr);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("ARM7 (Sub)")) {
            ImGui::Columns(4, "nds_arm7_reg_columns", true);
            for (int i = 0; i < 16; ++i) {
                std::string regLabel = "R" + std::to_string(i);
                if (i == 13) regLabel = "SP (R13)";
                else if (i == 14) regLabel = "LR (R14)";
                else if (i == 15) regLabel = "PC (R15)";

                ImGui::Text("%s", regLabel.c_str()); ImGui::NextColumn();
                ImGui::Text("0x%08X", arm7Registers[i]); ImGui::NextColumn();
            }
            ImGui::Columns(1);
            ImGui::Separator();
            ImGui::Text("CPSR: 0x%08X", arm7Cpsr);
            ImGui::Text("SPSR: 0x%08X", arm7Spsr);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();

    // 2. ROM Header Panel
    ImGui::SetNextWindowPos(ImVec2(970, 35), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(300, 300), ImGuiCond_Always);
    ImGui::Begin("Cabecalho da ROM NDS", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
    ImGui::Text("Titulo: %s", header.gameTitle);
    ImGui::Text("Codigo: %s", header.gameCode);
    ImGui::Text("Fabricante: %s", header.makerCode);
    ImGui::Text("Arquitetura: %s", (header.unitCode == 0 ? "DS (ARM9+ARM7)" : (header.unitCode == 2 ? "DSi Exclusivo" : "DS/DSi Hibrido")));
    ImGui::Text("Tipo Cartao: 0x%02X", header.deviceType);
    ImGui::Text("Tamanho: 0x%02X (%d MB)", header.deviceSize, (1 << (header.deviceSize - 17)));
    ImGui::Text("Versao ROM: %d", header.romVersion);
    ImGui::Text("Checksum: 0x%04X (%s)", header.headerChecksum, (header.isValid ? "Valido" : "Simulado"));
    ImGui::End();
}

void NintendoDS::getAudioSamples(std::vector<float>& outBuffer) {
    outBuffer.clear();
}

const NintendoDS::NDSHeader& NintendoDS::getHeader() const {
    return header;
}

uint32_t NintendoDS::getARM9Register(int index) const {
    if (index >= 0 && index < 16) return arm9Registers[index];
    return 0;
}

uint32_t NintendoDS::getARM9CPSR() const {
    return arm9Cpsr;
}

uint32_t NintendoDS::getARM7Register(int index) const {
    if (index >= 0 && index < 16) return arm7Registers[index];
    return 0;
}

uint32_t NintendoDS::getARM7CPSR() const {
    return arm7Cpsr;
}
