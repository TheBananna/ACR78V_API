// ACR78V_API.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <string>
#include "PositionersAPI.h"
#include <vector>

using namespace std;

int __cdecl main(int argc, char** argv)
{
    startup();
    vector<float> xs = { 0, 1, 4, 9, 16, 25, 36 };
    vector<float> ys = { 0, 1, 2, 3, 4, 5, 6 };
    add_moves(xs, ys);
    velocity_steer_run();
}

