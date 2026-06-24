#ifndef NINTENDO3DS_H
#define NINTENDO3DS_H

#include "Console.h"
#include <string>
#include <vector>
#include <cstdint>

class Nintendo3DS : public Console {
public:
    struct NCCHHeader {
        char gameTitle[17];      // 16 chars + null terminator
        char productCode[11];    // 10 chars + null terminator (e.g. CTR-P-CTAP)
        char makerCode[3];      // 2 chars + null terminator
        uint64_t programId;
        uint32_t sdkVersion;
        uint8_t systemMode;
        bool isValid;
    };

    Nintendo3DS();
    ~Nintendo3DS() override = default;

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

    // 3DS specific inspection getters
    const NCCHHeader& getHeader() const;
    uint32_t getARM11Register(int index) const;
    uint32_t getARM11CPSR() const;
    uint32_t getARM9Register(int index) const;
    uint32_t getARM9CPSR() const;

protected:
    std::string romFileName;
    std::vector<uint8_t> romData;
    NCCHHeader header;
    bool running;

    // ARM11 MPCore state
    uint32_t arm11Registers[16];
    uint32_t arm11Cpsr;

    // ARM9 state (security / retrocompatibility)
    uint32_t arm9Registers[16];
    uint32_t arm9Cpsr;

    // Flat framebuffers for separate screens (Top 400x240, Bottom 320x240)
    std::vector<uint32_t> topFrameBuffer;
    std::vector<uint32_t> bottomFrameBuffer;

    void parseHeader();
    void executeNextInstruction();
};

#endif // NINTENDO3DS_H
