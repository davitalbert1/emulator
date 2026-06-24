#ifndef GAMEBOYADVANCE_H
#define GAMEBOYADVANCE_H

#include "Console.h"
#include <string>
#include <vector>
#include <cstdint>

class GameBoyAdvance : public Console {
public:
    // Struct representing GBA ROM Header info (parsed from offset 0xA0 to 0xBE of ROM)
    struct ROMHeader {
        char gameTitle[13];      // 12 chars + null terminator
        char gameCode[5];        // 4 chars + null terminator
        char makerCode[3];      // 2 chars + null terminator
        uint8_t fixedValue;     // Must be 0x96
        uint8_t mainUnitCode;
        uint8_t deviceType;
        uint8_t softwareVersion;
        uint8_t checksum;
        bool isValid;
    };

    GameBoyAdvance();
    ~GameBoyAdvance() override = default;

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

    // GBA specific inspection methods
    const ROMHeader& getHeader() const;
    uint32_t getRegister(int index) const;
    uint32_t getCPSR() const;
    size_t getROMSize() const;
    uint32_t getPC() const;

private:
    std::string romFileName;
    std::vector<uint8_t> romData;
    ROMHeader header;
    bool running;

    // Simulated ARM7TDMI register state
    // R0-R12: General purpose
    // R13: SP (Stack Pointer)
    // R14: LR (Link Register)
    // R15: PC (Program Counter)
    uint32_t registers[16];
    uint32_t cpsr; // Current Program Status Register

    // Simulated Memory Mapped regions (skeletons for future extension)
    std::vector<uint8_t> wramBoard; // 256 KB Work RAM
    std::vector<uint8_t> wramChip;  // 32 KB Work RAM
    std::vector<uint8_t> vram;      // 96 KB Video RAM

    void parseHeader();
    void executeNextInstruction();
};

#endif // GAMEBOYADVANCE_H
