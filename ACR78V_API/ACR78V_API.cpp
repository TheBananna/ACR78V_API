// ACR78V_API.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <string>
using namespace std;

string make()
{
    char buf[12] = { 1, 2, 3, 4, 5, 6, 7, 8, 0, 0, 0, 1 };
    string s = buf;
    return buf;
}

int __cdecl main(int argc, char** argv)
{
    
    cout << make() << "!";
}

