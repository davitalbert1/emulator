#include "EmulatorApp.h"
#include <iostream>

int main() {
    EmulatorApp app;
    if (!app.init()) {
        std::cerr << "Falha ao inicializar o aplicativo do emulador.\n";
        return -1;
    }
    app.run();
    return 0;
}
