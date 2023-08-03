// ACR78V_API.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <string>
#include "PositionersAPI.h"
using namespace std;

int __cdecl main(int argc, char** argv)
{
    startup();

    drive_el_az(20, 20);
}

