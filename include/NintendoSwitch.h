#ifndef NINTENDOSWITCH_H
#define NINTENDOSWITCH_H

#include "Console.h"
#include <string>
#include <vector>
#include <cstdint>

class NintendoSwitch : public Console {
public:
    struct NroHeader {
        uint32_t entryPoint;
        char magic[5]; // "NRO0" + null terminator
        uint32_t size;
        uint32_t flags;
        uint32_t textSize;
        uint32_t roSize;
        uint32_t dataSize;
        uint32_t bssSize;
        uint8_t buildId[32];
        bool isValid;
    };

    NintendoSwitch();
    ~NintendoSwitch() override = default;

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

    // Switch specific inspection getters
    const NroHeader& getHeader() const;
    uint64_t getXRegister(int index) const;
    uint64_t getPC() const;
    uint64_t getSP() const;
    uint32_t getPstate() const;

private:
    std::string romFileName;
    std::vector<uint8_t> romData;
    NroHeader header;
    bool running;

    // ARMv8-A Cortex-A57 (64-bit ARM64) Registers
    uint64_t xRegisters[31]; // X0-X30
    uint64_t SP;
    uint64_t PC;
    uint32_t pstate;

    std::vector<uint32_t> frameBuffer;

    void parseHeader();
    void executeNextInstruction();
};

#endif // NINTENDOSWITCH_H
