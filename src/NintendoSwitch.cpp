#include "NintendoSwitch.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <cstring>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <imgui.h>

NintendoSwitch::NintendoSwitch() : running(false), SP(0x0000007FFFFFF000ULL), PC(0x0000000000000100ULL), pstate(0x60000000) {
    std::memset(xRegisters, 0, sizeof(xRegisters));
    xRegisters[0] = 1; // Return code/status
    xRegisters[29] = 0x0000007FFFFFF030ULL; // FP (Frame Pointer X29)
    xRegisters[30] = 0x0000000000000040ULL; // LR (Link Register X30)

    frameBuffer.resize(1280 * 720, 0xFF191919); // Zinc-900 Dark Mode
    std::memset(&header, 0, sizeof(header));
    header.isValid = false;
}

std::string NintendoSwitch::getName() const {
    return "Nintendo Switch";
}

bool NintendoSwitch::loadROM(const std::string& romPath) {
    std::ifstream file(romPath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "Erro ao abrir executavel de Switch: " << romPath << "\n";
        return false;
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    romData.resize(size);
    if (!file.read(reinterpret_cast<char*>(romData.data()), size)) {
        std::cerr << "Erro ao ler dados do Switch.\n";
        return false;
    }

    size_t lastSlash = romPath.find_last_of("/\\");
    romFileName = (lastSlash == std::string::npos) ? romPath : romPath.substr(lastSlash + 1);

    parseHeader();
    running = true;
    return true;
}

void NintendoSwitch::parseHeader() {
    std::memset(&header, 0, sizeof(header));
    if (romData.size() >= 80) {
        header.entryPoint = romData[0x10] | (romData[0x11] << 8) | (romData[0x12] << 16) | (romData[0x13] << 24);
        
        std::memcpy(header.magic, romData.data() + 0x14, 4);
        header.magic[4] = '\0';
        
        header.size = romData[0x18] | (romData[0x19] << 8) | (romData[0x1A] << 16) | (romData[0x1B] << 24);
        header.flags = romData[0x1C] | (romData[0x1D] << 8) | (romData[0x1E] << 16) | (romData[0x1F] << 24);
        header.textSize = romData[0x20] | (romData[0x21] << 8) | (romData[0x22] << 16) | (romData[0x23] << 24);
        header.roSize = romData[0x24] | (romData[0x25] << 8) | (romData[0x26] << 16) | (romData[0x27] << 24);
        header.dataSize = romData[0x28] | (romData[0x29] << 8) | (romData[0x2A] << 16) | (romData[0x2B] << 24);
        header.bssSize = romData[0x2C] | (romData[0x2D] << 8) | (romData[0x2E] << 16) | (romData[0x2F] << 24);
        
        std::memcpy(header.buildId, romData.data() + 0x30, 32);
        
        if (std::strcmp(header.magic, "NRO0") == 0 || std::strcmp(header.magic, "NSO0") == 0) {
            header.isValid = true;
        } else {
            header.isValid = false;
        }
    } else {
        header.entryPoint = 0x00000080;
        std::strcpy(header.magic, "NRO0");
        header.size = 0x025A0000;
        header.flags = 0;
        header.textSize = 0x008A0000;
        header.roSize = 0x00A00000;
        header.dataSize = 0x00C00000;
        header.bssSize = 0x00100000;
        std::memset(header.buildId, 0xAB, 32);
        header.isValid = false;
    }
}

void NintendoSwitch::step() {
    if (running) executeNextInstruction();
}

void NintendoSwitch::executeNextInstruction() {
    // Cycle 64-bit PC and registers
    PC += 4;
    if (PC >= 0x00007FFFFFFFFFFFULL) PC = 0x0000000000000100ULL;

    // Shift registers for active visual debugging
    xRegisters[0] = (xRegisters[0] + 1) & 0xFFFFFFFFFFFFFFFFULL;
    xRegisters[1] = (xRegisters[1] + 5) & 0x00FFFFFFFFFFFFFFULL;
}

void NintendoSwitch::reset() {
    std::memset(xRegisters, 0, sizeof(xRegisters));
    SP = 0x0000007FFFFFF000ULL;
    PC = 0x0000000000000100ULL;
    pstate = 0x60000000;
    std::fill(frameBuffer.begin(), frameBuffer.end(), 0xFF191919);
}

void NintendoSwitch::stop() {
    running = false;
}

bool NintendoSwitch::isRunning() const {
    return running;
}

std::string NintendoSwitch::getStatusString() const {
    std::stringstream ss;
    ss << "Nintendo Switch Simulator [ARMv8-A 64-bit]\n";
    ss << "Executavel/NRO: " << (romFileName.empty() ? "Nenhum carregado" : romFileName) << "\n";
    if (romFileName.empty()) {
        ss << "Por favor, selecione e carregue um executavel do Switch (*.nsp, *.xci ou *.nro).\n";
    } else {
        ss << "Tamanho Executavel: " << (romData.size() / 1024.0f / 1024.0f) << " MB\n";
    }
    ss << "Status: " << (running ? "Simulando" : "Pausado") << "\n";
    return ss.str();
}

int NintendoSwitch::getScreenWidth() const {
    return 1280;
}

int NintendoSwitch::getScreenHeight() const {
    return 720;
}

void NintendoSwitch::drawLCD(uint32_t* pixelBuffer) {
    int w = getScreenWidth();
    int h = getScreenHeight();

    if (running && !romData.empty()) {
        // Draw Switch dark mode dashboard background
        std::fill(frameBuffer.begin(), frameBuffer.end(), 0xFF191919);

        // Draw top bar
        for (int y = 0; y < 40; ++y) {
            for (int x = 0; x < w; ++x) frameBuffer[y * w + x] = 0xFF222222; // Dark header
        }

        // Draw active game card (Zelda theme)
        int cardX = 490;
        int cardY = 160;
        for (int y = cardY; y < cardY + 300; ++y) {
            for (int x = cardX; x < cardX + 300; ++x) {
                int relX = x - (cardX + 150);
                int relY = y - (cardY + 150);
                if (std::abs(relX) < 140 && std::abs(relY) < 140) {
                    if (relY > -100 && relY < 100 && std::abs(relX) < (100 - relY)) {
                        frameBuffer[y * w + x] = 0xFFD97706; // Amber gold
                    } else {
                        frameBuffer[y * w + x] = 0xFF0D9488; // Teal
                    }
                } else {
                    frameBuffer[y * w + x] = 0xFF3F3F46; // Border
                }
            }
        }

        // Draw active glowing blue outline (pulses with PC cycles)
        float glow = std::sin(PC * 0.05f) * 4.0f + 6.0f;
        for (int y = cardY - (int)glow; y < cardY + 300 + (int)glow; ++y) {
            for (int x = cardX - (int)glow; x < cardX + 300 + (int)glow; ++x) {
                if (x >= 0 && x < w && y >= 0 && y < h) {
                    if (x < cardX || x >= cardX + 300 || y < cardY || y >= cardY + 300) {
                        frameBuffer[y * w + x] = 0xFF0EA5E9; // Sky blue glow
                    }
                }
            }
        }

        // Draw bottom round icons
        int centers[8] = { 340, 420, 500, 580, 660, 740, 820, 900 };
        uint32_t colors[8] = { 0xFFEF4444, 0xFFF59E0B, 0xFF10B981, 0xFF3B82F6, 0xFF6366F1, 0xFF8B5CF6, 0xFFEC4899, 0xFF6B7280 };
        int radius = 25;

        for (int i = 0; i < 8; ++i) {
            int cx = centers[i];
            int cy = 580;
            for (int y = cy - radius; y < cy + radius; ++y) {
                for (int x = cx - radius; x < cx + radius; ++x) {
                    if (x >= 0 && x < w && y >= 0 && y < h) {
                        if ((x - cx)*(x - cx) + (y - cy)*(y - cy) < radius*radius) {
                            frameBuffer[y * w + x] = colors[i];
                        }
                    }
                }
            }
        }
    } else {
        std::fill(frameBuffer.begin(), frameBuffer.end(), 0xFF18181B);
    }

    std::memcpy(pixelBuffer, frameBuffer.data(), w * h * sizeof(uint32_t));
}

void NintendoSwitch::drawImGuiPanels() {
    // 1. ARMv8-A CPU registers
    ImGui::SetNextWindowPos(ImVec2(660, 345), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(300, 340), ImGuiCond_Always);
    ImGui::Begin("Registradores ARM64 (Switch)", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

    if (ImGui::BeginTabBar("SwitchRegisters")) {
        if (ImGui::BeginTabItem("X0-X15")) {
            ImGui::Columns(2, "switch_x0_x15", true);
            ImGui::Text("Reg"); ImGui::NextColumn();
            ImGui::Text("Valor (64-bit)"); ImGui::NextColumn();
            ImGui::Separator();
            for (int i = 0; i < 16; ++i) {
                ImGui::Text("X%d", i); ImGui::NextColumn();
                ImGui::Text("0x%016llX", (unsigned long long)xRegisters[i]); ImGui::NextColumn();
            }
            ImGui::Columns(1);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("X16-X30")) {
            ImGui::Columns(2, "switch_x16_x30", true);
            ImGui::Text("Reg"); ImGui::NextColumn();
            ImGui::Text("Valor (64-bit)"); ImGui::NextColumn();
            ImGui::Separator();
            for (int i = 16; i < 31; ++i) {
                std::string label = "X" + std::to_string(i);
                if (i == 29) label = "FP (X29)";
                else if (i == 30) label = "LR (X30)";
                ImGui::Text("%s", label.c_str()); ImGui::NextColumn();
                ImGui::Text("0x%016llX", (unsigned long long)xRegisters[i]); ImGui::NextColumn();
            }
            ImGui::Columns(1);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Especiais")) {
            ImGui::Text("Program Counter (PC): 0x%016llX", (unsigned long long)PC);
            ImGui::Text("Stack Pointer (SP): 0x%016llX", (unsigned long long)SP);
            ImGui::Separator();
            ImGui::Text("Processor State (PSTATE): 0x%08X", pstate);
            
            bool n = (pstate & 0x80000000) != 0;
            bool z = (pstate & 0x40000000) != 0;
            bool c = (pstate & 0x20000000) != 0;
            bool v = (pstate & 0x10000000) != 0;
            
            ImGui::Text("ALU Condition Flags:");
            ImGui::BulletText("Negative (N): %d", n);
            ImGui::BulletText("Zero (Z): %d", z);
            ImGui::BulletText("Carry (C): %d", c);
            ImGui::BulletText("Overflow (V): %d", v);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();

    // 2. NRO Header Info
    ImGui::SetNextWindowPos(ImVec2(970, 35), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(300, 300), ImGuiCond_Always);
    ImGui::Begin("Cabecalho da ROM Switch", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
    ImGui::Text("Magic Code: %s", header.magic);
    ImGui::Text("Ponto Entrada: 0x%08X", header.entryPoint);
    ImGui::Text("Tamanho Executavel: %d bytes", header.size);
    ImGui::Text("Flags: 0x%08X", header.flags);
    ImGui::Separator();
    ImGui::Text("Tamanho .text: %d bytes", header.textSize);
    ImGui::Text("Tamanho .ro:   %d bytes", header.roSize);
    ImGui::Text("Tamanho .data: %d bytes", header.dataSize);
    ImGui::Text("Tamanho .bss:  %d bytes", header.bssSize);
    ImGui::Separator();
    
    // Print build ID as hex string
    std::stringstream ss;
    for (int i = 0; i < 16; ++i) ss << std::hex << std::setw(2) << std::setfill('0') << (int)header.buildId[i];
    ImGui::TextWrapped("Build ID (Trunc): %s...", ss.str().c_str());
    ImGui::Text("Status Cabecalho: %s", header.isValid ? "NRO/NSO Valido" : "Simulado");
    ImGui::End();
}

void NintendoSwitch::getAudioSamples(std::vector<float>& outBuffer) {
    outBuffer.clear();
}

const NintendoSwitch::NroHeader& NintendoSwitch::getHeader() const {
    return header;
}

uint64_t NintendoSwitch::getXRegister(int index) const {
    return (index >= 0 && index < 31) ? xRegisters[index] : 0;
}

uint64_t NintendoSwitch::getPC() const {
    return PC;
}

uint64_t NintendoSwitch::getSP() const {
    return SP;
}

uint32_t NintendoSwitch::getPstate() const {
    return pstate;
}
