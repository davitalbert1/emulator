#ifndef NES_H
#define NES_H

#include "Console.h"
#include <string>
#include <vector>
#include <cstdint>

class NES : public Console {
public:
    struct iNESHeader {
        char magic[4]; // Must be "NES\x1A"
        uint8_t prgRomCount; // Number of 16KB PRG ROM banks
        uint8_t chrRomCount; // Number of 8KB CHR ROM banks
        uint8_t flags6; // Mapper, mirroring, battery, trainer
        uint8_t flags7; // Mapper, VS/Playchoice, NES 2.0
        uint8_t prgRamSize; // PRG RAM size (often 8KB)
        uint8_t flags9; // TV system
        uint8_t flags10; // TV system, PRG RAM presence
        bool isValid;
        int mapperId;
    };

    NES();
    ~NES() override = default;

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

    // NES specific inspection getters
    const iNESHeader& getHeader() const;
    uint8_t getA() const;
    uint8_t getX() const;
    uint8_t getY() const;
    uint8_t getS() const;
    uint8_t getP() const;
    uint16_t getPC() const;

private:
    std::string romFileName;
    std::vector<uint8_t> romData;
    iNESHeader header;
    bool running;

    // Ricoh 2A03 Registers (6502 based)
    uint8_t A;
    uint8_t X;
    uint8_t Y;
    uint8_t S; // Stack Pointer
    uint8_t P; // Status Register (Flags: N, V, U, B, D, I, Z, C)
    uint16_t PC;

    std::vector<uint32_t> frameBuffer;

    void parseHeader();
    void executeNextInstruction();
};

#endif // NES_H
