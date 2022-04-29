//http://educ8s.tv/arduino-rotary-encoder-menu/

#ifndef MENU_H
#define MENU_H

#include <vector>
#include <string>

class Menu {
    public:
        void displayMenu();
        void displaySetting(std::string itemName, int itemValue);
        void displayStep();

    private:
        int _menuItem;
        int _menuFrame;
        int _menuPage;
        int _lastMenuItem;

        std::vector<std::string> menuItems;
};

#endif //MENU_H