#include "NES.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <cstring>
#include <algorithm>
#include <cmath>
#include <imgui.h>

NES::NES() : running(false), A(0), X(0), Y(0), S(0xFD), P(0x34), PC(0x8000) {
    frameBuffer.resize(256 * 240, 0xFF18181B); // Zinc-900
    std::memset(&header, 0, sizeof(header));
    header.isValid = false;
}

std::string NES::getName() const {
    return "Nintendo Entertainment System (NES)";
}

bool NES::loadROM(const std::string& romPath) {
    std::ifstream file(romPath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "Erro ao abrir ROM de NES: " << romPath << "\n";
        return false;
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    romData.resize(size);
    if (!file.read(reinterpret_cast<char*>(romData.data()), size)) {
        std::cerr << "Erro ao ler dados da ROM de NES.\n";
        return false;
    }

    size_t lastSlash = romPath.find_last_of("/\\");
    romFileName = (lastSlash == std::string::npos) ? romPath : romPath.substr(lastSlash + 1);

    parseHeader();
    running = true;
    return true;
}

void NES::parseHeader() {
    std::memset(&header, 0, sizeof(header));
    if (romData.size() >= 16 && 
        romData[0] == 'N' && romData[1] == 'E' && romData[2] == 'S' && romData[3] == 0x1A) {
        
        std::memcpy(header.magic, romData.data(), 4);
        header.prgRomCount = romData[4];
        header.chrRomCount = romData[5];
        header.flags6 = romData[6];
        header.flags7 = romData[7];
        header.prgRamSize = romData[8];
        header.flags9 = romData[9];
        header.flags10 = romData[10];
        header.mapperId = (header.flags6 >> 4) | (header.flags7 & 0xF0);
        header.isValid = true;
    } else {
        // Fallback simulation header
        std::memcpy(header.magic, "NES\x1A", 4);
        header.prgRomCount = 2; // 32KB
        header.chrRomCount = 1; // 8KB
        header.flags6 = 0x10;
        header.flags7 = 0;
        header.prgRamSize = 1;
        header.mapperId = 1; // MMC1
        header.isValid = false;
    }
}

void NES::step() {
    if (running) executeNextInstruction();
}

void NES::executeNextInstruction() {
    // Cycle PC and register values to simulate execution
    PC += 1;
    if (PC == 0) PC = 0x8000;
    
    A = (A + 1) & 0xFF;
    X = (X + 2) & 0xFF;
    Y = (Y + 1) & 0xFF;
}

void NES::reset() {
    A = 0;
    X = 0;
    Y = 0;
    S = 0xFD;
    PC = 0x8000;
    P = 0x34;
    std::fill(frameBuffer.begin(), frameBuffer.end(), 0xFF18181B);
}

void NES::stop() {
    running = false;
}

bool NES::isRunning() const {
    return running;
}

std::string NES::getStatusString() const {
    std::stringstream ss;
    ss << "NES Simulator [Ricoh 2A03 CPU]\n";
    ss << "ROM: " << (romFileName.empty() ? "Nenhuma carregada" : romFileName) << "\n";
    if (romFileName.empty()) {
        ss << "Por favor, selecione e carregue uma ROM do NES (*.nes).\n";
    } else {
        ss << "Tamanho ROM: " << (romData.size() / 1024.0f) << " KB\n";
    }
    ss << "Status: " << (running ? "Simulando" : "Pausado") << "\n";
    return ss.str();
}

int NES::getScreenWidth() const {
    return 256;
}

int NES::getScreenHeight() const {
    return 240;
}

void NES::drawLCD(uint32_t* pixelBuffer) {
    int w = getScreenWidth();
    int h = getScreenHeight();

    if (running && !romData.empty()) {
        // Draw retro background sky (NES Sky Blue)
        for (int i = 0; i < w * h; ++i) frameBuffer[i] = 0xFF5C94FC;

        // Draw ground bricks at the bottom (lines 208 to 239)
        for (int y = 208; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                if ((x % 16 == 0) || (y % 16 == 0)) {
                    frameBuffer[y * w + x] = 0xFF000000; // Black brick border
                } else {
                    frameBuffer[y * w + x] = 0xFFC84C0C; // Brick red/brown
                }
            }
        }

        // Draw a jumping/bouncing character mockup
        int playerX = 120 + (int)(60 * std::sin(PC * 0.05f));
        int playerY = 176 + (int)(24 * std::abs(std::cos(PC * 0.04f))); // Jump height
        
        for (int dy = -16; dy < 16; ++dy) {
            for (int dx = -8; dx < 8; ++dx) {
                int px = playerX + dx;
                int py = playerY + dy;
                if (px >= 0 && px < w && py >= 0 && py < 208) {
                    uint32_t c = 0xFFE45C10; // Red
                    if (dy < -8) c = 0xFFE45C10; // Cap/Hat
                    else if (dy >= -8 && dy < 0) c = 0xFFFFA400; // Skin tone
                    else if (dy >= 0 && dy < 8) c = 0xFF0020B8; // Blue Overalls
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

void NES::drawImGuiPanels() {
    // 1. Ricoh 2A03 CPU Registers
    ImGui::SetNextWindowPos(ImVec2(660, 345), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(300, 340), ImGuiCond_Always);
    ImGui::Begin("Registradores Ricoh 2A03 (NES)", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

    ImGui::Columns(2, "nes_regs", true);
    ImGui::Text("Registrador"); ImGui::NextColumn();
    ImGui::Text("Valor"); ImGui::NextColumn();
    ImGui::Separator();

    ImGui::Text("Acumulador (A)"); ImGui::NextColumn();
    ImGui::Text("0x%02X (%d)", A, A); ImGui::NextColumn();

    ImGui::Text("Indice X"); ImGui::NextColumn();
    ImGui::Text("0x%02X (%d)", X, X); ImGui::NextColumn();

    ImGui::Text("Indice Y"); ImGui::NextColumn();
    ImGui::Text("0x%02X (%d)", Y, Y); ImGui::NextColumn();

    ImGui::Text("Stack Pointer (S)"); ImGui::NextColumn();
    ImGui::Text("0x%02X", S); ImGui::NextColumn();

    ImGui::Text("Program Counter (PC)"); ImGui::NextColumn();
    ImGui::Text("0x%04X", PC); ImGui::NextColumn();

    ImGui::Text("Status Flags (P)"); ImGui::NextColumn();
    ImGui::Text("0x%02X", P); ImGui::NextColumn();
    ImGui::Columns(1);

    ImGui::Separator();
    ImGui::Text("Flags:");
    bool n = (P & 0x80) != 0;
    bool v = (P & 0x40) != 0;
    bool d = (P & 0x08) != 0;
    bool i = (P & 0x04) != 0;
    bool z = (P & 0x02) != 0;
    bool c = (P & 0x01) != 0;

    ImGui::Checkbox("N (Negative)", &n); ImGui::SameLine();
    ImGui::Checkbox("V (Overflow)", &v);
    ImGui::Checkbox("D (Decimal)", &d); ImGui::SameLine();
    ImGui::Checkbox("I (IRQ Disable)", &i); ImGui::SameLine();
    ImGui::Checkbox("Z (Zero)", &z); ImGui::SameLine();
    ImGui::Checkbox("C (Carry)", &c);

    ImGui::End();

    // 2. iNES Header Info
    ImGui::SetNextWindowPos(ImVec2(970, 35), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(300, 300), ImGuiCond_Always);
    ImGui::Begin("Cabecalho da ROM NES", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
    
    ImGui::Text("Signature: %c%c%c 0x%02X", header.magic[0], header.magic[1], header.magic[2], header.magic[3]);
    ImGui::Text("PRG ROM: %d bancos (%d KB)", header.prgRomCount, header.prgRomCount * 16);
    ImGui::Text("CHR ROM: %d bancos (%d KB)", header.chrRomCount, header.chrRomCount * 8);
    ImGui::Text("ID do Mapper: %d", header.mapperId);
    
    std::string mirrorStr = (header.flags6 & 0x01) ? "Vertical" : "Horizontal";
    if (header.flags6 & 0x08) mirrorStr = "Four-screen";
    ImGui::Text("Mirroring: %s", mirrorStr.c_str());
    
    ImGui::Text("SRAM (Battery): %s", (header.flags6 & 0x02) ? "Sim" : "Nao");
    ImGui::Text("Trainer: %s", (header.flags6 & 0x04) ? "Sim (512 bytes)" : "Nao");
    ImGui::Text("Status Cabecalho: %s", header.isValid ? "iNES Valido" : "Simulado");
    ImGui::End();
}

void NES::getAudioSamples(std::vector<float>& outBuffer) {
    outBuffer.clear();
}

const NES::iNESHeader& NES::getHeader() const {
    return header;
}

uint8_t NES::getA() const {
    return A;
}

uint8_t NES::getX() const {
    return X;
}

uint8_t NES::getY() const {
    return Y;
}

uint8_t NES::getS() const {
    return S;
}

uint8_t NES::getP() const {
    return P;
}

uint16_t NES::getPC() const {
    return PC;
}
