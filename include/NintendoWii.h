#ifndef NINTENDOWII_H
#define NINTENDOWII_H

#include "Console.h"
#include <string>
#include <vector>
#include <cstdint>

class NintendoWii : public Console {
public:
    struct DiscHeader {
        char gameId[7]; // 6 chars + null terminator
        uint8_t discNumber;
        uint8_t discVersion;
        uint8_t streamingAudio;
        uint8_t streamingBufferSize;
        char gameTitle[65]; // 64 chars + null terminator
        uint32_t magic; // 0x5D1C9EA3 for Wii, 0xC12F3C1A for GameCube
        bool isValid;
    };

    NintendoWii();
    ~NintendoWii() override = default;

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

    // Wii specific inspection getters
    const DiscHeader& getHeader() const;
    uint32_t getGPR(int index) const;
    uint32_t getPC() const;
    uint32_t getLR() const;
    uint32_t getCTR() const;
    uint32_t getCR() const;
    uint32_t getMSR() const;

private:
    std::string romFileName;
    std::vector<uint8_t> romData;
    DiscHeader header;
    bool running;

    // IBM Broadway (PowerPC 750CL) Registers
    uint32_t gpr[32]; // General Purpose Registers GPR0-GPR31
    uint32_t PC;
    uint32_t LR; // Link Register
    uint32_t CTR; // Count Register
    uint32_t CR; // Condition Register
    uint32_t XER; // Integer Exception Register
    uint32_t MSR; // Machine State Register

    std::vector<uint32_t> frameBuffer;

    void parseHeader();
    void executeNextInstruction();
};

#endif // NINTENDOWII_H
