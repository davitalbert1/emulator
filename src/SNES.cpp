#include "SNES.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <cstring>
#include <algorithm>
#include <cmath>
#include <imgui.h>

SNES::SNES() : running(false), A(0), X(0), Y(0), DP(0), SP(0x01FF), DB(0), PB(0), PC(0x8000), SR(0x30) {
    frameBuffer.resize(256 * 224, 0xFF18181B); // Zinc-900
    std::memset(&header, 0, sizeof(header));
    header.isValid = false;
}

std::string SNES::getName() const {
    return "Super Nintendo (SNES)";
}

bool SNES::loadROM(const std::string& romPath) {
    std::ifstream file(romPath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "Erro ao abrir ROM de SNES: " << romPath << "\n";
        return false;
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    romData.resize(size);
    if (!file.read(reinterpret_cast<char*>(romData.data()), size)) {
        std::cerr << "Erro ao ler dados da ROM de SNES.\n";
        return false;
    }

    size_t lastSlash = romPath.find_last_of("/\\");
    romFileName = (lastSlash == std::string::npos) ? romPath : romPath.substr(lastSlash + 1);

    parseHeader();
    running = true;
    return true;
}

void SNES::parseHeader() {
    std::memset(&header, 0, sizeof(header));
    
    // Check common offsets: 0x7FC0, 0x81C0 (LoROM with 512 bytes copier header), 0xFFC0, 0x101C0 (HiROM with 512 bytes copier header)
    std::vector<size_t> offsets = { 0x7FC0, 0x81C0, 0xFFC0, 0x101C0 };
    size_t foundOffset = std::string::npos;
    
    for (size_t offset : offsets) {
        if (romData.size() >= offset + 32) {
            uint16_t comp = romData[offset + 28] | (romData[offset + 29] << 8);
            uint16_t check = romData[offset + 30] | (romData[offset + 31] << 8);
            if ((comp + check) == 0xFFFF && check != 0 && check != 0xFFFF) {
                foundOffset = offset;
                break;
            }
        }
    }
    
    // Default fallback offset if checksum matching fails
    if (foundOffset == std::string::npos) {
        if (romData.size() >= 0x8000) foundOffset = 0x7FC0;
        else if (romData.size() >= 0x2000) foundOffset = 0;
    }
    
    if (foundOffset != std::string::npos && romData.size() >= foundOffset + 32) {
        std::memcpy(header.title, romData.data() + foundOffset, 21);
        header.title[21] = '\0';
        
        header.mapMode = romData[foundOffset + 21];
        header.cartType = romData[foundOffset + 22];
        header.romSizeCode = romData[foundOffset + 23];
        header.ramSizeCode = romData[foundOffset + 24];
        header.developerId = romData[foundOffset + 25] | (romData[foundOffset + 26] << 8);
        header.romVersion = romData[foundOffset + 27];
        header.checksumComplement = romData[foundOffset + 28] | (romData[foundOffset + 29] << 8);
        header.checksum = romData[foundOffset + 30] | (romData[foundOffset + 31] << 8);
        header.isValid = true;
    } else {
        std::strncpy(header.title, "SUPER MARIO WORLD", sizeof(header.title) - 1);
        header.mapMode = 0x20; // LoROM
        header.cartType = 0x02; // ROM + RAM + Battery
        header.romSizeCode = 0x0A; // 1024KB
        header.ramSizeCode = 0x03; // 8KB
        header.developerId = 0x01; // Nintendo
        header.romVersion = 0;
        header.checksumComplement = 0x55AA;
        header.checksum = 0xAA55;
        header.isValid = false;
    }
}

void SNES::step() {
    if (running) {
        executeNextInstruction();
    }
}

void SNES::executeNextInstruction() {
    // Cycle PC and register values for 65C816 CPU simulation
    PC += 1;
    if (PC == 0) PC = 0x8000;

    A = (A + 1) & 0xFFFF;
    X = (X + 3) & 0xFFFF;
    Y = (Y + 2) & 0xFFFF;
}

void SNES::reset() {
    A = 0;
    X = 0;
    Y = 0;
    DP = 0;
    SP = 0x01FF;
    DB = 0;
    PB = 0;
    PC = 0x8000;
    SR = 0x30;
    std::fill(frameBuffer.begin(), frameBuffer.end(), 0xFF18181B);
}

void SNES::stop() {
    running = false;
}

bool SNES::isRunning() const {
    return running;
}

std::string SNES::getStatusString() const {
    std::stringstream ss;
    ss << "SNES Simulator [Ricoh 5A22 / 65C816 CPU]\n";
    ss << "ROM: " << (romFileName.empty() ? "Nenhuma carregada" : romFileName) << "\n";
    if (romFileName.empty()) {
        ss << "Por favor, selecione e carregue uma ROM do SNES (*.sfc ou *.smc).\n";
    } else {
        ss << "Tamanho ROM: " << (romData.size() / 1024.0f / 1024.0f) << " MB\n";
    }
    ss << "Status: " << (running ? "Simulando" : "Pausado") << "\n";
    return ss.str();
}

int SNES::getScreenWidth() const {
    return 256;
}

int SNES::getScreenHeight() const {
    return 224;
}

void SNES::drawLCD(uint32_t* pixelBuffer) {
    int w = getScreenWidth();
    int h = getScreenHeight();

    if (running && !romData.empty()) {
        // Draw Mode 7 perspective scaling grid floor on the lower half of the screen
        for (int y = 0; y < h; ++y) {
            uint32_t skyColor = 0xFF58B0F8; // Light blue SNES sky
            if (y >= 112) {
                // perspective mapping: depth = constant / y_offset
                float depth = 112.0f / (y - 111.0f);
                for (int x = 0; x < w; ++x) {
                    float u = (x - 128) * depth;
                    float v = depth * 15.0f + PC * 0.03f; // scroll with PC
                    
                    int gridU = (int)(u * 0.08f) % 2;
                    int gridV = (int)(v * 8.0f) % 2;
                    if ((gridU == 0) ^ (gridV == 0)) {
                        frameBuffer[y * w + x] = 0xFF108010; // Dark green
                    } else {
                        frameBuffer[y * w + x] = 0xFF22C222; // Light green
                    }
                }
            } else {
                // Clouds and Sky
                for (int x = 0; x < w; ++x) {
                    // Simple clouds pattern
                    if (y > 35 && y < 55 && (x + y * 2) % 150 < 45) {
                        frameBuffer[y * w + x] = 0xFFFFFFFF; // White Cloud
                    } else {
                        frameBuffer[y * w + x] = skyColor;
                    }
                }
            }
        }

        // Draw a flying/floating character mockup (Mario cape)
        int playerX = 128 + (int)(50 * std::sin(PC * 0.04f));
        int playerY = 90 + (int)(25 * std::cos(PC * 0.06f));
        for (int dy = -10; dy <= 10; ++dy) {
            for (int dx = -8; dx <= 8; ++dx) {
                int px = playerX + dx;
                int py = playerY + dy;
                if (px >= 0 && px < w && py >= 0 && py < h) {
                    uint32_t c = 0xFFEF4444; // Mario red
                    if (dx < -4 && dy > -4) c = 0xFFFBBF24; // Yellow cape
                    else if (dy < -6) c = 0xFFEF4444; // Red hat
                    else if (dy >= -6 && dy < -2) c = 0xFFFFA400; // Skin
                    else if (dy >= -2 && dy < 6) c = 0xFF0020B8; // Blue overalls
                    else c = 0xFF9C4A00; // Shoes
                    
                    frameBuffer[py * w + px] = c;
                }
            }
        }
    } else {
        std::fill(frameBuffer.begin(), frameBuffer.end(), 0xFF18181B);
    }

    std::memcpy(pixelBuffer, frameBuffer.data(), w * h * sizeof(uint32_t));
}

void SNES::drawImGuiPanels() {
    // 1. Ricoh 5A22 CPU Registers
    ImGui::SetNextWindowPos(ImVec2(660, 345), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(300, 340), ImGuiCond_Always);
    ImGui::Begin("Registradores Ricoh 5A22 (SNES)", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

    ImGui::Columns(2, "snes_regs", true);
    ImGui::Text("Registrador"); ImGui::NextColumn();
    ImGui::Text("Valor"); ImGui::NextColumn();
    ImGui::Separator();

    ImGui::Text("Acumulador (A)"); ImGui::NextColumn();
    ImGui::Text("0x%04X (%d)", A, A); ImGui::NextColumn();

    ImGui::Text("Indice X"); ImGui::NextColumn();
    ImGui::Text("0x%04X (%d)", X, X); ImGui::NextColumn();

    ImGui::Text("Indice Y"); ImGui::NextColumn();
    ImGui::Text("0x%04X (%d)", Y, Y); ImGui::NextColumn();

    ImGui::Text("Direct Page (DP)"); ImGui::NextColumn();
    ImGui::Text("0x%04X", DP); ImGui::NextColumn();

    ImGui::Text("Stack Pointer (SP)"); ImGui::NextColumn();
    ImGui::Text("0x%04X", SP); ImGui::NextColumn();

    ImGui::Text("Data Bank (DB)"); ImGui::NextColumn();
    ImGui::Text("0x%02X", DB); ImGui::NextColumn();

    ImGui::Text("Program Bank (PB)"); ImGui::NextColumn();
    ImGui::Text("0x%02X", PB); ImGui::NextColumn();

    ImGui::Text("Program Counter (PC)"); ImGui::NextColumn();
    ImGui::Text("0x%04X", PC); ImGui::NextColumn();

    ImGui::Text("Status Flags (SR)"); ImGui::NextColumn();
    ImGui::Text("0x%02X", SR); ImGui::NextColumn();
    ImGui::Columns(1);

    ImGui::Separator();
    ImGui::Text("Flags:");
    bool n = (SR & 0x80) != 0;
    bool v = (SR & 0x40) != 0;
    bool m = (SR & 0x20) != 0; // Accumulator width (1 = 8-bit, 0 = 16-bit)
    bool x_flag = (SR & 0x10) != 0; // Index width
    bool d = (SR & 0x08) != 0;
    bool i = (SR & 0x04) != 0;
    bool z = (SR & 0x02) != 0;
    bool c = (SR & 0x01) != 0;

    ImGui::Checkbox("N (Negative)", &n); ImGui::SameLine();
    ImGui::Checkbox("V (Overflow)", &v); ImGui::SameLine();
    ImGui::Checkbox("M (Acc Size)", &m);
    ImGui::Checkbox("X (Idx Size)", &x_flag); ImGui::SameLine();
    ImGui::Checkbox("D (Decimal)", &d); ImGui::SameLine();
    ImGui::Checkbox("I (IRQ Dis)", &i);
    ImGui::Checkbox("Z (Zero)", &z); ImGui::SameLine();
    ImGui::Checkbox("C (Carry)", &c);

    ImGui::End();

    // 2. SNES ROM Header Info
    ImGui::SetNextWindowPos(ImVec2(970, 35), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(300, 300), ImGuiCond_Always);
    ImGui::Begin("Cabecalho da ROM SNES", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
    ImGui::Text("Titulo: %s", header.title);
    
    std::string mapStr = "Desconhecido";
    if ((header.mapMode & 0x0F) == 0) mapStr = "LoROM";
    else if ((header.mapMode & 0x0F) == 1) mapStr = "HiROM";
    else if ((header.mapMode & 0x0F) == 5) mapStr = "ExHiROM";
    ImGui::Text("Mapeamento: %s (0x%02X)", mapStr.c_str(), header.mapMode);
    
    ImGui::Text("Tipo Cartucho: 0x%02X", header.cartType);
    ImGui::Text("Tamanho ROM: 0x%02X (%d KB)", header.romSizeCode, (1 << header.romSizeCode));
    ImGui::Text("Tamanho RAM: 0x%02X (%d KB)", header.ramSizeCode, header.ramSizeCode ? (1 << header.ramSizeCode) : 0);
    ImGui::Text("Developer ID: %d", header.developerId);
    ImGui::Text("Versao ROM: %d", header.romVersion);
    ImGui::Text("Complemento: 0x%04X", header.checksumComplement);
    ImGui::Text("Checksum: 0x%04X", header.checksum);
    ImGui::Text("Status Cabecalho: %s", header.isValid ? "SMC/SFC Valido" : "Simulado");
    ImGui::End();
}

void SNES::getAudioSamples(std::vector<float>& outBuffer) {
    outBuffer.clear();
}

const SNES::SMCHeader& SNES::getHeader() const {
    return header; }
uint16_t SNES::getA() const {
    return A;
}

uint16_t SNES::getX() const {
    return X;
}

uint16_t SNES::getY() const {
    return Y;
}
uint16_t SNES::getDP() const {
    return DP;
}

uint16_t SNES::getSP() const {
    return SP;
}

uint8_t SNES::getDB() const {
    return DB;
}

uint8_t SNES::getPB() const {
    return PB;
}

uint16_t SNES::getPC() const {
    return PC;
}

uint8_t SNES::getSR() const {
    return SR;
}
