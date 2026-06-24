#ifndef NINTENDO2DS_H
#define NINTENDO2DS_H

#include "Nintendo3DS.h"

class Nintendo2DS : public Nintendo3DS {
public:
    Nintendo2DS();
    ~Nintendo2DS() override = default;

    std::string getName() const override;
};

#endif // NINTENDO2DS_H
