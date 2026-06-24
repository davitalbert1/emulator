#include "SegaGenesis.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <cstring>
#include <algorithm>
#include <cmath>
#include <imgui.h>

static uint32_t readBE32(const uint8_t* ptr) {
    return (ptr[0] << 24) | (ptr[1] << 16) | (ptr[2] << 8) | ptr[3];
}

static uint16_t readBE16(const uint8_t* ptr) {
    return (ptr[0] << 8) | ptr[1];
}

SegaGenesis::SegaGenesis() : running(false), SR(0x2700), PC(0x00000200) {
    std::memset(D, 0, sizeof(D));
    std::memset(A, 0, sizeof(A));
    A[7] = 0x00FFFE00; // Initial Stack Pointer
    
    frameBuffer.resize(320 * 224, 0xFF18181B); // Zinc-900
    std::memset(&header, 0, sizeof(header));
    header.isValid = false;
}

std::string SegaGenesis::getName() const {
    return "Sega Genesis / Mega Drive";
}

bool SegaGenesis::loadROM(const std::string& romPath) {
    std::ifstream file(romPath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "Erro ao abrir ROM de Mega Drive: " << romPath << "\n";
        return false;
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    romData.resize(size);
    if (!file.read(reinterpret_cast<char*>(romData.data()), size)) {
        std::cerr << "Erro ao ler dados da ROM de Mega Drive.\n";
        return false;
    }

    size_t lastSlash = romPath.find_last_of("/\\");
    romFileName = (lastSlash == std::string::npos) ? romPath : romPath.substr(lastSlash + 1);

    parseHeader();
    running = true;
    return true;
}

void SegaGenesis::parseHeader() {
    std::memset(&header, 0, sizeof(header));
    if (romData.size() >= 0x200) {
        // Parse SEGA header at 0x100
        std::memcpy(header.consoleName, romData.data() + 0x100, 16);
        header.consoleName[16] = '\0';

        std::memcpy(header.copyright, romData.data() + 0x110, 16);
        header.copyright[16] = '\0';

        std::memcpy(header.domesticTitle, romData.data() + 0x120, 48);
        header.domesticTitle[48] = '\0';

        std::memcpy(header.overseasTitle, romData.data() + 0x150, 48);
        header.overseasTitle[48] = '\0';

        std::memcpy(header.productCode, romData.data() + 0x180, 14);
        header.productCode[14] = '\0';

        header.checksum = readBE16(romData.data() + 0x18E);
        header.romStart = readBE32(romData.data() + 0x1A0);
        header.romEnd = readBE32(romData.data() + 0x1A4);
        header.ramStart = readBE32(romData.data() + 0x1A8);
        header.ramEnd = readBE32(romData.data() + 0x1AB);
        header.isValid = true;
    } else {
        // Fallback simulation header
        std::strncpy(header.consoleName, "SEGA MEGA DRIVE", sizeof(header.consoleName) - 1);
        std::strncpy(header.copyright, "(C)SEGA 1991.JUN", sizeof(header.copyright) - 1);
        std::strncpy(header.domesticTitle, "SONIC THE HEDGEHOG", sizeof(header.domesticTitle) - 1);
        std::strncpy(header.overseasTitle, "SONIC THE HEDGEHOG", sizeof(header.overseasTitle) - 1);
        std::strncpy(header.productCode, "GM 00001009-00", sizeof(header.productCode) - 1);
        header.checksum = 0x5D4B;
        header.romStart = 0x00000000;
        header.romEnd = 0x0007FFFF;
        header.ramStart = 0x00FF0000;
        header.ramEnd = 0x00FFFFFF;
        header.isValid = false;
    }
}

void SegaGenesis::step() {
    if (running) {
        executeNextInstruction();
    }
}

void SegaGenesis::executeNextInstruction() {
    // Cycle PC and registers for 68000 CPU simulation
    PC += 2;
    if (PC >= 0x01000000) PC = 0x00000200;

    // Cycle registers slightly
    D[0] = (D[0] + 1) & 0xFFFFFFFF;
    D[1] = (D[1] + 2) & 0xFFFFFFFF;
    A[0] = (A[0] + 4) & 0xFFFFFFFF;
}

void SegaGenesis::reset() {
    std::memset(D, 0, sizeof(D));
    std::memset(A, 0, sizeof(A));
    A[7] = 0x00FFFE00;
    PC = 0x00000200;
    SR = 0x2700;
    std::fill(frameBuffer.begin(), frameBuffer.end(), 0xFF18181B);
}

void SegaGenesis::stop() {
    running = false;
}

bool SegaGenesis::isRunning() const {
    return running;
}

std::string SegaGenesis::getStatusString() const {
    std::stringstream ss;
    ss << "Sega Genesis Simulator [Motorola 68000]\n";
    ss << "ROM: " << (romFileName.empty() ? "Nenhuma carregada" : romFileName) << "\n";
    if (romFileName.empty()) {
        ss << "Por favor, selecione e carregue uma ROM do Sega Genesis (*.gen ou *.md).\n";
    } else {
        ss << "Tamanho ROM: " << (romData.size() / 1024.0f / 1024.0f) << " MB\n";
    }
    ss << "Status: " << (running ? "Simulando" : "Pausado") << "\n";
    return ss.str();
}

int SegaGenesis::getScreenWidth() const {
    return 320;
}

int SegaGenesis::getScreenHeight() const {
    return 224;
}

void SegaGenesis::drawLCD(uint32_t* pixelBuffer) {
    int w = getScreenWidth();
    int h = getScreenHeight();

    if (running && !romData.empty()) {
        // Parallax background sky (Deep Sega Blue)
        for (int i = 0; i < w * h; ++i) {
            frameBuffer[i] = 0xFF0050C0;
        }

        // Draw parallax background hills (lines 96 to 143)
        for (int y = 96; y < 144; ++y) {
            for (int x = 0; x < w; ++x) {
                int hillY = 120 + (int)(16 * std::sin((x + PC * 0.5f) * 0.03f));
                if (y > hillY) {
                    frameBuffer[y * w + x] = 0xFF007A00; // Dark Green hill
                }
            }
        }

        // Draw Sonic-style checkerboard soil (lines 144 to 223)
        for (int y = 144; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                int scrolledX = (x + (int)PC) % w;
                int checkX = (scrolledX / 16) % 2;
                int checkY = (y / 16) % 2;
                if (y < 160) {
                    if ((checkX == 0) ^ (checkY == 0)) {
                        frameBuffer[y * w + x] = 0xFF00A200; // Light Green
                    } else {
                        frameBuffer[y * w + x] = 0xFF006C00; // Dark Green
                    }
                } else {
                    if ((checkX == 0) ^ (checkY == 0)) {
                        frameBuffer[y * w + x] = 0xFFC65A00; // Light Brown
                    } else {
                        frameBuffer[y * w + x] = 0xFF8C3E00; // Dark Brown
                    }
                }
            }
        }

        // Draw a rolling/bouncing blue ball (Sonic)
        int playerX = 160 + (int)(80 * std::sin(PC * 0.03f));
        int playerY = 136 + (int)(15 * std::abs(std::cos(PC * 0.05f)));
        int radius = 8;
        for (int dy = -radius; dy <= radius; ++dy) {
            for (int dx = -radius; dx <= radius; ++dx) {
                if (dx*dx + dy*dy <= radius*radius) {
                    int px = playerX + dx;
                    int py = playerY + dy;
                    if (px >= 0 && px < w && py >= 0 && py < h) {
                        uint32_t c = 0xFF0022EE; // Blue
                        if (dx > 2 && dy > 2) c = 0xFFEF4444; // Red Shoes
                        else if (dx < 0 && dy > -2 && dy < 4) c = 0xFFFFA400; // Peach Face/Belly
                        
                        frameBuffer[py * w + px] = c;
                    }
                }
            }
        }
    } else {
        std::fill(frameBuffer.begin(), frameBuffer.end(), 0xFF18181B);
    }

    std::memcpy(pixelBuffer, frameBuffer.data(), w * h * sizeof(uint32_t));
}

