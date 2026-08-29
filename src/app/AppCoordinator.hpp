#pragma once

class AppCoordinator {
public:
    void begin();
    void update();
    void startAiPetListening();
    void stopAiPetListening();
    void updateAiPetListening();
};

extern AppCoordinator appCoordinator;
