#ifndef ATARI2600_H
#define ATARI2600_H

#include "Console.h"
#include <string>
#include <vector>
#include <cstdint>

class Atari2600 : public Console {
public:
    Atari2600();
    ~Atari2600() override = default;

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

    // Atari 2600 specific inspection getters
    uint8_t getA() const;
    uint8_t getX() const;
    uint8_t getY() const;
    uint8_t getSP() const;
    uint16_t getPC() const;
    uint8_t getSR() const;

private:
    std::string romFileName;
    std::vector<uint8_t> romData;
    bool running;

    // MOS 6507 Registers
    uint8_t A;
    uint8_t X;
    uint8_t Y;
    uint8_t SP;
    uint16_t PC;
    uint8_t SR; // Status Register (Flags: N, V, B, D, I, Z, C)

    std::vector<uint32_t> frameBuffer;

    void executeNextInstruction();
};

#endif // ATARI2600_H
