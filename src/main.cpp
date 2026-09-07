// Arduino entry points delegate initialization and loop work to the application.
#include "app/Application.hpp"

void setup() {
    app::begin();
}

void loop() {
    app::update();
}
