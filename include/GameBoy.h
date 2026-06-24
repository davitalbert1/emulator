#ifndef GAMEBOY_H
#define GAMEBOY_H

#include "Console.h"
#include <string>
#include <vector>
#include <cstdint>

class GameBoy : public Console {
public:
    struct GBHeader {
        char title[17];          // 16 chars + null terminator (0x134 - 0x143)
        uint8_t cgbFlag;        // 0x143 (0x80 = GB/CGB, 0xC0 = CGB only)
        uint8_t cartType;       // 0x147
        uint8_t romSizeCode;    // 0x148
        uint8_t ramSizeCode;    // 0x149
        uint8_t checksum;       // 0x14D
        bool isValid;
    };

    GameBoy();
    ~GameBoy() override = default;

    // Console interface implementation
    std::string getName() const override;
    bool loadROM(const std::string& romPath) override;
    void step() override;
    void reset() override;
    void stop() override;
    bool isRunning() const override;
    std::string getStatusString() const override;

    int getScreenWidth() const override;
    int getScreenHeight() const override;
    void drawLCD(uint32_t* pixelBuffer) override;
    void drawImGuiPanels() override;
    void getAudioSamples(std::vector<float>& outBuffer) override;

    // GB specific inspection getters
    uint8_t getReg(char r) const;
    uint16_t getSP() const;
    uint16_t getPC() const;
    uint8_t getF() const;

protected:
    std::string romFileName;
    std::vector<uint8_t> romData;
    GBHeader header;
    bool running;

    // 8-bit registers: A, F (flags), B, C, D, E, H, L
    uint8_t A, F, B, C, D, E, H, L;
    
    // 16-bit registers
    uint16_t SP;
    uint16_t PC;

    // Memory spaces
    std::vector<uint8_t> wram; // 8 KB Work RAM
    std::vector<uint8_t> vram; // 8 KB Video RAM
    std::vector<uint8_t> hram; // 127 bytes High RAM (0xFF80 - 0xFFFE)
    std::vector<uint8_t> oam;  // 160 bytes Sprite Attribute Memory (0xFE00 - 0xFE9F)
    std::vector<uint8_t> extram; // Cartridge RAM (optional)
    uint8_t io[128];           // I/O Registers (0xFF00 - 0xFF7F)
    uint8_t ie;                // Interrupt Enable Register (0xFFFF)

    // Banking variables
    uint8_t romBank;
    uint8_t ramBank;
    bool ramEnabled;

    // Interrupt variables
    bool IME;
    bool haltMode;
    bool pendingEI;

    // Clock accumulators
    int scanlineCounter;
    int divCounter;
    int timaCounter;

    // Flat 160x144 internal framebuffer for drawing
    std::vector<uint32_t> frameBuffer;

    // Graphic State (for fallback animation)
    float pulseTime;
    int ballX, ballY;
    int ballDX, ballDY;

    virtual void parseHeader();
    virtual void executeNextInstruction();

    // Helper utilities
    uint16_t getBC() const;
    uint16_t getDE() const;
    uint16_t getHL() const;
    uint16_t getAF() const;
    void setBC(uint16_t val);
    void setDE(uint16_t val);
    void setHL(uint16_t val);
    void setAF(uint16_t val);

    uint8_t getRegVal(int code) const;
    void setRegVal(int code, uint8_t val);

    uint8_t readByte(uint16_t address) const;
    void writeByte(uint16_t address, uint8_t value);
    uint16_t readWord(uint16_t address) const { return readByte(address) | (readByte(address + 1) << 8); }
    void writeWord(uint16_t address, uint16_t value) { writeByte(address, value & 0xFF); writeByte(address + 1, value >> 8); }

    // Stack utilities
    void pushByte(uint8_t val);
    void pushWord(uint16_t val);
    uint8_t popByte();
    uint16_t popWord();

    // CPU helpers
    uint8_t fetchByte();
    uint16_t fetchWord();
    int executeNextInstructionAndReturnCycles();
    int executeCBOpcode();

    // Interrupt helper
    void requestInterrupt(int id);
    void handleInterrupts();

    // Timer helper
    void updateTimers(int cycles);

    // Graphic PPU helpers
    void updateGraphics(int cycles);
    void renderScanline(uint8_t ly);

    // Joypad helper
    void updateJoypad();

    // Arithmetic helpers
    uint8_t inc8(uint8_t val);
    uint8_t dec8(uint8_t val);
    void addHL(uint16_t val);
    uint8_t add8(uint8_t a, uint8_t b);
    uint8_t adc8(uint8_t a, uint8_t b);
    uint8_t sub8(uint8_t a, uint8_t b);
    uint8_t sbc8(uint8_t a, uint8_t b);
    uint8_t and8(uint8_t a, uint8_t b);
    uint8_t or8(uint8_t a, uint8_t b);
    uint8_t xor8(uint8_t a, uint8_t b);
    void cp8(uint8_t a, uint8_t b);
    void daa();

    // Flag helpers
    void setFlagZ(bool value);
    void setFlagN(bool value);
    void setFlagH(bool value);
    void setFlagC(bool value);
    bool getFlagZ() const;
    bool getFlagN() const;
    bool getFlagH() const;
    bool getFlagC() const;

    // Audio helper and states
    void generateAudio(int numSamples);
    float ch1Phase = 0.0f;
    float ch2Phase = 0.0f;
    float ch3Phase = 0.0f;
    uint16_t lfsr = 0x7FFF;
    float lfsrSec = 0.0f;
    std::vector<float> audioSamples;
};

#endif // GAMEBOY_H
