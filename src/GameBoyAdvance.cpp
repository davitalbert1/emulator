#include "GameBoyAdvance.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <algorithm>
#include <iostream>
#include <imgui.h>
#include <cmath>

GameBoyAdvance::GameBoyAdvance() : romFileName("Nenhum"), running(false), cpsr(0) {
    header.isValid = false;
    std::memset(&header, 0, sizeof(header));
    reset();
}

std::string GameBoyAdvance::getName() const {
    return "Game Boy Advance";
}

bool GameBoyAdvance::loadROM(const std::string& romPath) {
    std::ifstream file(romPath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return false;

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    romData.resize(size);
    if (!file.read(reinterpret_cast<char*>(romData.data()), size)) {
        romData.clear();
        return false;
    }

    // Extract filename from path
    size_t lastSlash = romPath.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
        romFileName = romPath.substr(lastSlash + 1);
    } else {
        romFileName = romPath;
    }

    parseHeader();
    reset();
    return true;
}

void GameBoyAdvance::parseHeader() {
    if (romData.size() < 0xC0) {
        header.isValid = false;
        std::strncpy(header.gameTitle, "SIMULATION", sizeof(header.gameTitle) - 1);
        std::strncpy(header.gameCode, "SIMU", sizeof(header.gameCode) - 1);
        std::strncpy(header.makerCode, "00", sizeof(header.makerCode) - 1);
        header.fixedValue = 0;
        header.softwareVersion = 0;
        header.checksum = 0;
        return;
    }

    // GBA header format:
    // 0x0A0 - 0x0AB: Game Title (12 bytes)
    // 0x0AC - 0x0AF: Game Code (4 bytes)
    // 0x0B0 - 0x0B1: Maker Code (2 bytes)
    // 0x0B2: Fixed value (should be 0x96)
    // 0x0BC: Software version (1 byte)
    // 0x0BD: Complement checksum (1 byte)

    // Game Title
    for (int i = 0; i < 12; ++i) {
        char c = static_cast<char>(romData[0xA0 + i]);
        header.gameTitle[i] = (c >= 32 && c <= 126) ? c : ' ';
    }
    header.gameTitle[12] = '\0';

    // Game Code
    for (int i = 0; i < 4; ++i) {
        char c = static_cast<char>(romData[0xAC + i]);
        header.gameCode[i] = (c >= 32 && c <= 126) ? c : '?';
    }
    header.gameCode[4] = '\0';

    // Maker Code
    for (int i = 0; i < 2; ++i) {
        char c = static_cast<char>(romData[0xB0 + i]);
        header.makerCode[i] = (c >= 32 && c <= 126) ? c : '?';
    }
    header.makerCode[2] = '\0';

    header.fixedValue = romData[0xB2];
    header.mainUnitCode = romData[0xB3];
    header.deviceType = romData[0xB4];
    header.softwareVersion = romData[0xBC];
    header.checksum = romData[0xBD];

    // Validate using the GBA fixed value 0x96
    header.isValid = (header.fixedValue == 0x96);
    
    // Fallback description for simulated / non-GBA binary loads
    if (!header.isValid) {
        std::strncpy(header.gameTitle, "DUMMY ROM", sizeof(header.gameTitle) - 1);
        std::strncpy(header.gameCode, "DMY1", sizeof(header.gameCode) - 1);
        std::strncpy(header.makerCode, "99", sizeof(header.makerCode) - 1);
    }
}

void GameBoyAdvance::reset() {
    // Zero all general purpose registers (R0-R12)
    for (int i = 0; i <= 12; ++i) registers[i] = 0;

    // Initialize stack pointers and control registers
    registers[13] = 0x03007F00; // R13 is SP: typical System/User stack pointer is placed at end of WRAM (0x03007F00)
    registers[14] = 0; // R14 is LR (Link Register)
    registers[15] = 0x08000000; // R15 is PC (Program Counter): GamePak ROM space starts at 0x08000000
    cpsr = 0x0000001F; // CPSR: System Mode (0x1F), ARM state (T bit = 0)

    // Initialize memory maps with default sizes
    wramBoard.assign(256 * 1024, 0); // 256 KB
    wramChip.assign(32 * 1024, 0); // 32 KB
    vram.assign(96 * 1024, 0); // 96 KB

    running = false;
}

