#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <string>

/*
Controller class acts as a controller for a setting in the settings menu
It handles incrememting/decrementing the value and can store a unit, default value and pointer to the setting variable, as well as an increment value
I decided to use a class to handle changing the variables to avoid the settings state incorrectly altering key variables
Class is contained entirely in header as it is quite simple
*/

class Controller {
    public:                                                                                                         //Public member functions/variables
        Controller(int& var, int defVal, int min, int max, int increment, std::string unit) :                       //Class constructor
            _var {var}, _defaultValue {defVal}, _min {min}, _max {max}, _increment {increment}, _unit {unit} {}

        void increment() {                                                                                          //Handles incrementing value of variable
            if(_var <= (_max - _increment)) {                                                                       //Increases by defined increment amount
                _var += _increment;
            }
        }

        void decrement() {                                                                                          //Handles decrementing value of variable
            if(_var >= (_min + _increment)) {                                                                       //Decreases by defined increment amount
                _var -= _increment;
            }
        }

        float get_percent() {                                                                                       //Returns variable value as a percentage of its range
            float percent = float(_var - _min) / (_max - _min);
            return percent;
        }

        void reset() { _var = _defaultValue;}                                                                       //Sets variable value to default

        int& get_var() {return _var;}                                                                               //Returns value of variable

        std::string get_unit() {return _unit;}                                                                      //Returns unit

    private:                                                                                                        //Private member variables
        int& _var;
        int _defaultValue;
        int _min;
        int _max;
        int _increment;
        std::string _unit;
};



#endif //CONTROLLER_H