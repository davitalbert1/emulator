#include "NintendoWii.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <cstring>
#include <algorithm>
#include <cmath>
#include <imgui.h>

NintendoWii::NintendoWii() : running(false), PC(0x80003000), LR(0), CTR(0), CR(0), XER(0), MSR(0x00002000) {
    std::memset(gpr, 0, sizeof(gpr));
    gpr[1] = 0x807FFFF0; // SP (Stack Pointer GPR1 in PowerPC ABI)
    gpr[3] = 0x80000100; // Arg R3

    frameBuffer.resize(640 * 480, 0xFF18181B); // Zinc-900
    std::memset(&header, 0, sizeof(header));
    header.isValid = false;
}

std::string NintendoWii::getName() const {
    return "Nintendo Wii";
}

bool NintendoWii::loadROM(const std::string& romPath) {
    std::ifstream file(romPath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "Erro ao abrir imagem de Wii: " << romPath << "\n";
        return false;
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    romData.resize(size);
    if (!file.read(reinterpret_cast<char*>(romData.data()), size)) {
        std::cerr << "Erro ao ler dados do Wii.\n";
        return false;
    }

    size_t lastSlash = romPath.find_last_of("/\\");
    romFileName = (lastSlash == std::string::npos) ? romPath : romPath.substr(lastSlash + 1);

    parseHeader();
    running = true;
    return true;
}

void NintendoWii::parseHeader() {
    std::memset(&header, 0, sizeof(header));
    if (romData.size() >= 0x60) {
        std::memcpy(header.gameId, romData.data(), 6);
        header.gameId[6] = '\0';

        header.discNumber = romData[6];
        header.discVersion = romData[7];
        header.streamingAudio = romData[8];
        header.streamingBufferSize = romData[9];

        header.magic = (romData[0x1C] << 24) | (romData[0x1D] << 16) | (romData[0x1E] << 8) | romData[0x1F];

        std::memcpy(header.gameTitle, romData.data() + 0x20, 64);
        header.gameTitle[64] = '\0';
        header.isValid = true;
    } else {
        std::strncpy(header.gameId, "RSBP01", sizeof(header.gameId) - 1);
        header.discNumber = 1;
        header.discVersion = 0;
        header.streamingAudio = 0;
        header.streamingBufferSize = 0;
        header.magic = 0x5D1C9EA3;
        std::strncpy(header.gameTitle, "SUPER SMASH BROS. BRAWL", sizeof(header.gameTitle) - 1);
        header.isValid = false;
    }
}

void NintendoWii::step() {
    if (running) executeNextInstruction();
}

void NintendoWii::executeNextInstruction() {
    // Cycle PC and register values for PowerPC simulation
    PC += 4;
    if (PC >= 0x81800000) PC = 0x80003000;

    // Cycle registers slightly
    gpr[0] = (gpr[0] + 1) & 0xFFFFFFFF;
    gpr[3] = (gpr[3] + 3) & 0xFFFFFFFF;
    gpr[4] = (gpr[4] + 2) & 0xFFFFFFFF;
}

void NintendoWii::reset() {
    std::memset(gpr, 0, sizeof(gpr));
    gpr[1] = 0x807FFFF0;
    gpr[3] = 0x80000100;
    PC = 0x80003000;
    LR = 0;
    CTR = 0;
    CR = 0;
    XER = 0;
    MSR = 0x00002000;
    std::fill(frameBuffer.begin(), frameBuffer.end(), 0xFF18181B);
}

void NintendoWii::stop() {
    running = false;
}

bool NintendoWii::isRunning() const {
    return running;
}

std::string NintendoWii::getStatusString() const {
    std::stringstream ss;
    ss << "Nintendo Wii Simulator [Broadway PowerPC]\n";
    ss << "ROM/ISO: " << (romFileName.empty() ? "Nenhuma carregada" : romFileName) << "\n";
    if (romFileName.empty()) {
        ss << "Por favor, selecione e carregue uma ROM do Wii (*.wbfs, *.iso ou *.gcm).\n";
    } else {
        ss << "Tamanho Imagem: " << (romData.size() / 1024.0f / 1024.0f) << " MB\n";
    }
    ss << "Status: " << (running ? "Simulando" : "Pausado") << "\n";
    return ss.str();
}

int NintendoWii::getScreenWidth() const {
    return 640;
}

int NintendoWii::getScreenHeight() const {
    return 480;
}

void NintendoWii::drawLCD(uint32_t* pixelBuffer) {
    int w = getScreenWidth();
    int h = getScreenHeight();

    if (running && !romData.empty()) {
        // Draw off-white Wii background channel grid
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                if (y % 60 == 0 || x % 80 == 0) {
                    frameBuffer[y * w + x] = 0xFFEAEAEA; // Grid lines
                } else {
                    frameBuffer[y * w + x] = 0xFFF5F5F7; // Off-white
                }
            }
        }

        // Draw Wii Channel cards grid (3 columns, 2 rows)
        int colCenters[3] = { 140, 320, 500 };
        int rowCenters[2] = { 125, 275 };

        for (int r = 0; r < 2; ++r) {
            for (int c = 0; c < 3; ++c) {
                int cx = colCenters[c];
                int cy = rowCenters[r];
                for (int y = cy - 45; y < cy + 45; ++y) {
                    for (int x = cx - 70; x < cx + 70; ++x) {
                        if (std::abs(y - cy) > 42 || std::abs(x - cx) > 67) {
                            frameBuffer[y * w + x] = 0xFFC0C0C4; // Border
                        } else {
                            uint32_t contentColor = 0xFFFFFFFF;
                            if (r == 0 && c == 0) contentColor = 0xFF3B82F6; // Disc
                            else if (r == 0 && c == 1) contentColor = 0xFFEF4444; // Mii
                            else if (r == 0 && c == 2) contentColor = 0xFFEAB308; // Photo
                            else contentColor = 0xFF10B981; // Shop
                            
                            if ((x + y) % 8 < 2) {
                                frameBuffer[y * w + x] = contentColor;
                            } else {
                                frameBuffer[y * w + x] = 0xFFF0F0F2;
                            }
                        }
                    }
                }
            }
        }

        // Draw floating blue Wii Remote IR pointer
        int cursorX = 320 + (int)(150 * std::sin(PC * 0.03f));
        int cursorY = 240 + (int)(100 * std::cos(PC * 0.02f));
        for (int dy = -10; dy <= 10; ++dy) {
            for (int dx = -10; dx <= 10; ++dx) {
                int distSq = dx*dx + dy*dy;
                if (distSq < 100) {
                    int px = cursorX + dx;
                    int py = cursorY + dy;
                    if (px >= 0 && px < w && py >= 0 && py < h) {
                        if (distSq < 16 || (distSq > 64 && distSq < 81)) {
                            frameBuffer[py * w + px] = 0xFF06B6D4; // Cyan pointer
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

void NintendoWii::drawImGuiPanels() {
    // 1. IBM Broadway GPR registers
    ImGui::SetNextWindowPos(ImVec2(660, 345), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(300, 340), ImGuiCond_Always);
    ImGui::Begin("Registradores PowerPC (Wii)", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

    if (ImGui::BeginTabBar("WiiRegisters")) {
        if (ImGui::BeginTabItem("GPR0-GPR15")) {
            ImGui::Columns(2, "wii_gpr0_gpr15", true);
            ImGui::Text("Reg"); ImGui::NextColumn();
            ImGui::Text("Valor"); ImGui::NextColumn();
            ImGui::Separator();
            for (int i = 0; i < 16; ++i) {
                std::string label = "r" + std::to_string(i);
                if (i == 1) label = "sp (r1)";
                ImGui::Text("%s", label.c_str()); ImGui::NextColumn();
                ImGui::Text("0x%08X", gpr[i]); ImGui::NextColumn();
            }
            ImGui::Columns(1);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("GPR16-GPR31")) {
            ImGui::Columns(2, "wii_gpr16_gpr31", true);
            ImGui::Text("Reg"); ImGui::NextColumn();
            ImGui::Text("Valor"); ImGui::NextColumn();
            ImGui::Separator();
            for (int i = 16; i < 32; ++i) {
                ImGui::Text("r%d", i); ImGui::NextColumn();
                ImGui::Text("0x%08X", gpr[i]); ImGui::NextColumn();
            }
            ImGui::Columns(1);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Registradores SPR")) {
            ImGui::Text("Program Counter (PC): 0x%08X", PC);
            ImGui::Text("Link Register (LR): 0x%08X", LR);
            ImGui::Text("Count Register (CTR): 0x%08X", CTR);
            ImGui::Text("Condition Reg (CR): 0x%08X", CR);
            ImGui::Text("Exception Reg (XER): 0x%08X", XER);
            ImGui::Separator();
            ImGui::Text("Machine State (MSR): 0x%08X", MSR);
            
            bool pr = (MSR & 0x4000) != 0; // Privilege level (1 = user)
            bool ee = (MSR & 0x8000) != 0; // External interrupt
            
            ImGui::Text("Status Flags:");
            ImGui::BulletText("Privilege: %s", pr ? "User (Problem)" : "Supervisor");
            ImGui::BulletText("Int Enable (EE): %s", ee ? "ON" : "OFF");
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();

    // 2. Disc Header Info
    ImGui::SetNextWindowPos(ImVec2(970, 35), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(300, 300), ImGuiCond_Always);
    ImGui::Begin("Cabecalho da Imagem Wii", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
    ImGui::Text("Game ID: %s", header.gameId);
    ImGui::Text("Disco Num: %d", header.discNumber);
    ImGui::Text("Disco Versao: %d", header.discVersion);
    ImGui::Text("Audio Streaming: %s", header.streamingAudio ? "Sim" : "Nao");
    ImGui::Text("Tamanho Audio Buffer: %d", header.streamingBufferSize);
    ImGui::Text("Titulo: %s", header.gameTitle);
    ImGui::Text("Magic Code: 0x%08X (%s)", header.magic, 
                (header.magic == 0x5D1C9EA3 ? "Wii Nativo" : 
                (header.magic == 0xC12F3C1A ? "GameCube" : "Invalido")));
    ImGui::Text("Status Assinatura: %s", header.isValid ? "Valido" : "Simulado");
    ImGui::End();
}

void NintendoWii::getAudioSamples(std::vector<float>& outBuffer) {
    outBuffer.clear();
}

const NintendoWii::DiscHeader& NintendoWii::getHeader() const {
    return header;
}

uint32_t NintendoWii::getGPR(int index) const {
    return (index >= 0 && index < 32) ? gpr[index] : 0;
}

uint32_t NintendoWii::getPC() const {
    return PC;
}

uint32_t NintendoWii::getLR() const {
    return LR;
}

uint32_t NintendoWii::getCTR() const {
    return CTR;
}

uint32_t NintendoWii::getCR() const {
    return CR;
}

uint32_t NintendoWii::getMSR() const {
    return MSR;
}