void GameBoyAdvance::step() {
    if (romData.empty()) return;
    
    running = true;
    executeNextInstruction();
}

void GameBoyAdvance::executeNextInstruction() {
    uint32_t pcVal = registers[15];
    uint32_t romOffset = 0;

    // Check if PC is in ROM memory range (0x08000000 - 0x09FFFFFF)
    if (pcVal >= 0x08000000 && pcVal < 0x0A000000) {
        romOffset = pcVal - 0x08000000;
    } else {
        // For simulation, wrap PC back to ROM entry if it escapes
        registers[15] = 0x08000000;
        romOffset = 0;
    }

    // Check if we can fetch instruction from the loaded ROM
    if (romOffset + 3 < romData.size()) {
        // Fetch 32-bit ARM instruction (Little Endian)
        uint32_t instruction = romData[romOffset] | (romData[romOffset + 1] << 8) |
                              (romData[romOffset + 2] << 16) | (romData[romOffset + 3] << 24);

        // Simulated Decoding & Execution Flow
        // Increment program counter (ARM instructions are 4 bytes)
        registers[15] += 4;

        // Perform some mock arithmetic just to show registers changing during emulation
        registers[0] += 1;                  // Simple counter R0
        registers[1] = instruction & 0xFFFF; // R1 captures low bytes of instruction
        registers[2] = (registers[2] + 5) % 256;
        registers[3] = registers[0] * 2;
        
        // Update CPSR flag simulation (toggle zero/carry flags based on register states)
        cpsr &= ~0xC0000000; // Clear Zero and Carry flags
        if (registers[0] % 2 == 0) cpsr |= 0x40000000; // Set Zero Flag (Z)
        if (registers[2] > 128) cpsr |= 0x20000000; // Set Carry Flag (C)
    } else {
        // Fallback for simulation runs if PC goes out of ROM bounds or ROM is too small
        registers[15] += 4;
        registers[0]++;
        registers[1] = 0xE1A00000; // Mock NOP opcode (mov r0, r0)
        registers[2] = (registers[2] + 1) * 3;
    }
}

void GameBoyAdvance::stop() {
    running = false;
}

bool GameBoyAdvance::isRunning() const {
    return running;
}

const GameBoyAdvance::ROMHeader& GameBoyAdvance::getHeader() const {
    return header;
}

uint32_t GameBoyAdvance::getRegister(int index) const {
    if (index >= 0 && index < 16) return registers[index];
    return 0;
}

uint32_t GameBoyAdvance::getCPSR() const {
    return cpsr;
}

size_t GameBoyAdvance::getROMSize() const {
    return romData.size();
}

uint32_t GameBoyAdvance::getPC() const {
    return registers[15];
}

std::string GameBoyAdvance::getStatusString() const {
    std::stringstream ss;
    ss << "=== STATUS DO GAME BOY ADVANCE ===\n";
    ss << "Arquivo ROM: " << romFileName << "\n";
    ss << "Tamanho ROM: " << (romData.size() / 1024.0) << " KB\n";
    
    ss << "GBA Header Valido: " << (header.isValid ? "SIM (GBA ROM Real)" : "NAO (Simulacao/Binario)") << "\n";
    ss << "Titulo do Jogo:    [" << header.gameTitle << "]\n";
    ss << "Codigo do Jogo:    [" << header.gameCode << "]\n";
    ss << "Codigo Fabricante: [" << header.makerCode << "]\n";
    ss << "Versao Software:   " << static_cast<int>(header.softwareVersion) << "\n\n";

    ss << "--- Registradores ARM7TDMI ---\n";
    for (int i = 0; i < 16; ++i) {
        std::string regName = "R" + std::to_string(i);
        if (i == 13) regName = "SP (R13)";
        else if (i == 14) regName = "LR (R14)";
        else if (i == 15) regName = "PC (R15)";

        ss << std::left << std::setw(9) << regName << ": 0x"  << std::hex << std::setw(8) << std::setfill('0') << registers[i] << "   ";
        if ((i + 1) % 4 == 0) ss << "\n";
    }

    ss << "\nCPSR Flags: 0x" << std::hex << std::setw(8) << std::setfill('0') << cpsr << " [";
    ss << ((cpsr & 0x80000000) ? "N" : "-"); // Negative
    ss << ((cpsr & 0x40000000) ? "Z" : "-"); // Zero
    ss << ((cpsr & 0x20000000) ? "C" : "-"); // Carry
    ss << ((cpsr & 0x10000000) ? "V" : "-"); // Overflow
    ss << " Mode: ";
    
    uint32_t mode = cpsr & 0x1F;
    if (mode == 0x10) ss << "User";
    else if (mode == 0x11) ss << "FIQ";
    else if (mode == 0x12) ss << "IRQ";
    else if (mode == 0x13) ss << "Supervisor";
    else if (mode == 0x17) ss << "Abort";
    else if (mode == 0x1B) ss << "Undefined";
    else if (mode == 0x1F) ss << "System";
    else ss << "Unknown";
    ss << "]\n";

    ss << "Status: " << (running ? "EXECUTANDO (Running)" : "PAUSADO (Paused)") << "\n";

    return ss.str();
}

