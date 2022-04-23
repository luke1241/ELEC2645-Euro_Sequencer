#include <string>
#include "Controller.h"

Controller::Controller(int& var, int defVal, int min, int max, int increment, std::string unit) : 
    _var {var}, _defaultValue {defVal}, _min {min}, _max {max}, _increment {increment}, _unit {unit} {}

void Controller::increment() {
    if(_var <= (_max - increment) {
        _var += increment;
    }
}