void SegaGenesis::drawImGuiPanels() {
    // 1. Motorola 68000 CPU Registers
    ImGui::SetNextWindowPos(ImVec2(660, 345), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(300, 340), ImGuiCond_Always);
    ImGui::Begin("Registradores M68000 (Genesis)", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

    if (ImGui::BeginTabBar("M68KRegisters")) {
        if (ImGui::BeginTabItem("Registradores Dados (D)")) {
            ImGui::Columns(2, "m68k_d_regs", true);
            ImGui::Text("Registrador"); ImGui::NextColumn();
            ImGui::Text("Valor"); ImGui::NextColumn();
            ImGui::Separator();
            for (int i = 0; i < 8; ++i) {
                ImGui::Text("D%d", i); ImGui::NextColumn();
                ImGui::Text("0x%08X", D[i]); ImGui::NextColumn();
            }
            ImGui::Columns(1);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Registradores Endereco (A)")) {
            ImGui::Columns(2, "m68k_a_regs", true);
            ImGui::Text("Registrador"); ImGui::NextColumn();
            ImGui::Text("Valor"); ImGui::NextColumn();
            ImGui::Separator();
            for (int i = 0; i < 8; ++i) {
                std::string label = "A" + std::to_string(i);
                if (i == 7) label = "A7 (SP)";
                ImGui::Text("%s", label.c_str()); ImGui::NextColumn();
                ImGui::Text("0x%08X", A[i]); ImGui::NextColumn();
            }
            ImGui::Columns(1);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("CPU Status")) {
            ImGui::Text("Program Counter (PC): 0x%08X", PC);
            ImGui::Text("Status Register (SR): 0x%04X", SR);
            ImGui::Separator();
            
            // Output flags of Status Register
            bool s = (SR & 0x2000) != 0; // Supervisor state
            bool t = (SR & 0x8000) != 0; // Trace state
            int ipl = (SR & 0x0700) >> 8; // Interrupt mask
            
            ImGui::Text("Modo: %s", s ? "Supervisor" : "Usuario");
            ImGui::Text("Trace: %s", t ? "Ativo" : "Inativo");
            ImGui::Text("Mascara Interrupcoes: %d", ipl);
            ImGui::Separator();
            
            bool n = (SR & 0x0008) != 0;
            bool z = (SR & 0x0004) != 0;
            bool v = (SR & 0x0002) != 0;
            bool c = (SR & 0x0001) != 0;
            bool x = (SR & 0x0010) != 0;
            
            ImGui::Text("Flags:");
            ImGui::Checkbox("X (Extend)", &x); ImGui::SameLine();
            ImGui::Checkbox("N (Negative)", &n);
            ImGui::Checkbox("Z (Zero)", &z); ImGui::SameLine();
            ImGui::Checkbox("V (Overflow)", &v); ImGui::SameLine();
            ImGui::Checkbox("C (Carry)", &c);
            
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();

    // 2. Genesis ROM Header Info
    ImGui::SetNextWindowPos(ImVec2(970, 35), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(300, 300), ImGuiCond_Always);
    ImGui::Begin("Cabecalho da ROM Sega Genesis", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
    ImGui::Text("Console: %s", header.consoleName);
    ImGui::Text("Copyright: %s", header.copyright);
    ImGui::Text("Titulo: %s", header.domesticTitle);
    ImGui::Text("Ref Serial: %s", header.productCode);
    ImGui::Text("Checksum: 0x%04X", header.checksum);
    ImGui::Separator();
    ImGui::Text("Limites ROM: 0x%08X - 0x%08X", header.romStart, header.romEnd);
    ImGui::Text("Limites RAM: 0x%08X - 0x%08X", header.ramStart, header.ramEnd);
    ImGui::Text("Status Cabecalho: %s", header.isValid ? "Sega Valido" : "Simulado");
    ImGui::End();
}

void SegaGenesis::getAudioSamples(std::vector<float>& outBuffer) {
    outBuffer.clear();
}

const SegaGenesis::GenesisHeader& SegaGenesis::getHeader() const { return header; }
uint32_t SegaGenesis::getD(int index) const { return (index >= 0 && index < 8) ? D[index] : 0; }
uint32_t SegaGenesis::getA(int index) const { return (index >= 0 && index < 8) ? A[index] : 0; }
uint16_t SegaGenesis::getSR() const { return SR; }
uint32_t SegaGenesis::getPC() const { return PC; }
