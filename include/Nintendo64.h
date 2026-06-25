#ifndef NINTENDO64_H
#define NINTENDO64_H

#include "Console.h"
#include <string>
#include <vector>
#include <cstdint>

class Nintendo64 : public Console {
public:
    struct N64Header {
        uint32_t validationMagic; // 0x80371240 for z64, etc.
        uint32_t clockRate;
        uint32_t bootAddress;
        uint32_t releaseAddress;
        uint32_t crc1;
        uint32_t crc2;
        char gameTitle[21];       // 20 chars + null terminator
        char manufacturerCode;
        char cartridgeId[3];      // 2 chars + null terminator
        char countryCode;
        bool isValid;
    };

    Nintendo64();
    ~Nintendo64() override = default;

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

    // N64 specific inspection getters
    const N64Header& getHeader() const;
    uint64_t getReg(int index) const;
    uint64_t getPC() const;
    uint64_t getHI() const;
    uint64_t getLO() const;
    uint32_t getSR() const;

private:
    std::string romFileName;
    std::vector<uint8_t> romData;
    N64Header header;
    bool running;

    // NEC VR4300 Registers (64-bit MIPS III)
    uint64_t registers[32]; // R0-R31
    uint64_t PC;
    uint64_t HI;
    uint64_t LO;
    uint32_t SR;            // Coprocessor 0 Status Register

    std::vector<uint32_t> frameBuffer;

    void parseHeader();
    void executeNextInstruction();
};

#endif // NINTENDO64_H
