#include <iostream>
#include <ctime>
#include <windows.h>

void gotoXY(int x, int y) {
    COORD d;
    d.X = x;
    d.Y = y;
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), d);
}

void F_Loading() {
    std::cout << "\n\n\n\n\n\n";

    std::cout << "\t\t\t       ======================================  \n";
    std::cout << "\t\t\t                    DOWNLOAD TOOLS V2       \n";
    std::cout << "\t\t\t       ====================================== \n";
    std::cout << "\t\t\t                      BY WISNU RAFI             \n";
    std::cout << "\t\t\t       ________________________________________  \n";

    char a = 219;
    gotoXY(45, 14);

    std::cout << "LOADING... " << std::endl;

    gotoXY(37, 16);
    for (int r = 1; r <= 26; r++) {
        for (int speed = 0; speed <= 30000000; speed++);
        std::cout << a;
    }
    std::cout << std::endl;
}
