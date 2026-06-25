#include "GameBoy.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <cmath>
#include <imgui.h>
#include <SDL.h>

GameBoy::GameBoy() 
    : romFileName("Nenhum"), running(false), A(0), F(0), B(0), C(0), D(0), E(0), H(0), L(0), SP(0), PC(0) {
    header.isValid = false;
    std::memset(&header, 0, sizeof(header));
    
    // Graphic state
    pulseTime = 0.0f;
    ballX = 80;
    ballY = 72;
    ballDX = 2;
    ballDY = 1;
    
    reset();
}

std::string GameBoy::getName() const {
    return "Game Boy";
}

bool GameBoy::loadROM(const std::string& romPath) {
    std::ifstream file(romPath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return false;

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    romData.resize(size);
    if (!file.read(reinterpret_cast<char*>(romData.data()), size)) {
        romData.clear();
        return false;
    }

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

void GameBoy::parseHeader() {
    if (romData.size() < 0x150) {
        header.isValid = false;
        std::strncpy(header.title, "SIMULATION GB", sizeof(header.title) - 1);
        header.cgbFlag = 0x00;
        header.cartType = 0x00;
        header.romSizeCode = 0x00;
        header.ramSizeCode = 0x00;
        header.checksum = 0;
        return;
    }

    // Game Title (0x134 - 0x143)
    for (int i = 0; i < 16; ++i) {
        char c = static_cast<char>(romData[0x134 + i]);
        header.title[i] = (c >= 32 && c <= 126) ? c : '\0';
    }
    header.title[16] = '\0';

    header.cgbFlag = romData[0x143];
    header.cartType = romData[0x147];
    header.romSizeCode = romData[0x148];
    header.ramSizeCode = romData[0x149];
    header.checksum = romData[0x14D];

    // Compute checksum to validate GB header
    uint8_t calculatedChecksum = 0;
    for (uint16_t i = 0x0134; i <= 0x014C; ++i) calculatedChecksum = calculatedChecksum - romData[i] - 1;

    header.isValid = (calculatedChecksum == header.checksum);

    if (!header.isValid) std::strncpy(header.title, "DUMMY GB", sizeof(header.title) - 1); // Mock fallback if validation fails
}

void GameBoy::reset() {
    // Official Game Boy DMG-01 boot values for registers
    A = 0x01;
    F = 0xB0; // Flags: Z=1, N=0, H=1, C=1
    B = 0x00;
    C = 0x13;
    D = 0x00;
    E = 0xD8;
    H = 0x01;
    L = 0x4D;
    SP = 0xFFFE;
    PC = 0x0100; // ROM entry point

    wram.assign(8192, 0); // 8 KB Work RAM
    vram.assign(8192, 0); // 8 KB Video RAM
    hram.assign(128, 0);  // 127 bytes High RAM
    oam.assign(160, 0);   // 160 bytes Sprite Attribute Memory
    extram.assign(32768, 0); // Cartridge external RAM (32KB max)
    
    std::memset(io, 0, sizeof(io));
    ie = 0;

    romBank = 1;
    ramBank = 0;
    ramEnabled = false;

    IME = false;
    haltMode = false;
    pendingEI = false;

    scanlineCounter = 0;
    divCounter = 0;
    timaCounter = 0;

    frameBuffer.assign(160 * 144, 0xFFCADC0F); // Light Green DMG

    io[0x00] = 0xFF; // Joypad IO register default
    io[0x40] = 0x91; // LCDC default (LCD enable, BG enable)

    running = false;
}

void GameBoy::step() {
    if (romData.empty()) {
        // Keep screen animated even without ROM in simulation mode
        pulseTime += 0.05f;
        
        // Move bouncing ball
        ballX += ballDX;
        ballY += ballDY;
        if (ballX <= 5 || ballX >= 155) ballDX = -ballDX;
        if (ballY <= 5 || ballY >= 135) ballDY = -ballDY;
        generateAudio(738);
        return;
    }

    running = true;
    
    // Update keyboard input joypad register
    updateJoypad();

    // Execute 70224 cycles (corresponds to one Game Boy frame)
    int cyclesThisFrame = 0;
    const int targetCycles = 70224;

    while (cyclesThisFrame < targetCycles) {
        int cycles = executeNextInstructionAndReturnCycles();
        cyclesThisFrame += cycles;
        updateTimers(cycles);
        updateGraphics(cycles);
        handleInterrupts();
    }
    
    generateAudio(738);
}

void GameBoy::executeNextInstruction() {
    executeNextInstructionAndReturnCycles();
}

void GameBoy::stop() {
    running = false;
}

bool GameBoy::isRunning() const {
    return running;
}

int GameBoy::getScreenWidth() const {
    return 160;
}

int GameBoy::getScreenHeight() const {
    return 144;
}

void GameBoy::drawLCD(uint32_t* pixelBuffer) {
    int w = getScreenWidth();
    int h = getScreenHeight();
    if (romData.empty()) {
        // Flat dark background (zinc-900) instead of psychedelic test render
        for (int i = 0; i < w * h; ++i) {
            pixelBuffer[i] = 0xFF18181B;
        }
    } else {
        // Copy the real emulated framebuffer
        std::memcpy(pixelBuffer, frameBuffer.data(), w * h * sizeof(uint32_t));
    }
}

void GameBoy::drawImGuiPanels() {
    ImGui::SetNextWindowPos(ImVec2(660, 345), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(300, 340), ImGuiCond_Always);
    ImGui::Begin("Registradores Sharp LR35902 (8-bit)", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
    
    // Grid of registers
    ImGui::Columns(2, "registers_columns", true);
    ImGui::Text("Registrador"); ImGui::NextColumn();
    ImGui::Text("Valor (Hex)"); ImGui::NextColumn();
    ImGui::Separator();

    ImGui::Text("Acumulador (A)"); ImGui::NextColumn(); ImGui::Text("0x%02X", A); ImGui::NextColumn();
    ImGui::Text("Flags (F)"); ImGui::NextColumn(); ImGui::Text("0x%02X", F); ImGui::NextColumn();
    ImGui::Text("Registrador B"); ImGui::NextColumn(); ImGui::Text("0x%02X", B); ImGui::NextColumn();
    ImGui::Text("Registrador C"); ImGui::NextColumn(); ImGui::Text("0x%02X", C); ImGui::NextColumn();
    ImGui::Text("Registrador D"); ImGui::NextColumn(); ImGui::Text("0x%02X", D); ImGui::NextColumn();
    ImGui::Text("Registrador E"); ImGui::NextColumn(); ImGui::Text("0x%02X", E); ImGui::NextColumn();
    ImGui::Text("Registrador H"); ImGui::NextColumn(); ImGui::Text("0x%02X", H); ImGui::NextColumn();
    ImGui::Text("Registrador L"); ImGui::NextColumn(); ImGui::Text("0x%02X", L); ImGui::NextColumn();
    
    ImGui::Separator();
    ImGui::Text("Stack Pointer (SP)"); ImGui::NextColumn(); ImGui::Text("0x%04X", SP); ImGui::NextColumn();
    ImGui::Text("Program Counter (PC)"); ImGui::NextColumn(); ImGui::Text("0x%04X", PC); ImGui::NextColumn();

    ImGui::Columns(1);
    
    ImGui::Separator();
    ImGui::Text("Bits de Flags (F):");
    
    // Zero, Subtract, Half-carry, Carry flags
    bool z = getFlagZ();
    bool n = getFlagN();
    bool h = getFlagH();
    bool c = getFlagC();

    ImGui::Checkbox("Zero (Z)", &z); ImGui::SameLine();
    ImGui::Checkbox("Subtract (N)", &n); ImGui::SameLine();
    ImGui::Checkbox("Half-Carry (H)", &h); ImGui::SameLine();
    ImGui::Checkbox("Carry (C)", &c);

    ImGui::End();

    ImGui::SetNextWindowPos(ImVec2(970, 35), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(300, 300), ImGuiCond_Always);
    ImGui::Begin("Status do Cartucho", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
    ImGui::Text("Titulo: %s", header.title);
    ImGui::Text("Valido: %s", header.isValid ? "SIM" : "NAO (Simulando)");
    ImGui::Text("CGB Flag: 0x%02X (%s)", header.cgbFlag, 
                (header.cgbFlag == 0x80 ? "Suporta CGB/DMG" : (header.cgbFlag == 0xC0 ? "Somente CGB" : "Somente DMG")));
    
    std::string cartTypeName = "ROM Only";
    if (header.cartType == 0x01) cartTypeName = "MBC1";
    else if (header.cartType == 0x02) cartTypeName = "MBC1+RAM";
    else if (header.cartType == 0x03) cartTypeName = "MBC1+RAM+BATTERY";
    else if (header.cartType == 0x13) cartTypeName = "MBC3+RAM+BATTERY";
    
    ImGui::Text("Cartucho Tipo: 0x%02X (%s)", header.cartType, cartTypeName.c_str());
    ImGui::Text("ROM Bank ativo: %d", romBank);
    ImGui::Text("Tamanho ROM: %d KB", 32 << header.romSizeCode);
    ImGui::Text("Checksum: 0x%02X", header.checksum);
    ImGui::End();
}

std::string GameBoy::getStatusString() const {
    std::stringstream ss;
    ss << "=== STATUS GAME BOY ===\n";
    ss << "ROM: " << romFileName << " (" << (romData.size() / 1024.0) << " KB)\n";
    ss << "Game Title: " << header.title << "\n";
    ss << "PC: 0x" << std::hex << std::setw(4) << std::setfill('0') << PC << "\n";
    ss << "SP: 0x" << std::hex << std::setw(4) << std::setfill('0') << SP << "\n";
    ss << "A: 0x" << std::hex << std::setw(2) << std::setfill('0') << (int)A << "\n";
    return ss.str();
}

uint8_t GameBoy::getReg(char r) const {
    switch (r) {
        case 'A': return A;
        case 'F': return F;
        case 'B': return B;
        case 'C': return C;
        case 'D': return D;
        case 'E': return E;
        case 'H': return H;
        case 'L': return L;
        default: return 0;
    }
}

uint16_t GameBoy::getSP() const { return SP; }
uint16_t GameBoy::getPC() const { return PC; }
uint8_t GameBoy::getF() const { return F; }

// EMULATOR CPU/BUS HELPERS
uint16_t GameBoy::getBC() const { return (B << 8) | C; }
uint16_t GameBoy::getDE() const { return (D << 8) | E; }
uint16_t GameBoy::getHL() const { return (H << 8) | L; }
uint16_t GameBoy::getAF() const { return (A << 8) | F; }

void GameBoy::setBC(uint16_t val) { B = val >> 8; C = val & 0xFF; }
void GameBoy::setDE(uint16_t val) { D = val >> 8; E = val & 0xFF; }
void GameBoy::setHL(uint16_t val) { H = val >> 8; L = val & 0xFF; }
void GameBoy::setAF(uint16_t val) { A = val >> 8; F = val & 0xF0; }

uint8_t GameBoy::getRegVal(int code) const {
    switch (code) {
        case 0: return B;
        case 1: return C;
        case 2: return D;
        case 3: return E;
        case 4: return H;
        case 5: return L;
        case 6: return readByte((H << 8) | L);
        case 7: return A;
        default: return 0;
    }
}

void GameBoy::setRegVal(int code, uint8_t val) {
    switch (code) {
        case 0: B = val; break;
        case 1: C = val; break;
        case 2: D = val; break;
        case 3: E = val; break;
        case 4: H = val; break;
        case 5: L = val; break;
        case 6: writeByte((H << 8) | L, val); break;
        case 7: A = val; break;
    }
}

uint8_t GameBoy::readByte(uint16_t address) const {
    if (address < 0x4000) {
        if (romData.size() > address) return romData[address];
        return 0xFF;
    } else if (address < 0x8000) {
        uint32_t realAddress = (romBank * 0x4000) + (address - 0x4000);
        if (romData.size() > realAddress) return romData[realAddress];
        return 0xFF;
    } else if (address < 0xA000) {
        return vram[address - 0x8000];
    } else if (address < 0xC000) {
        if (ramEnabled && !extram.empty()) {
            uint32_t realAddress = (ramBank * 0x2000) + (address - 0xA000);
            if (extram.size() > realAddress) return extram[realAddress];
        }
        return 0xFF;
    } else if (address < 0xE000) {
        return wram[address - 0xC000];
    } else if (address < 0xFE00) {
        return wram[address - 0xE000];
    } else if (address < 0xFEA0) {
        return oam[address - 0xFE00];
    } else if (address < 0xFF00) {
        return 0x00;
    } else if (address < 0xFF80) {
        return io[address - 0xFF00];
    } else if (address < 0xFFFF) {
        return hram[address - 0xFF80];
    } else {
        return ie;
    }
}

void GameBoy::writeByte(uint16_t address, uint8_t value) {
    if (address < 0x2000) {
        ramEnabled = ((value & 0x0F) == 0x0A);
    } else if (address < 0x4000) {
        uint8_t bank = value & 0x1F;
        if (bank == 0) bank = 1;
        romBank = (romBank & 0xE0) | bank;
    } else if (address < 0x6000) {
        ramBank = value & 0x03;
    } else if (address < 0x8000) {
        // Mode register select
    } else if (address < 0xA000) {
        vram[address - 0x8000] = value;
    } else if (address < 0xC000) {
        if (ramEnabled && !extram.empty()) {
            uint32_t realAddress = (ramBank * 0x2000) + (address - 0xA000);
            if (extram.size() > realAddress) extram[realAddress] = value;
        }
    } else if (address < 0xE000) {
        wram[address - 0xC000] = value;
    } else if (address < 0xFE00) {
        wram[address - 0xE000] = value;
    } else if (address < 0xFEA0) {
        oam[address - 0xFE00] = value;
    } else if (address < 0xFF00) {
        // Unused
    } else if (address < 0xFF80) {
        if (address == 0xFF04) {
            io[0x04] = 0;
            divCounter = 0;
        }
        else if (address == 0xFF46) {
            uint16_t sourceAddress = value * 0x100;
            for (int i = 0; i < 160; ++i) oam[i] = readByte(sourceAddress + i);
            io[0x46] = value;
        } else {
            io[address - 0xFF00] = value;
        }
    } else if (address < 0xFFFF) {
        hram[address - 0xFF80] = value;
    } else {
        ie = value;
    }
}

void GameBoy::pushByte(uint8_t val) {
    SP--;
    writeByte(SP, val);
}

void GameBoy::pushWord(uint16_t val) {
    pushByte(val >> 8);
    pushByte(val & 0xFF);
}

uint8_t GameBoy::popByte() {
    uint8_t val = readByte(SP);
    SP++;
    return val;
}

uint16_t GameBoy::popWord() {
    uint16_t low = popByte();
    uint16_t high = popByte();
    return (high << 8) | low;
}

uint8_t GameBoy::fetchByte() {
    uint8_t val = readByte(PC);
    PC++;
    return val;
}

uint16_t GameBoy::fetchWord() {
    uint16_t val = readByte(PC) | (readByte(PC + 1) << 8);
    PC += 2;
    return val;
}

void GameBoy::requestInterrupt(int id) {
    uint8_t interruptFlag = readByte(0xFF0F);
    interruptFlag |= (1 << id);
    writeByte(0xFF0F, interruptFlag);
}

void GameBoy::handleInterrupts() {
    if (haltMode) {
        uint8_t iff = readByte(0xFF0F);
        uint8_t ie_reg = readByte(0xFFFF);
        if ((iff & ie_reg) != 0) haltMode = false;
    }

    if (!IME) return;

    uint8_t iff = readByte(0xFF0F);
    uint8_t ie_reg = readByte(0xFFFF);
    uint8_t pending = iff & ie_reg;

    if (pending == 0) return;

    for (int i = 0; i < 5; ++i) {
        if (pending & (1 << i)) {
            IME = false;
            haltMode = false;
            
            iff &= ~(1 << i);
            writeByte(0xFF0F, iff);
            
            pushWord(PC);
            
            switch (i) {
                case 0: PC = 0x0040; break; // VBlank
                case 1: PC = 0x0048; break; // LCD STAT
                case 2: PC = 0x0050; break; // Timer
                case 3: PC = 0x0058; break; // Serial
                case 4: PC = 0x0060; break; // Joypad
            }
            break;
        }
    }
}

void GameBoy::updateTimers(int cycles) {
    divCounter += cycles;
    if (divCounter >= 256) {
        divCounter -= 256;
        io[0x04]++;
    }

    uint8_t tac = readByte(0xFF07);
    if (tac & 0x04) {
        timaCounter += cycles;
        int threshold = 1024;
        switch (tac & 0x03) {
            case 0: threshold = 1024; break;
            case 1: threshold = 16; break;
            case 2: threshold = 64; break;
            case 3: threshold = 256; break;
        }
        while (timaCounter >= threshold) {
            timaCounter -= threshold;
            uint8_t tima = readByte(0xFF05);
            if (tima == 255) {
                writeByte(0xFF05, readByte(0xFF06));
                requestInterrupt(2);
            } else {
                writeByte(0xFF05, tima + 1);
            }
        }
    }
}

void GameBoy::updateGraphics(int cycles) {
    scanlineCounter += cycles;

    if (scanlineCounter >= 456) {
        scanlineCounter -= 456;
        uint8_t ly = readByte(0xFF44);
        ly++;
        
        if (ly == 144) {
            requestInterrupt(0); // VBlank
        } else if (ly > 153) {
            ly = 0;
        }
        
        writeByte(0xFF44, ly);
        
        if (ly < 144) renderScanline(ly);
    }
}

void GameBoy::renderScanline(uint8_t ly) {
    uint8_t lcdc = readByte(0xFF40);
    
    if (!(lcdc & 0x01)) {
        for (int x = 0; x < 160; ++x) frameBuffer[ly * 160 + x] = 0xFFCADC0F;
        return;
    }

    uint8_t scy = readByte(0xFF42);
    uint8_t scx = readByte(0xFF43);
    uint8_t bgp = readByte(0xFF47);

    uint16_t tileMapBase = (lcdc & 0x08) ? 0x9C00 : 0x9800;
    uint16_t tileDataBase = (lcdc & 0x10) ? 0x8000 : 0x9000;
    bool unsignedAddressing = (lcdc & 0x10) != 0;

    uint8_t bgY = (ly + scy) & 255;
    uint8_t tileRow = bgY / 8;
    uint8_t pixelRow = bgY % 8;

    const uint32_t greenPalette[] = {
        0xFFCADC0F, 0xFF8BAC0F, 0xFF306230, 0xFF0F380F
    };

    for (int x = 0; x < 160; ++x) {
        uint8_t bgX = (x + scx) & 255;
        uint8_t tileCol = bgX / 8;
        uint8_t pixelCol = bgX % 8;

        uint16_t tileMapAddress = tileMapBase + (tileRow * 32) + tileCol;
        uint8_t tileIndex = readByte(tileMapAddress);

        uint16_t tileDataAddress;
        if (unsignedAddressing) {
            tileDataAddress = tileDataBase + (tileIndex * 16);
        } else {
            tileDataAddress = tileDataBase + (static_cast<int8_t>(tileIndex) * 16);
        }

        uint16_t lineAddress = tileDataAddress + (pixelRow * 2);
        uint8_t byte1 = readByte(lineAddress);
        uint8_t byte2 = readByte(lineAddress + 1);

        int bitShift = 7 - pixelCol;
        uint8_t bit1 = (byte1 >> bitShift) & 0x01;
        uint8_t bit2 = (byte2 >> bitShift) & 0x01;
        uint8_t colorIndex = (bit2 << 1) | bit1;

        uint8_t shade = (bgp >> (colorIndex * 2)) & 0x03;
        frameBuffer[ly * 160 + x] = greenPalette[shade];
    }

    if (lcdc & 0x20) {
        uint8_t wy = readByte(0xFF4A);
        uint8_t wx = readByte(0xFF4B);

        if (ly >= wy && wx < 167) {
            int windowXStart = wx - 7;
            if (windowXStart < 0) windowXStart = 0;

            uint16_t winTileMapBase = (lcdc & 0x40) ? 0x9C00 : 0x9800;
            uint8_t winY = ly - wy;
            uint8_t winTileRow = winY / 8;
            uint8_t winPixelRow = winY % 8;

            for (int x = windowXStart; x < 160; ++x) {
                int winX = x - windowXStart;
                uint8_t winTileCol = winX / 8;
                uint8_t winPixelCol = winX % 8;

                uint16_t tileMapAddress = winTileMapBase + (winTileRow * 32) + winTileCol;
                uint8_t tileIndex = readByte(tileMapAddress);

                uint16_t tileDataAddress;
                if (unsignedAddressing) {
                    tileDataAddress = tileDataBase + (tileIndex * 16);
                } else {
                    tileDataAddress = tileDataBase + (static_cast<int8_t>(tileIndex) * 16);
                }

                uint16_t lineAddress = tileDataAddress + (winPixelRow * 2);
                uint8_t byte1 = readByte(lineAddress);
                uint8_t byte2 = readByte(lineAddress + 1);

                int bitShift = 7 - winPixelCol;
                uint8_t bit1 = (byte1 >> bitShift) & 0x01;
                uint8_t bit2 = (byte2 >> bitShift) & 0x01;
                uint8_t colorIndex = (bit2 << 1) | bit1;

                uint8_t shade = (bgp >> (colorIndex * 2)) & 0x03;
                frameBuffer[ly * 160 + x] = greenPalette[shade];
            }
        }
    }

    if (lcdc & 0x02) {
        int spriteHeight = (lcdc & 0x04) ? 16 : 8;
        uint8_t obp0 = readByte(0xFF48);
        uint8_t obp1 = readByte(0xFF49);

        for (int i = 39; i >= 0; --i) {
            uint16_t oamEntryAddr = 0xFE00 + (i * 4);
            int spriteY = readByte(oamEntryAddr) - 16;
            int spriteX = readByte(oamEntryAddr + 1) - 8;
            uint8_t tileIndex = readByte(oamEntryAddr + 2);
            uint8_t attributes = readByte(oamEntryAddr + 3);

            if (ly >= spriteY && ly < (spriteY + spriteHeight)) {
                int row = ly - spriteY;
                if (attributes & 0x40) row = spriteHeight - 1 - row;
                if (spriteHeight == 16) tileIndex &= 0xFE;

                uint16_t tileDataAddress = 0x8000 + (tileIndex * 16);
                uint16_t lineAddress = tileDataAddress + (row * 2);
                uint8_t byte1 = readByte(lineAddress);
                uint8_t byte2 = readByte(lineAddress + 1);

                uint8_t palette = (attributes & 0x10) ? obp1 : obp0;

                for (int col = 0; col < 8; ++col) {
                    int screenX = spriteX + col;
                    if (screenX >= 0 && screenX < 160) {
                        int pixelCol = 7 - col;
                        if (attributes & 0x20) pixelCol = col;

                        uint8_t bit1 = (byte1 >> pixelCol) & 0x01;
                        uint8_t bit2 = (byte2 >> pixelCol) & 0x01;
                        uint8_t colorIndex = (bit2 << 1) | bit1;

                        if (colorIndex != 0) {
                            bool bgOverSprite = (attributes & 0x80) != 0;
                            if (bgOverSprite && (frameBuffer[ly * 160 + screenX] != greenPalette[0])) continue;

                            uint8_t shade = (palette >> (colorIndex * 2)) & 0x03;
                            frameBuffer[ly * 160 + screenX] = greenPalette[shade];
                        }
                    }
                }
            }
        }
    }
}

void GameBoy::updateJoypad() {
    const uint8_t* state = SDL_GetKeyboardState(nullptr);
    
    uint8_t joypadSelect = io[0x00];
    uint8_t directions = 0x0F;
    uint8_t actions = 0x0F;
    
    if (state) {
        if (state[SDL_SCANCODE_RIGHT]) directions &= ~0x01;
        if (state[SDL_SCANCODE_LEFT]) directions &= ~0x02;
        if (state[SDL_SCANCODE_UP]) directions &= ~0x04;
        if (state[SDL_SCANCODE_DOWN]) directions &= ~0x08;
        if (state[SDL_SCANCODE_Z]) actions &= ~0x01; // A
        if (state[SDL_SCANCODE_X]) actions &= ~0x02; // B
        if (state[SDL_SCANCODE_SPACE]) actions &= ~0x04; // Select
        if (state[SDL_SCANCODE_RETURN]) actions &= ~0x08; // Start
    }
    
    uint8_t result = 0xCF;
    
    if ((joypadSelect & 0x10) == 0) {
        result &= ~0x10;
        result |= directions;
        if (directions != 0x0F) requestInterrupt(4);
    }
    
    if ((joypadSelect & 0x20) == 0) {
        result &= ~0x20;
        result |= actions;
        if (actions != 0x0F) requestInterrupt(4);
    }
    
    io[0x00] = result;
}

// ARITHMETIC HELPERS
uint8_t GameBoy::inc8(uint8_t val) {
    uint8_t result = val + 1;
    setFlagZ(result == 0);
    setFlagN(false);
    setFlagH((val & 0x0F) == 0x0F);
    return result;
}

uint8_t GameBoy::dec8(uint8_t val) {
    uint8_t result = val - 1;
    setFlagZ(result == 0);
    setFlagN(true);
    setFlagH((val & 0x0F) == 0x00);
    return result;
}

void GameBoy::addHL(uint16_t val) {
    uint16_t hl = getHL();
    uint32_t result = hl + val;
    setFlagN(false);
    setFlagH(((hl & 0x0FFF) + (val & 0x0FFF)) > 0x0FFF);
    setFlagC(result > 0xFFFF);
    setHL(result & 0xFFFF);
}

uint8_t GameBoy::add8(uint8_t a, uint8_t b) {
    uint16_t result = a + b;
    setFlagZ((result & 0xFF) == 0);
    setFlagN(false);
    setFlagH(((a & 0x0F) + (b & 0x0F)) > 0x0F);
    setFlagC(result > 0xFF);
    return result & 0xFF;
}

uint8_t GameBoy::adc8(uint8_t a, uint8_t b) {
    uint8_t carry = getFlagC() ? 1 : 0;
    uint16_t result = a + b + carry;
    setFlagZ((result & 0xFF) == 0);
    setFlagN(false);
    setFlagH(((a & 0x0F) + (b & 0x0F) + carry) > 0x0F);
    setFlagC(result > 0xFF);
    return result & 0xFF;
}

uint8_t GameBoy::sub8(uint8_t a, uint8_t b) {
    int result = a - b;
    setFlagZ((result & 0xFF) == 0);
    setFlagN(true);
    setFlagH(((a & 0x0F) - (b & 0x0F)) < 0);
    setFlagC(result < 0);
    return result & 0xFF;
}

uint8_t GameBoy::sbc8(uint8_t a, uint8_t b) {
    uint8_t carry = getFlagC() ? 1 : 0;
    int result = a - b - carry;
    setFlagZ((result & 0xFF) == 0);
    setFlagN(true);
    setFlagH(((a & 0x0F) - (b & 0x0F) - carry) < 0);
    setFlagC(result < 0);
    return result & 0xFF;
}

uint8_t GameBoy::and8(uint8_t a, uint8_t b) {
    uint8_t result = a & b;
    setFlagZ(result == 0);
    setFlagN(false);
    setFlagH(true);
    setFlagC(false);
    return result;
}

uint8_t GameBoy::or8(uint8_t a, uint8_t b) {
    uint8_t result = a | b;
    setFlagZ(result == 0);
    setFlagN(false);
    setFlagH(false);
    setFlagC(false);
    return result;
}

uint8_t GameBoy::xor8(uint8_t a, uint8_t b) {
    uint8_t result = a ^ b;
    setFlagZ(result == 0);
    setFlagN(false);
    setFlagH(false);
    setFlagC(false);
    return result;
}

void GameBoy::cp8(uint8_t a, uint8_t b) {
    sub8(a, b);
}

void GameBoy::daa() {
    int val = A;
    if (!getFlagN()) {
        if (getFlagH() || (val & 0x0F) > 9) val += 6;
        if (getFlagC() || val > 0x9F) {
            val += 0x60;
            setFlagC(true);
        }
    } else {
        if (getFlagH()) val = (val - 6) & 0xFF;
        if (getFlagC()) val -= 0x60;
    }
    A = static_cast<uint8_t>(val);
    setFlagZ(A == 0);
    setFlagH(false);
}

// FLAG HELPERS
void GameBoy::setFlagZ(bool value) {
    if (value) F |= 0x80;
    else F &= ~0x80;
}

void GameBoy::setFlagN(bool value) {
    if (value) F |= 0x40;
    else F &= ~0x40;
}

void GameBoy::setFlagH(bool value) {
    if (value) F |= 0x20;
    else F &= ~0x20;
}

void GameBoy::setFlagC(bool value) {
    if (value) F |= 0x10;
    else F &= ~0x10;
}

bool GameBoy::getFlagZ() const {
    return (F & 0x80) != 0;
}

bool GameBoy::getFlagN() const {
    return (F & 0x40) != 0;
}

bool GameBoy::getFlagH() const {
    return (F & 0x20) != 0;
}

bool GameBoy::getFlagC() const {
    return (F & 0x10) != 0;
}

// INSTRUCTION DECODER
int GameBoy::executeNextInstructionAndReturnCycles() {
    if (haltMode) return 4;

    uint16_t currentPC = PC;
    uint8_t opcode = fetchByte();
    int cycles = 4;

    bool enableIME = false;
    if (pendingEI) enableIME = true;

    switch (opcode) {
        case 0x00: cycles = 4; break; // NOP
        case 0x01: setBC(fetchWord()); cycles = 12; break; // LD BC, d16
        case 0x02: writeByte(getBC(), A); cycles = 8; break; // LD (BC), A
        case 0x03: setBC(getBC() + 1); cycles = 8; break; // INC BC
        case 0x04: B = inc8(B); cycles = 4; break; // INC B
        case 0x05: B = dec8(B); cycles = 4; break; // DEC B
        case 0x06: B = fetchByte(); cycles = 8; break; // LD B, d8
        case 0x07: { // RLCA
            uint8_t carry = (A & 0x80) >> 7;
            A = (A << 1) | carry;
            setFlagZ(false); setFlagN(false); setFlagH(false); setFlagC(carry != 0);
            cycles = 4; break;
        }
        case 0x08: { // LD (a16), SP
            uint16_t addr = fetchWord();
            writeByte(addr, SP & 0xFF);
            writeByte(addr + 1, SP >> 8);
            cycles = 20; break;
        }
        case 0x09: addHL(getBC()); cycles = 8; break; // ADD HL, BC
        case 0x0A: A = readByte(getBC()); cycles = 8; break; // LD A, (BC)
        case 0x0B: setBC(getBC() - 1); cycles = 8; break; // DEC BC
        case 0x0C: C = inc8(C); cycles = 4; break; // INC C
        case 0x0D: C = dec8(C); cycles = 4; break; // DEC C
        case 0x0E: C = fetchByte(); cycles = 8; break; // LD C, d8
        case 0x0F: { // RRCA
            uint8_t carry = A & 0x01;
            A = (A >> 1) | (carry << 7);
            setFlagZ(false); setFlagN(false); setFlagH(false); setFlagC(carry != 0);
            cycles = 4; break;
        }
        
        case 0x10: cycles = 4; break; // STOP
        case 0x11: setDE(fetchWord()); cycles = 12; break; // LD DE, d16
        case 0x12: writeByte(getDE(), A); cycles = 8; break; // LD (DE), A
        case 0x13: setDE(getDE() + 1); cycles = 8; break; // INC DE
        case 0x14: D = inc8(D); cycles = 4; break; // INC D
        case 0x15: D = dec8(D); cycles = 4; break; // DEC D
        case 0x16: D = fetchByte(); cycles = 8; break; // LD D, d8
        case 0x17: { // RLA
            uint8_t oldCarry = getFlagC() ? 1 : 0;
            uint8_t newCarry = (A & 0x80) >> 7;
            A = (A << 1) | oldCarry;
            setFlagZ(false); setFlagN(false); setFlagH(false); setFlagC(newCarry != 0);
            cycles = 4; break;
        }
        case 0x18: { // JR r8
            int8_t offset = static_cast<int8_t>(fetchByte());
            PC += offset;
            cycles = 12; break;
        }
        case 0x19: addHL(getDE()); cycles = 8; break; // ADD HL, DE
        case 0x1A: A = readByte(getDE()); cycles = 8; break; // LD A, (DE)
        case 0x1B: setDE(getDE() - 1); cycles = 8; break; // DEC DE
        case 0x1C: E = inc8(E); cycles = 4; break; // INC E
        case 0x1D: E = dec8(E); cycles = 4; break; // DEC E
        case 0x1E: E = fetchByte(); cycles = 8; break; // LD E, d8
        case 0x1F: { // RRA
            uint8_t oldCarry = getFlagC() ? 1 : 0;
            uint8_t newCarry = A & 0x01;
            A = (A >> 1) | (oldCarry << 7);
            setFlagZ(false); setFlagN(false); setFlagH(false); setFlagC(newCarry != 0);
            cycles = 4; break;
        }
        case 0x20: { // JR NZ, r8
            int8_t offset = static_cast<int8_t>(fetchByte());
            if (!getFlagZ()) { PC += offset; cycles = 12; } else { cycles = 8; }
            break;
        }
        case 0x21: setHL(fetchWord()); cycles = 12; break; // LD HL, d16
        case 0x22: { // LD (HL+), A
            uint16_t hl = getHL();
            writeByte(hl, A);
            setHL(hl + 1);
            cycles = 8; break;
        }
        case 0x23: setHL(getHL() + 1); cycles = 8; break; // INC HL
        case 0x24: H = inc8(H); cycles = 4; break; // INC H
        case 0x25: H = dec8(H); cycles = 4; break; // DEC H
        case 0x26: H = fetchByte(); cycles = 8; break; // LD H, d8
        case 0x27: daa(); cycles = 4; break; // DAA
        case 0x28: { // JR Z, r8
            int8_t offset = static_cast<int8_t>(fetchByte());
            if (getFlagZ()) { PC += offset; cycles = 12; } else { cycles = 8; }
            break;
        }
        case 0x29: addHL(getHL()); cycles = 8; break; // ADD HL, HL
        case 0x2A: { // LD A, (HL+)
            uint16_t hl = getHL();
            A = readByte(hl);
            setHL(hl + 1);
            cycles = 8; break;
        }
        case 0x2B: setHL(getHL() - 1); cycles = 8; break; // DEC HL
        case 0x2C: L = inc8(L); cycles = 4; break; // INC L
        case 0x2D: L = dec8(L); cycles = 4; break; // DEC L
        case 0x2E: L = fetchByte(); cycles = 8; break; // LD L, d8
        case 0x2F: A = ~A; setFlagN(true); setFlagH(true); cycles = 4; break; // CPL
        
        case 0x30: { // JR NC, r8
            int8_t offset = static_cast<int8_t>(fetchByte());
            if (!getFlagC()) { PC += offset; cycles = 12; } else { cycles = 8; }
            break;
        }
        case 0x31: SP = fetchWord(); cycles = 12; break; // LD SP, d16
        case 0x32: { // LD (HL-), A
            uint16_t hl = getHL();
            writeByte(hl, A);
            setHL(hl - 1);
            cycles = 8; break;
        }
        case 0x33: SP++; cycles = 8; break; // INC SP
        case 0x34: { // INC (HL)
            uint16_t hl = getHL();
            writeByte(hl, inc8(readByte(hl)));
            cycles = 12; break;
        }
        case 0x35: { // DEC (HL)
            uint16_t hl = getHL();
            writeByte(hl, dec8(readByte(hl)));
            cycles = 12; break;
        }
        case 0x36: writeByte(getHL(), fetchByte()); cycles = 12; break; // LD (HL), d8
        case 0x37: setFlagN(false); setFlagH(false); setFlagC(true); cycles = 4; break; // SCF
        case 0x38: { // JR C, r8
            int8_t offset = static_cast<int8_t>(fetchByte());
            if (getFlagC()) { PC += offset; cycles = 12; } else { cycles = 8; }
            break;
        }
        case 0x39: addHL(SP); cycles = 8; break; // ADD HL, SP
        case 0x3A: { // LD A, (HL-)
            uint16_t hl = getHL();
            A = readByte(hl);
            setHL(hl - 1);
            cycles = 8; break;
        }
        case 0x3B: SP--; cycles = 8; break; // DEC SP
        case 0x3C: A = inc8(A); cycles = 4; break; // INC A
        case 0x3D: A = dec8(A); cycles = 4; break; // DEC A
        case 0x3E: A = fetchByte(); cycles = 8; break; // LD A, d8
        case 0x3F: setFlagN(false); setFlagH(false); setFlagC(!getFlagC()); cycles = 4; break; // CCF
        
        // 0x40 - 0x7F: LD group
        case 0x40: case 0x41: case 0x42: case 0x43: case 0x44: case 0x45: case 0x46: case 0x47:
        case 0x48: case 0x49: case 0x4A: case 0x4B: case 0x4C: case 0x4D: case 0x4E: case 0x4F:
        case 0x50: case 0x51: case 0x52: case 0x53: case 0x54: case 0x55: case 0x56: case 0x57:
        case 0x58: case 0x59: case 0x5A: case 0x5B: case 0x5C: case 0x5D: case 0x5E: case 0x5F:
        case 0x60: case 0x61: case 0x62: case 0x63: case 0x64: case 0x65: case 0x66: case 0x67:
        case 0x68: case 0x69: case 0x6A: case 0x6B: case 0x6C: case 0x6D: case 0x6E: case 0x6F:
        case 0x70: case 0x71: case 0x72: case 0x73: case 0x74: case 0x75:            case 0x77:
        case 0x78: case 0x79: case 0x7A: case 0x7B: case 0x7C: case 0x7D: case 0x7E: case 0x7F: {
            uint8_t dest = (opcode >> 3) & 0x07;
            uint8_t src = opcode & 0x07;
            setRegVal(dest, getRegVal(src));
            cycles = (dest == 6 || src == 6) ? 8 : 4;
            break;
        }
        case 0x76: { // HALT
            haltMode = true;
            cycles = 4; break;
        }
        
        // 0x80 - 0xBF: ALU group
        case 0x80: case 0x81: case 0x82: case 0x83: case 0x84: case 0x85: case 0x86: case 0x87:
        case 0x88: case 0x89: case 0x8A: case 0x8B: case 0x8C: case 0x8D: case 0x8E: case 0x8F:
        case 0x90: case 0x91: case 0x92: case 0x93: case 0x94: case 0x95: case 0x96: case 0x97:
        case 0x98: case 0x99: case 0x9A: case 0x9B: case 0x9C: case 0x9D: case 0x9E: case 0x9F:
        case 0xA0: case 0xA1: case 0xA2: case 0xA3: case 0xA4: case 0xA5: case 0xA6: case 0xA7:
        case 0xA8: case 0xA9: case 0xAA: case 0xAB: case 0xAC: case 0xAD: case 0xAE: case 0xAF:
        case 0xB0: case 0xB1: case 0xB2: case 0xB3: case 0xB4: case 0xB5: case 0xB6: case 0xB7:
        case 0xB8: case 0xB9: case 0xBA: case 0xBB: case 0xBC: case 0xBD: case 0xBE: case 0xBF: {
            uint8_t src = opcode & 0x07;
            uint8_t val = getRegVal(src);
            cycles = (src == 6) ? 8 : 4;
            switch ((opcode >> 3) & 0x07) {
                case 0: A = add8(A, val); break;
                case 1: A = adc8(A, val); break;
                case 2: A = sub8(A, val); break;
                case 3: A = sbc8(A, val); break;
                case 4: A = and8(A, val); break;
                case 5: A = xor8(A, val); break;
                case 6: A = or8(A, val); break;
                case 7: cp8(A, val); break;
            }
            break;
        }
        case 0xC0: { // RET NZ
            if (!getFlagZ()) { PC = popWord(); cycles = 20; } else { cycles = 8; }
            break;
        }
        case 0xC1: setBC(popWord()); cycles = 12; break; // POP BC
        case 0xC2: { // JP NZ, a16
            uint16_t addr = fetchWord();
            if (!getFlagZ()) { PC = addr; cycles = 16; } else { cycles = 12; }
            break;
        }
        case 0xC3: PC = fetchWord(); cycles = 16; break; // JP a16
        case 0xC4: { // CALL NZ, a16
            uint16_t addr = fetchWord();
            if (!getFlagZ()) { pushWord(PC); PC = addr; cycles = 24; } else { cycles = 12; }
            break;
        }
        case 0xC5: pushWord(getBC()); cycles = 16; break; // PUSH BC
        case 0xC6: A = add8(A, fetchByte()); cycles = 8; break; // ADD A, d8
        case 0xC7: pushWord(PC); PC = 0x0000; cycles = 16; break; // RST 00H
        case 0xC8: { // RET Z
            if (getFlagZ()) { PC = popWord(); cycles = 20; } else { cycles = 8; }
            break;
        }
        case 0xC9: PC = popWord(); cycles = 16; break; // RET
        case 0xCA: { // JP Z, a16
            uint16_t addr = fetchWord();
            if (getFlagZ()) { PC = addr; cycles = 16; } else { cycles = 12; }
            break;
        }
        case 0xCB: cycles = executeCBOpcode(); break; // CB Prefix
        case 0xCC: { // CALL Z, a16
            uint16_t addr = fetchWord();
            if (getFlagZ()) { pushWord(PC); PC = addr; cycles = 24; } else { cycles = 12; }
            break;
        }
        case 0xCD: { // CALL a16
            uint16_t addr = fetchWord();
            pushWord(PC);
            PC = addr;
            cycles = 24; break;
        }
        case 0xCE: A = adc8(A, fetchByte()); cycles = 8; break; // ADC A, d8
        case 0xCF: pushWord(PC); PC = 0x0008; cycles = 16; break; // RST 08H
        
        case 0xD0: { // RET NC
            if (!getFlagC()) { PC = popWord(); cycles = 20; } else { cycles = 8; }
            break;
        }
        case 0xD1: setDE(popWord()); cycles = 12; break; // POP DE
        case 0xD2: { // JP NC, a16
            uint16_t addr = fetchWord();
            if (!getFlagC()) { PC = addr; cycles = 16; } else { cycles = 12; }
            break;
        }
        case 0xD4: { // CALL NC, a16
            uint16_t addr = fetchWord();
            if (!getFlagC()) { pushWord(PC); PC = addr; cycles = 24; } else { cycles = 12; }
            break;
        }
        case 0xD5: pushWord(getDE()); cycles = 16; break; // PUSH DE
        case 0xD6: A = sub8(A, fetchByte()); cycles = 8; break; // SUB d8
        case 0xD7: pushWord(PC); PC = 0x0010; cycles = 16; break; // RST 10H
        case 0xD8: { // RET C
            if (getFlagC()) { PC = popWord(); cycles = 20; } else { cycles = 8; }
            break;
        }
        case 0xD9: PC = popWord(); IME = true; cycles = 16; break; // RETI
        case 0xDA: { // JP C, a16
            uint16_t addr = fetchWord();
            if (getFlagC()) { PC = addr; cycles = 16; } else { cycles = 12; }
            break;
        }
        case 0xDC: { // CALL C, a16
            uint16_t addr = fetchWord();
            if (getFlagC()) { pushWord(PC); PC = addr; cycles = 24; } else { cycles = 12; }
            break;
        }
        case 0xDE: A = sbc8(A, fetchByte()); cycles = 8; break; // SBC A, d8
        case 0xDF: pushWord(PC); PC = 0x0018; cycles = 16; break; // RST 18H
        
        case 0xE0: writeByte(0xFF00 + fetchByte(), A); cycles = 12; break; // LDH (a8), A
        case 0xE1: setHL(popWord()); cycles = 12; break; // POP HL
        case 0xE2: writeByte(0xFF00 + C, A); cycles = 8; break; // LD (C), A
        case 0xE5: pushWord(getHL()); cycles = 16; break; // PUSH HL
        case 0xE6: A = and8(A, fetchByte()); cycles = 8; break; // AND d8
        case 0xE7: pushWord(PC); PC = 0x0020; cycles = 16; break; // RST 20H
        case 0xE8: { // ADD SP, r8
            int8_t offset = static_cast<int8_t>(fetchByte());
            uint32_t result = SP + offset;
            setFlagZ(false);
            setFlagN(false);
            setFlagH(((SP & 0x0F) + (offset & 0x0F)) > 0x0F);
            setFlagC(((SP & 0xFF) + (offset & 0xFF)) > 0xFF);
            SP = result & 0xFFFF;
            cycles = 16; break;
        }
        case 0xE9: PC = getHL(); cycles = 4; break; // JP HL
        case 0xEA: writeByte(fetchWord(), A); cycles = 16; break; // LD (a16), A
        case 0xEE: A = xor8(A, fetchByte()); cycles = 8; break; // XOR d8
        case 0xEF: pushWord(PC); PC = 0x0028; cycles = 16; break; // RST 28H
        case 0xF0: A = readByte(0xFF00 + fetchByte()); cycles = 12; break; // LDH A, (a8)
        case 0xF1: setAF(popWord()); cycles = 12; break; // POP AF
        case 0xF2: A = readByte(0xFF00 + C); cycles = 8; break; // LD A, (C)
        case 0xF3: IME = false; cycles = 4; break; // DI
        case 0xF5: pushWord(getAF()); cycles = 16; break; // PUSH AF
        case 0xF6: A = or8(A, fetchByte()); cycles = 8; break; // OR d8
        case 0xF7: pushWord(PC); PC = 0x0030; cycles = 16; break; // RST 30H
        case 0xF8: { // LD HL, SP + r8
            int8_t offset = static_cast<int8_t>(fetchByte());
            setFlagZ(false);
            setFlagN(false);
            setFlagH(((SP & 0x0F) + (offset & 0x0F)) > 0x0F);
            setFlagC(((SP & 0xFF) + (offset & 0xFF)) > 0xFF);
            setHL((SP + offset) & 0xFFFF);
            cycles = 12; break;
        }
        case 0xF9: SP = getHL(); cycles = 8; break; // LD SP, HL
        case 0xFA: A = readByte(fetchWord()); cycles = 16; break; // LD A, (a16)
        case 0xFB: pendingEI = true; cycles = 4; break; // EI
        case 0xFE: cp8(A, fetchByte()); cycles = 8; break; // CP d8
        case 0xFF: pushWord(PC); PC = 0x0038; cycles = 16; break; // RST 38H
        default:
            cycles = 4;
            break;
    }

    if (enableIME) {
        IME = true;
        pendingEI = false;
    }

    return cycles;
}

int GameBoy::executeCBOpcode() {
    uint8_t cbOpcode = fetchByte();
    uint8_t regCode = cbOpcode & 0x07;
    uint8_t val = getRegVal(regCode);
    uint8_t bit = (cbOpcode >> 3) & 0x07;
    uint8_t group = cbOpcode >> 6;
    
    int cycles = (regCode == 6) ? 16 : 8;

    if (group == 0) {
        switch (bit) {
            case 0: { // RLC
                uint8_t carry = (val & 0x80) >> 7;
                val = (val << 1) | carry;
                setFlagZ(val == 0); setFlagN(false); setFlagH(false); setFlagC(carry != 0);
                break;
            }
            case 1: { // RRC
                uint8_t carry = val & 0x01;
                val = (val >> 1) | (carry << 7);
                setFlagZ(val == 0); setFlagN(false); setFlagH(false); setFlagC(carry != 0);
                break;
            }
            case 2: { // RL
                uint8_t oldCarry = getFlagC() ? 1 : 0;
                uint8_t newCarry = (val & 0x80) >> 7;
                val = (val << 1) | oldCarry;
                setFlagZ(val == 0); setFlagN(false); setFlagH(false); setFlagC(newCarry != 0);
                break;
            }
            case 3: { // RR
                uint8_t oldCarry = getFlagC() ? 1 : 0;
                uint8_t newCarry = val & 0x01;
                val = (val >> 1) | (oldCarry << 7);
                setFlagZ(val == 0); setFlagN(false); setFlagH(false); setFlagC(newCarry != 0);
                break;
            }
            case 4: { // SLA
                uint8_t carry = (val & 0x80) >> 7;
                val = val << 1;
                setFlagZ(val == 0); setFlagN(false); setFlagH(false); setFlagC(carry != 0);
                break;
            }
            case 5: { // SRA
                uint8_t carry = val & 0x01;
                val = (static_cast<int8_t>(val) >> 1);
                setFlagZ(val == 0); setFlagN(false); setFlagH(false); setFlagC(carry != 0);
                break;
            }
            case 6: { // SWAP
                val = ((val & 0x0F) << 4) | ((val & 0xF0) >> 4);
                setFlagZ(val == 0); setFlagN(false); setFlagH(false); setFlagC(false);
                break;
            }
            case 7: { // SRL
                uint8_t carry = val & 0x01;
                val = val >> 1;
                setFlagZ(val == 0); setFlagN(false); setFlagH(false); setFlagC(carry != 0);
                break;
            }
        }
        setRegVal(regCode, val);
    } else if (group == 1) {
        setFlagZ((val & (1 << bit)) == 0);
        setFlagN(false);
        setFlagH(true);
        cycles = (regCode == 6) ? 12 : 8;
    } else if (group == 2) {
        val &= ~(1 << bit);
        setRegVal(regCode, val);
    } else if (group == 3) {
        val |= (1 << bit);
        setRegVal(regCode, val);
    }
    
    return cycles;
}

void GameBoy::getAudioSamples(std::vector<float>& outBuffer) {
    outBuffer = std::move(audioSamples);
    audioSamples.clear();
}

void GameBoy::generateAudio(int numSamples) {
    audioSamples.clear();
    
    // Check master sound switch (bit 7 of NR52 / 0xFF26)
    if (!(io[0x26] & 0x80)) {
        audioSamples.assign(numSamples, 0.0f);
        return;
    }
    
    // Channel 1 (Pulse 1) settings
    uint16_t ch1Freq = io[0x13] | ((io[0x14] & 0x07) << 8);
    float ch1Hz = ch1Freq < 2048 ? (131072.0f / (2048 - ch1Freq)) : 0.0f;
    float ch1Delta = ch1Hz / 44100.0f;
    uint8_t ch1DutyCode = io[0x11] >> 6;
    float ch1Duty = 0.5f;
    if (ch1DutyCode == 0) ch1Duty = 0.125f;
    else if (ch1DutyCode == 1) ch1Duty = 0.25f;
    else if (ch1DutyCode == 2) ch1Duty = 0.50f;
    else if (ch1DutyCode == 3) ch1Duty = 0.75f;
    float ch1Vol = (io[0x12] >> 4) / 15.0f;
    
    // Channel 2 (Pulse 2) settings
    uint16_t ch2Freq = io[0x18] | ((io[0x19] & 0x07) << 8);
    float ch2Hz = ch2Freq < 2048 ? (131072.0f / (2048 - ch2Freq)) : 0.0f;
    float ch2Delta = ch2Hz / 44100.0f;
    uint8_t ch2DutyCode = io[0x16] >> 6;
    float ch2Duty = 0.5f;
    if (ch2DutyCode == 0) ch2Duty = 0.125f;
    else if (ch2DutyCode == 1) ch2Duty = 0.25f;
    else if (ch2DutyCode == 2) ch2Duty = 0.50f;
    else if (ch2DutyCode == 3) ch2Duty = 0.75f;
    float ch2Vol = (io[0x17] >> 4) / 15.0f;
    
    // Channel 3 (Wave) settings
    bool ch3On = (io[0x1A] & 0x80) != 0;
    uint16_t ch3Freq = io[0x1D] | ((io[0x1E] & 0x07) << 8);
    float ch3Hz = ch3Freq < 2048 ? (65536.0f / (2048 - ch3Freq)) : 0.0f;
    float ch3Delta = ch3Hz / 44100.0f;
    uint8_t ch3VolCode = (io[0x1C] >> 5) & 0x03; // 0=mute, 1=100%, 2=50%, 3=25%
    float ch3VolShift = 0.0f;
    if (ch3VolCode == 1) ch3VolShift = 1.0f;
    else if (ch3VolCode == 2) ch3VolShift = 0.5f;
    else if (ch3VolCode == 3) ch3VolShift = 0.25f;
    
    // Channel 4 (Noise) settings
    float ch4Vol = (io[0x21] >> 4) / 15.0f;
    uint8_t ch4Shift = io[0x22] >> 4;
    uint8_t ch4Ratio = io[0x22] & 0x07;
    float ch4FreqVal = 524288.0f / (((ch4Ratio == 0) ? 0.5f : ch4Ratio) * (1 << (ch4Shift + 1)));
    float ch4Delta = ch4FreqVal / 44100.0f;
    bool ch4ShortStep = (io[0x22] & 0x08) != 0;

    // Master volume (mono mixing)
    float masterVolLeft = ((io[0x24] >> 4) & 0x07) / 7.0f;
    float masterVolRight = (io[0x24] & 0x07) / 7.0f;
    float masterVol = (masterVolLeft + masterVolRight) * 0.5f;

    for (int s = 0; s < numSamples; ++s) {
        // Channel 1 output
        float ch1Val = 0.0f;
        if (ch1Vol > 0.0f && (io[0x26] & 0x01)) {
            ch1Phase += ch1Delta;
            if (ch1Phase >= 1.0f) ch1Phase -= 1.0f;
            ch1Val = (ch1Phase < ch1Duty ? 1.0f : -1.0f) * ch1Vol;
        }
        
        // Channel 2 output
        float ch2Val = 0.0f;
        if (ch2Vol > 0.0f && (io[0x26] & 0x02)) {
            ch2Phase += ch2Delta;
            if (ch2Phase >= 1.0f) ch2Phase -= 1.0f;
            ch2Val = (ch2Phase < ch2Duty ? 1.0f : -1.0f) * ch2Vol;
        }
        
        // Channel 3 output (Wave RAM)
        float ch3Val = 0.0f;
        if (ch3On && ch3VolShift > 0.0f && (io[0x26] & 0x04)) {
            ch3Phase += ch3Delta;
            if (ch3Phase >= 1.0f) ch3Phase -= 1.0f;
            
            int sampleIdx = static_cast<int>(ch3Phase * 32.0f) % 32;
            uint8_t byteVal = io[0x30 + (sampleIdx / 2)];
            uint8_t sample4Bit = (sampleIdx % 2 == 0) ? (byteVal >> 4) : (byteVal & 0x0F);
            float rawSample = ((sample4Bit / 15.0f) * 2.0f) - 1.0f;
            ch3Val = rawSample * ch3VolShift;
        }
        
        // Channel 4 output (Noise)
        float ch4Val = 0.0f;
        if (ch4Vol > 0.0f && (io[0x26] & 0x08)) {
            lfsrSec += ch4Delta;
            while (lfsrSec >= 1.0f) {
                lfsrSec -= 1.0f;
                uint16_t bit0 = lfsr & 0x0001;
                uint16_t bit1 = (lfsr >> 1) & 0x0001;
                uint16_t result = bit0 ^ bit1;
                lfsr = (lfsr >> 1) | (result << 14);
                if (ch4ShortStep) lfsr = (lfsr & ~(1 << 6)) | (result << 6);
            }
            ch4Val = ((lfsr & 0x01) ? 1.0f : -1.0f) * ch4Vol;
        }
        
        // Mix all active channels
        float mixed = (ch1Val + ch2Val + ch3Val + ch4Val) * 0.25f * masterVol;
        
        // Clamp limits
        if (mixed > 1.0f) mixed = 1.0f;
        else if (mixed < -1.0f) mixed = -1.0f;
        
        audioSamples.push_back(mixed);
    }
}
