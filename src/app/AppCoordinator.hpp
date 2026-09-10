// Coordinates the AI Pet capture cycle, response task and RGB updates.
#pragma once

class AppCoordinator {
public:
    void begin();
    void update();
    void startAiPetListening();
    void stopAiPetListening();
    void updateAiPetListening();
    void updateAiPetBehavior();
    void reactToAiPetTouch();
};

extern AppCoordinator appCoordinator;
