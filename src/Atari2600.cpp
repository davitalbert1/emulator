#include "Atari2600.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <cstring>
#include <algorithm>
#include <cmath>
#include <imgui.h>

Atari2600::Atari2600() : running(false), A(0), X(0), Y(0), SP(0xFD), PC(0xF000), SR(0x20) {
    frameBuffer.resize(160 * 192, 0xFF18181B); // Zinc-900
}

std::string Atari2600::getName() const {
    return "Atari 2600";
}

bool Atari2600::loadROM(const std::string& romPath) {
    std::ifstream file(romPath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "Erro ao abrir ROM de Atari: " << romPath << "\n";
        return false;
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    romData.resize(size);
    if (!file.read(reinterpret_cast<char*>(romData.data()), size)) {
        std::cerr << "Erro ao ler dados da ROM de Atari.\n";
        return false;
    }

    size_t lastSlash = romPath.find_last_of("/\\");
    romFileName = (lastSlash == std::string::npos) ? romPath : romPath.substr(lastSlash + 1);

    running = true;
    return true;
}

void Atari2600::step() {
    if (running) executeNextInstruction();
}

void Atari2600::executeNextInstruction() {
    // Cycle PC and register values to simulate execution
    PC += 1;
    if (PC == 0) PC = 0xF000;
    
    A = (A + 1) & 0xFF;
    X = (X + 2) & 0xFF;
    Y = (Y + 3) & 0xFF;
}

void Atari2600::reset() {
    A = 0;
    X = 0;
    Y = 0;
    SP = 0xFD;
    PC = 0xF000;
    SR = 0x20;
    std::fill(frameBuffer.begin(), frameBuffer.end(), 0xFF18181B);
}

void Atari2600::stop() {
    running = false;
}

bool Atari2600::isRunning() const {
    return running;
}

std::string Atari2600::getStatusString() const {
    std::stringstream ss;
    ss << "Atari 2600 Simulator [MOS 6507]\n";
    ss << "ROM: " << (romFileName.empty() ? "Nenhuma carregada" : romFileName) << "\n";
    if (romFileName.empty()) {
        ss << "Por favor, selecione e carregue uma ROM do Atari 2600 (*.a26 ou *.bin).\n";
    } else {
        ss << "Tamanho ROM: " << romData.size() << " bytes (SRAM/Mapeamento Direto)\n";
    }
    ss << "Status: " << (running ? "Simulando" : "Pausado") << "\n";
    return ss.str();
}

int Atari2600::getScreenWidth() const {
    return 160;
}

int Atari2600::getScreenHeight() const {
    return 192;
}

void Atari2600::drawLCD(uint32_t* pixelBuffer) {
    int w = getScreenWidth();
    int h = getScreenHeight();

    if (running && !romData.empty()) {
        // Draw horizontal color bars typical of Atari 2600's TIA output
        for (int y = 0; y < h; ++y) {
            uint32_t rowColor = 0xFF18181B;
            int section = y / 24;
            switch (section) {
                case 0: rowColor = 0xFFEF4444; break; // Red
                case 1: rowColor = 0xFFF97316; break; // Orange
                case 2: rowColor = 0xFFEAB308; break; // Yellow
                case 3: rowColor = 0xFF22C55E; break; // Green
                case 4: rowColor = 0xFF06B6D4; break; // Cyan
                case 5: rowColor = 0xFF3B82F6; break; // Blue
                case 6: rowColor = 0xFF8B5CF6; break; // Purple
                case 7: rowColor = 0xFFEC4899; break; // Pink
            }
            for (int x = 0; x < w; ++x) frameBuffer[y * w + x] = rowColor;
        }

        // Draw a bouncing player square sprite (white)
        int playerX = 80 + (int)(50 * std::sin(PC * 0.05f));
        int playerY = 96 + (int)(40 * std::cos(PC * 0.03f));
        for (int dy = -4; dy <= 4; ++dy) {
            for (int dx = -4; dx <= 4; ++dx) {
                int px = playerX + dx;
                int py = playerY + dy;
                if (px >= 0 && px < w && py >= 0 && py < h) frameBuffer[py * w + px] = 0xFFFFFFFF; // White player sprite
            }
        }
    } else {
        std::fill(frameBuffer.begin(), frameBuffer.end(), 0xFF18181B);
    }

    std::memcpy(pixelBuffer, frameBuffer.data(), w * h * sizeof(uint32_t));
}

void Atari2600::drawImGuiPanels() {
    // 1. MOS 6507 CPU Registers
    ImGui::SetNextWindowPos(ImVec2(660, 345), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(300, 340), ImGuiCond_Always);
    ImGui::Begin("Registradores MOS 6507 (Atari)", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

    ImGui::Columns(2, "atari_regs", true);
    ImGui::Text("Registrador"); ImGui::NextColumn();
    ImGui::Text("Valor"); ImGui::NextColumn();
    ImGui::Separator();

    ImGui::Text("Acumulador (A)"); ImGui::NextColumn();
    ImGui::Text("0x%02X (%d)", A, A); ImGui::NextColumn();

    ImGui::Text("Indice X"); ImGui::NextColumn();
    ImGui::Text("0x%02X (%d)", X, X); ImGui::NextColumn();

    ImGui::Text("Indice Y"); ImGui::NextColumn();
    ImGui::Text("0x%02X (%d)", Y, Y); ImGui::NextColumn();

    ImGui::Text("Stack Pointer (SP)"); ImGui::NextColumn();
    ImGui::Text("0x%02X", SP); ImGui::NextColumn();

    ImGui::Text("Program Counter (PC)"); ImGui::NextColumn();
    ImGui::Text("0x%04X", PC); ImGui::NextColumn();

    ImGui::Text("Status Register (SR)"); ImGui::NextColumn();
    ImGui::Text("0x%02X", SR); ImGui::NextColumn();
    ImGui::Columns(1);

    ImGui::Separator();
    ImGui::Text("Flags:");
    bool n = (SR & 0x80) != 0;
    bool v = (SR & 0x40) != 0;
    bool b = (SR & 0x10) != 0;
    bool d = (SR & 0x08) != 0;
    bool i = (SR & 0x04) != 0;
    bool z = (SR & 0x02) != 0;
    bool c = (SR & 0x01) != 0;

    ImGui::Checkbox("N (Negative)", &n); ImGui::SameLine();
    ImGui::Checkbox("V (Overflow)", &v);
    ImGui::Checkbox("B (Break)", &b); ImGui::SameLine();
    ImGui::Checkbox("D (Decimal)", &d); ImGui::SameLine();
    ImGui::Checkbox("I (IRQ Disable)", &i);
    ImGui::Checkbox("Z (Zero)", &z); ImGui::SameLine();
    ImGui::Checkbox("C (Carry)", &c);

    ImGui::End();

    // 2. Atari Cartridge details
    ImGui::SetNextWindowPos(ImVec2(970, 35), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(300, 300), ImGuiCond_Always);
    ImGui::Begin("Cartucho Atari 2600", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
    ImGui::Text("Nome ROM: %s", romFileName.empty() ? "Nenhum" : romFileName.c_str());
    ImGui::Text("Tamanho: %d bytes", (int)romData.size());
    ImGui::Text("Arquitetura: MOS 6507");
    ImGui::Text("Coprocessador: TIA (Televisao)");
    ImGui::Text("RAM do Console: 128 bytes (RIOT)");
    ImGui::End();
}

void Atari2600::getAudioSamples(std::vector<float>& outBuffer) {
    outBuffer.clear();
}

uint8_t Atari2600::getA() const {
    return A;
}

uint8_t Atari2600::getX() const {
    return X;
}

uint8_t Atari2600::getY() const {
    return Y;
}

uint8_t Atari2600::getSP() const {
    return SP;
}

uint16_t Atari2600::getPC() const {
    return PC;
}

uint8_t Atari2600::getSR() const {
    return SR;
}
