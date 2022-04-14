#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <string>

class Controller {
    public:
        Controller(int& var, int defVal, int min, int max, int increment, std::string unit) :
            _var {var}, _defaultValue {defVal}, _min {min}, _max {max}, _increment {increment}, _unit {unit} {}

        void increment() {
            if(_var <= (_max - _increment)) {
                _var += _increment;
            }
        }

        void decrement() {
            if(_var >= (_min + _increment)) {
                _var -= _increment;
            }
        }

        float get_percent() {
            float percent = float(_var - _min) / (_max - _min);
            return percent;
        }

        void reset() { _var = _defaultValue;}

        int& get_var() {return _var;}

        std::string get_unit() {return _unit;}

    private:
        int& _var;
        int _defaultValue;
        int _min;
        int _max;
        int _increment;
        std::string _unit;
};



#endif //CONTROLLER_H