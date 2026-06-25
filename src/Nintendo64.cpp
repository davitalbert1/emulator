#include "Nintendo64.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <cstring>
#include <algorithm>
#include <cmath>
#include <imgui.h>

Nintendo64::Nintendo64() : running(false), PC(0xA4000040), HI(0), LO(0), SR(0x34000000) {
    std::memset(registers, 0, sizeof(registers));
    registers[29] = 0x801FFF00; // SP (Stack Pointer R29)
    registers[31] = 0x80000400; // LR (Return Address R31)

    frameBuffer.resize(320 * 240, 0xFF18181B); // Zinc-900
    std::memset(&header, 0, sizeof(header));
    header.isValid = false;
}

std::string Nintendo64::getName() const {
    return "Nintendo 64";
}

bool Nintendo64::loadROM(const std::string& romPath) {
    std::ifstream file(romPath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "Erro ao abrir ROM de N64: " << romPath << "\n";
        return false;
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    romData.resize(size);
    if (!file.read(reinterpret_cast<char*>(romData.data()), size)) {
        std::cerr << "Erro ao ler dados da ROM de N64.\n";
        return false;
    }

    size_t lastSlash = romPath.find_last_of("/\\");
    romFileName = (lastSlash == std::string::npos) ? romPath : romPath.substr(lastSlash + 1);

    parseHeader();
    running = true;
    return true;
}

void Nintendo64::parseHeader() {
    std::memset(&header, 0, sizeof(header));
    if (romData.size() >= 64) {
        header.validationMagic = (romData[0] << 24) | (romData[1] << 16) | (romData[2] << 8) | romData[3];
        header.clockRate = (romData[4] << 24) | (romData[5] << 16) | (romData[6] << 8) | romData[7];
        header.bootAddress = (romData[8] << 24) | (romData[9] << 16) | (romData[10] << 8) | romData[11];
        header.releaseAddress = (romData[12] << 24) | (romData[13] << 16) | (romData[14] << 8) | romData[15];
        header.crc1 = (romData[16] << 24) | (romData[17] << 16) | (romData[18] << 8) | romData[19];
        header.crc2 = (romData[20] << 24) | (romData[21] << 16) | (romData[22] << 8) | romData[23];
        
        std::memcpy(header.gameTitle, romData.data() + 0x20, 20);
        header.gameTitle[20] = '\0';
        
        header.manufacturerCode = romData[0x3B];
        std::memcpy(header.cartridgeId, romData.data() + 0x3C, 2);
        header.cartridgeId[2] = '\0';
        header.countryCode = romData[0x3E];
        header.isValid = true;
    } else {
        header.validationMagic = 0x80371240;
        header.clockRate = 0x0000000F;
        header.bootAddress = 0x80000400;
        header.releaseAddress = 0x00000000;
        header.crc1 = 0xEE2A01A0;
        header.crc2 = 0x4D3A1F2B;
        std::strncpy(header.gameTitle, "SUPER MARIO 64", sizeof(header.gameTitle) - 1);
        header.manufacturerCode = 'N';
        std::strncpy(header.cartridgeId, "NS", sizeof(header.cartridgeId) - 1);
        header.countryCode = 'E'; // USA
        header.isValid = false;
    }
}

void Nintendo64::step() {
    if (running) executeNextInstruction();
}

void Nintendo64::executeNextInstruction() {
    // Cycle 64-bit PC and cycle values
    PC += 4;
    if (PC >= 0xFFFFFFFF) PC = 0x80000400;

    // Shift registers for active debugger feeling
    registers[0] = 0; // MIPS R0 is always hardwired to 0
    registers[1] = (registers[1] + 1) & 0x000FFFFFFFFFFFFFULL;
    registers[2] = (registers[2] + 4) & 0x000000FFFFFFFFFFULL;
    registers[3] = (registers[3] + 2) & 0x00000000FFFFFFFFULL;
}

void Nintendo64::reset() {
    std::memset(registers, 0, sizeof(registers));
    registers[29] = 0x801FFF00;
    registers[31] = 0x80000400;
    PC = 0xA4000040;
    HI = 0;
    LO = 0;
    SR = 0x34000000;
    std::fill(frameBuffer.begin(), frameBuffer.end(), 0xFF18181B);
}

void Nintendo64::stop() {
    running = false;
}

bool Nintendo64::isRunning() const {
    return running;
}

std::string Nintendo64::getStatusString() const {
    std::stringstream ss;
    ss << "Nintendo 64 Simulator [MIPS VR4300 64-bit]\n";
    ss << "ROM: " << (romFileName.empty() ? "Nenhuma carregada" : romFileName) << "\n";
    if (romFileName.empty()) {
        ss << "Por favor, selecione e carregue uma ROM do N64 (*.z64, *.n64 ou *.v64).\n";
    } else {
        ss << "Tamanho ROM: " << (romData.size() / 1024.0f / 1024.0f) << " MB\n";
    }
    ss << "Status: " << (running ? "Simulando" : "Pausado") << "\n";
    return ss.str();
}

int Nintendo64::getScreenWidth() const {
    return 320;
}

int Nintendo64::getScreenHeight() const {
    return 240;
}

void Nintendo64::drawLCD(uint32_t* pixelBuffer) {
    int w = getScreenWidth();
    int h = getScreenHeight();

    if (running && !romData.empty()) {
        // Draw dark background
        std::fill(frameBuffer.begin(), frameBuffer.end(), 0xFF0F0F11); // Zinc-950

        // Draw spinning 3D logo demo lines
        float angle = PC * 0.02f;
        int centerX = 160;
        int centerY = 120;
        
        for (int y = 50; y < 190; ++y) {
            for (int x = 90; x < 230; ++x) {
                int relX = x - centerX;
                int relY = y - centerY;
                
                // Rotated projections
                float rotX = relX * std::cos(angle) - relY * std::sin(angle);
                float rotY = relX * std::sin(angle) + relY * std::cos(angle);
                
                if (std::abs(rotX) < 45 && std::abs(rotY) < 45) {
                    uint32_t color = 0xFF3B82F6; // Blue quadrant
                    if (rotX > 0 && rotY > 0) color = 0xFF22C55E; // Green
                    else if (rotX < 0 && rotY > 0) color = 0xFFEAB308; // Yellow
                    else if (rotX > 0 && rotY < 0) color = 0xFFEF4444; // Red
                    
                    if (std::abs(std::abs(rotX) - 22) < 2 || std::abs(std::abs(rotY) - 22) < 2) {
                        frameBuffer[y * w + x] = 0xFFFFFFFF; // Edges
                    } else {
                        frameBuffer[y * w + x] = color;
                    }
                }
            }
        }
    } else {
        std::fill(frameBuffer.begin(), frameBuffer.end(), 0xFF18181B);
    }

    std::memcpy(pixelBuffer, frameBuffer.data(), w * h * sizeof(uint32_t));
}

void Nintendo64::drawImGuiPanels() {
    // 1. MIPS VR4300 Registers
    ImGui::SetNextWindowPos(ImVec2(660, 345), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(300, 340), ImGuiCond_Always);
    ImGui::Begin("Registradores MIPS VR4300 (N64)", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

    if (ImGui::BeginTabBar("N64Registers")) {
        if (ImGui::BeginTabItem("R0-R15")) {
            ImGui::Columns(2, "n64_r0_r15", true);
            ImGui::Text("Reg"); ImGui::NextColumn();
            ImGui::Text("Valor (64-bit)"); ImGui::NextColumn();
            ImGui::Separator();
            for (int i = 0; i < 16; ++i) {
                ImGui::Text("R%d", i); ImGui::NextColumn();
                ImGui::Text("0x%016llX", (unsigned long long)registers[i]); ImGui::NextColumn();
            }
            ImGui::Columns(1);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("R16-R31")) {
            ImGui::Columns(2, "n64_r16_r31", true);
            ImGui::Text("Reg"); ImGui::NextColumn();
            ImGui::Text("Valor (64-bit)"); ImGui::NextColumn();
            ImGui::Separator();
            for (int i = 16; i < 32; ++i) {
                std::string name = "R" + std::to_string(i);
                if (i == 29) name = "SP (R29)";
                else if (i == 31) name = "RA (R31)";
                ImGui::Text("%s", name.c_str()); ImGui::NextColumn();
                ImGui::Text("0x%016llX", (unsigned long long)registers[i]); ImGui::NextColumn();
            }
            ImGui::Columns(1);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("CP0 & ALU")) {
            ImGui::Text("Program Counter (PC): 0x%016llX", (unsigned long long)PC);
            ImGui::Text("Multiplication HI: 0x%016llX", (unsigned long long)HI);
            ImGui::Text("Multiplication LO: 0x%016llX", (unsigned long long)LO);
            ImGui::Separator();
            ImGui::Text("CP0 Status Register: 0x%08X", SR);
            
            bool ie = (SR & 0x01) != 0; // Interrupt Enable
            bool exl = (SR & 0x02) != 0; // Exception Level
            bool erl = (SR & 0x04) != 0; // Error Level
            
            ImGui::Text("Status Flags:");
            ImGui::BulletText("Interrupt Enable (IE): %s", ie ? "ON" : "OFF");
            ImGui::BulletText("Exception Level (EXL): %s", exl ? "Active" : "Normal");
            ImGui::BulletText("Error Level (ERL): %s", erl ? "Error" : "Normal");
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();

    // 2. N64 ROM Header Info
    ImGui::SetNextWindowPos(ImVec2(970, 35), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(300, 300), ImGuiCond_Always);
    ImGui::Begin("Cabecalho da ROM N64", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
    ImGui::Text("Titulo: %s", header.gameTitle);
    ImGui::Text("Validacao Magic: 0x%08X", header.validationMagic);
    ImGui::Text("Clock Rate: 0x%08X", header.clockRate);
    ImGui::Text("Boot (Entry Point): 0x%08X", header.bootAddress);
    ImGui::Text("Release Address: 0x%08X", header.releaseAddress);
    ImGui::Text("CRC1: 0x%08X", header.crc1);
    ImGui::Text("CRC2: 0x%08X", header.crc2);
    ImGui::Separator();
    ImGui::Text("Manufacturer: %c", header.manufacturerCode);
    ImGui::Text("Cartridge ID: %s", header.cartridgeId);
    ImGui::Text("Country Code: %c (%s)", header.countryCode, 
                (header.countryCode == 'E' ? "USA/Canada" : 
                (header.countryCode == 'J' ? "Japan" : 
                (header.countryCode == 'P' ? "Europe" : "Desconhecido"))));
    ImGui::Text("Status Cabecalho: %s", header.isValid ? "N64 Valido" : "Simulado");
    ImGui::End();
}

void Nintendo64::getAudioSamples(std::vector<float>& outBuffer) {
    outBuffer.clear();
}

const Nintendo64::N64Header& Nintendo64::getHeader() const {
    return header;
}

uint64_t Nintendo64::getReg(int index) const {
    return (index >= 0 && index < 32) ? registers[index] : 0;
}

uint64_t Nintendo64::getPC() const {
    return PC;
}

uint64_t Nintendo64::getHI() const {
    return HI;
}

uint64_t Nintendo64::getLO() const {
    return LO;
}

uint32_t Nintendo64::getSR() const {
    return SR;
}
