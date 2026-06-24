#ifndef NINTENDODS_H
#define NINTENDODS_H

#include "Console.h"
#include <string>
#include <vector>
#include <cstdint>

class NintendoDS : public Console {
public:
    struct NDSHeader {
        char gameTitle[13];      // 12 chars + null terminator
        char gameCode[5];        // 4 chars + null terminator
        char makerCode[3];      // 2 chars + null terminator
        uint8_t unitCode;       // 0 = NDS, 2 = DSi
        uint8_t deviceType;     // Card device type
        uint8_t deviceSize;     // Card size
        uint8_t romVersion;
        uint16_t headerChecksum;
        bool isValid;
    };

    NintendoDS();
    ~NintendoDS() override = default;

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

    // NDS specific inspection getters
    const NDSHeader& getHeader() const;
    uint32_t getARM9Register(int index) const;
    uint32_t getARM9CPSR() const;
    uint32_t getARM7Register(int index) const;
    uint32_t getARM7CPSR() const;

private:
    std::string romFileName;
    std::vector<uint8_t> romData;
    NDSHeader header;
    bool running;

    // ARM946E-S state
    uint32_t arm9Registers[16];
    uint32_t arm9Cpsr;
    uint32_t arm9Spsr;

    // ARM7TDMI state
    uint32_t arm7Registers[16];
    uint32_t arm7Cpsr;
    uint32_t arm7Spsr;

    // Flat framebuffers for separate screens (256x192 each)
    std::vector<uint32_t> topFrameBuffer;
    std::vector<uint32_t> bottomFrameBuffer;

    void parseHeader();
    void executeNextInstruction();
};

#endif // NINTENDODS_H
