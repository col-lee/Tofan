#pragma once

class InputController {
public:
    void begin();
    void update();
    void handleInput();

private:
    unsigned long backBtnPressTime = 0;
    bool isBackBtnLongPressed = false;
    unsigned long lastTouchTime = 0;
    unsigned long lastVolActivityTime = 0;
};

extern InputController inputController;
