#include "Nintendo2DS.h"

Nintendo2DS::Nintendo2DS() : Nintendo3DS() {
    // 2DS shares identical hardware parameters, screen coordinates, and CPU register maps
    // as the 3DS, but lacks the 3D depth slider/render outputs.
}

std::string Nintendo2DS::getName() const {
    return "Nintendo 2DS";
}
