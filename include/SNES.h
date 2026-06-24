#ifndef SNES_H
#define SNES_H

#include "Console.h"
#include <string>
#include <vector>
#include <cstdint>

class SNES : public Console {
public:
    struct SMCHeader {
        char title[22];         // 21 chars + null terminator
        uint8_t mapMode;        // Map mode (e.g. 0x20 LoROM, 0x21 HiROM)
        uint8_t cartType;       // ROM only, RAM, battery, coprocessor
        uint8_t romSizeCode;    // ROM size code
        uint8_t ramSizeCode;    // RAM size code
        uint16_t developerId;
        uint8_t romVersion;
        uint16_t checksumComplement;
        uint16_t checksum;
        bool isValid;
    };

    SNES();
    ~SNES() override = default;

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

    // SNES specific inspection getters
    const SMCHeader& getHeader() const;
    uint16_t getA() const;
    uint16_t getX() const;
    uint16_t getY() const;
    uint16_t getDP() const;
    uint16_t getSP() const;
    uint8_t getDB() const;
    uint8_t getPB() const;
    uint16_t getPC() const;
    uint8_t getSR() const;

private:
    std::string romFileName;
    std::vector<uint8_t> romData;
    SMCHeader header;
    bool running;

    // Ricoh 5A22 (65C816 based) Registers
    uint16_t A; // Accumulator (C)
    uint16_t X; // Index X
    uint16_t Y; // Index Y
    uint16_t DP; // Direct Page Register
    uint16_t SP; // Stack Pointer
    uint8_t DB; // Data Bank Register
    uint8_t PB; // Program Bank Register
    uint16_t PC; // Program Counter
    uint8_t SR; // Status Register (Flags: N, V, M, X, D, I, Z, C)

    std::vector<uint32_t> frameBuffer;

    void parseHeader();
    void executeNextInstruction();
};

#endif // SNES_H