int GameBoyAdvance::getScreenWidth() const {
    return 240;
}

int GameBoyAdvance::getScreenHeight() const {
    return 160;
}

void GameBoyAdvance::drawLCD(uint32_t* pixelBuffer) {
    int w = getScreenWidth();
    int h = getScreenHeight();
    
    // Fill screen with a clean dark-grey background (no neon grid or bouncing box)
    for (int i = 0; i < w * h; ++i) pixelBuffer[i] = 0xFF18181B; // Zinc-900

    if (!romData.empty()) {
        // Draw a discrete green running indicator box (10x10) in the top-left corner
        for (int y = 5; y < 15; ++y) {
            for (int x = 5; x < 15; ++x) pixelBuffer[y * w + x] = 0xFF00FF00;
        }
    }
}

void GameBoyAdvance::drawImGuiPanels() {
    ImGui::SetNextWindowPos(ImVec2(660, 345), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(300, 340), ImGuiCond_Always);
    ImGui::Begin("Registradores ARM7TDMI (GBA 32-bit)", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

    // Grid of registers
    ImGui::Columns(4, "gba_registers_columns", true);
    for (int i = 0; i < 16; ++i) {
        std::string regLabel = "R" + std::to_string(i);
        if (i == 13) regLabel = "SP (R13)";
        else if (i == 14) regLabel = "LR (R14)";
        else if (i == 15) regLabel = "PC (R15)";

        ImGui::Text("%s", regLabel.c_str()); ImGui::NextColumn();
        ImGui::Text("0x%08X", registers[i]); ImGui::NextColumn();
    }
    
    ImGui::Columns(1);
    ImGui::Separator();
    
    ImGui::Text("CPSR: 0x%08X", cpsr);
    
    // Condition flags
    bool n = (cpsr & 0x80000000) != 0;
    bool z = (cpsr & 0x40000000) != 0;
    bool c = (cpsr & 0x20000000) != 0;
    bool v = (cpsr & 0x10000000) != 0;
    
    ImGui::Checkbox("Negative (N)", &n); ImGui::SameLine();
    ImGui::Checkbox("Zero (Z)", &z); ImGui::SameLine();
    ImGui::Checkbox("Carry (C)", &c); ImGui::SameLine();
    ImGui::Checkbox("Overflow (V)", &v);

    ImGui::End();

    ImGui::SetNextWindowPos(ImVec2(970, 35), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(300, 300), ImGuiCond_Always);
    ImGui::Begin("Cabecalho da ROM GBA", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
    ImGui::Text("Titulo: %s", header.gameTitle);
    ImGui::Text("Codigo: %s", header.gameCode);
    ImGui::Text("Fabricante: %s", header.makerCode);
    ImGui::Text("GBA Fixo (0x96): 0x%02X (%s)", header.fixedValue, (header.isValid ? "Valido" : "Invalido"));
    ImGui::Text("Versao Software: %d", header.softwareVersion);
    ImGui::Text("Checksum: 0x%02X", header.checksum);
    ImGui::End();
}

void GameBoyAdvance::getAudioSamples(std::vector<float>& outBuffer) {
    outBuffer.clear();
}
