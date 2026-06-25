#ifndef SEGAGENESIS_H
#define SEGAGENESIS_H

#include "Console.h"
#include <string>
#include <vector>
#include <cstdint>

class SegaGenesis : public Console {
public:
    struct GenesisHeader {
        char consoleName[17]; // 16 chars + null terminator
        char copyright[17]; // 16 chars + null terminator
        char domesticTitle[49]; // 48 chars + null terminator
        char overseasTitle[49]; // 48 chars + null terminator
        char productCode[15]; // 14 chars + null terminator
        uint16_t checksum;
        uint32_t romStart;
        uint32_t romEnd;
        uint32_t ramStart;
        uint32_t ramEnd;
        bool isValid;
    };

    SegaGenesis();
    ~SegaGenesis() override = default;

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

    // Genesis specific inspection getters
    const GenesisHeader& getHeader() const;
    uint32_t getD(int index) const;
    uint32_t getA(int index) const;
    uint16_t getSR() const;
    uint32_t getPC() const;

private:
    std::string romFileName;
    std::vector<uint8_t> romData;
    GenesisHeader header;
    bool running;

    // Motorola 68000 registers
    uint32_t D[8]; // Data registers D0-D7
    uint32_t A[8]; // Address registers A0-A7 (A7 is SP)
    uint16_t SR; // Status Register
    uint32_t PC; // Program Counter

    std::vector<uint32_t> frameBuffer;

    void parseHeader();
    void executeNextInstruction();
};

#endif // SEGAGENESIS_H
