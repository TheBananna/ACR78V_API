#pragma once
#include <iostream>
#include <string>
#define WIN32_LEAN_AND_MEAN
#define _USE_MATH_DEFINES

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <vector>
#include <math.h>
#include <sstream>
#include <cmath>
using namespace std;


// Need to link with Ws2_32.lib, Mswsock.lib, and Advapi32.lib
#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")


#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "5002"
#define DEFAULT_IP "192.168.100.1"
static SOCKET sock = INVALID_SOCKET;
static string _ip_address = "192.168.100.1";
static int _port = 5002;
static string _motors[2] = { "X", "Y" };
static string _pos_alias[2] = { "P12290", "P12546" };
static int _centers[2] = { 3029374, 34538205 };
static int _ratios[2] = { 189 * 524288, 765 * 524288 };
static float _vel = 4.9;

string _dumb_transmit(SOCKET sock, string command)
{
    send(sock, command.c_str(), command.length(), NULL);
    char buf[128];

    Sleep(100);
    recv(sock, buf, 128, NULL);

    return buf;
}

string _transmit(SOCKET sock, string command)
{
    send(sock, command.c_str(), command.length(), NULL);
    cout << "Sent: " << command;
    char buf[32];
    string full_response = "";

    while (true)
    {
        ZeroMemory(buf, 32);
        recv(sock, buf, 32, NULL);
        int i = 0;
        while (buf[i] != '\0')
        {
            full_response += buf[i];
            i++;
        }
        if (full_response.find("SYS>") != -1 || full_response.find("P00>") != -1)
            break;
    }

    cout << "Recieved: " << full_response << endl;

    return full_response;
}

string send_ascii_command(const string init_command)
{
    string command = init_command + "\r\n";
    string response;
    int count = 0;
    while (true)
    {
        response = _transmit(sock, command);
        if (response.find("Unknown Command") != -1 && count < 5)
        {
            _dumb_transmit(sock, "PROG0\r\n");
            cout << "Comms Retry on : " + command;
            count++;
        }
        else if (count >= 5)
        {
            cout << "Communication Error" << endl;
            return "";
        }
        else
            break;
    }
    return response;
}

bool is_number(const std::string& s)
{
    return !s.empty() && std::find_if(s.begin(),
        s.end(), [](unsigned char c) { return !std::isdigit(c); }) == s.end();
}

//orig is the sequence that is replaced everywhere by orig
string _replace(const string s, const string orig, const string replace)
{
    string copy(s);
    int pos = copy.find(orig);
    while (pos != -1) {
        copy.replace(pos, orig.length(), replace);
        pos = copy.find(orig);
    }
    return copy;
}

float _decode_response(const string& response)
{
    _replace(response, "\n", " ");
    stringstream ss(response);
    string word;
    string longest = "";
    while (ss >> word)
    {
        string decimated = _replace(_replace(word, "-", ""), ".", "");
        if (decimated.length() > longest.length() && is_number(decimated))
            longest = decimated;
    }

    return stof(longest);
}

//azi = _decode_response(send_ascii_command(f'? {_pos_alias[1]}'))
//if azi is None :
//return get_azimuth()
//_curr_azi = azi
//return azi / _ratios[1] * 360
float get_elevation()
{
    float el = _decode_response(send_ascii_command("? " + _pos_alias[0]));
    return el / _ratios[0] * 360;
}

float get_azimuth()
{
    float azi = _decode_response(send_ascii_command("? " + _pos_alias[1]));
    return azi / _ratios[1] * 360;
}

float set_elevation(float el)
{
    if (abs(el) > 90)
        throw exception("abs(Elevation) cannot exceed 90 degrees");
    send_ascii_command(_motors[0] + " " + to_string(el));
    return abs(get_elevation() - el) / _vel;
}

float set_azimuth(float az)
{
    send_ascii_command(_motors[1] + " " + to_string(az));
    return abs(get_azimuth() - az) / _vel;
}

float set_el_az(float el, float az)
{
    if (abs(el) > 90)
        throw invalid_argument("Elevation cannot be more than 90 degrees from center");
    send_ascii_command(_motors[0] + to_string(el) + " " + _motors[1] + to_string(az));
    return max(abs(get_elevation() - el), abs(get_azimuth()) - az) / _vel;
}

float drive_el_az(float el, float az, bool ignore_limits = false)
{
    if (!ignore_limits && abs(get_elevation() + el) > 90)
        throw invalid_argument("Elevation cannot be more than 90 degrees from center");
    send_ascii_command(_motors[0] + "/" + to_string(el) + " " + _motors[1] + "/" + to_string(az));
    return max(abs(el) / _vel, abs(az) / _vel);
}

void set_motion_parameters(float vel, float acc, float dec, float stp)
{
    send_ascii_command("VEL " + to_string(vel));
    send_ascii_command("ACC " + to_string(acc));
    send_ascii_command("DEC " + to_string(dec));
    send_ascii_command("STP " + to_string(stp));
}

void set_angle(float el, float az)
{
    send_ascii_command("RES " + _motors[0] + " " + to_string(el) + " RES " + _motors[1] + " " + to_string(az));
}

float _cold_start()
{
    float d_el = (_centers[0] - get_elevation() / 360 * _ratios[0]) / _ratios[0] * 360;
    if (d_el > 360)
        d_el = fmod(d_el, 360.);
    if (d_el < -360)
        d_el = -fmod(-d_el, 360.);

    float d_az = (_centers[1] - get_azimuth() / 360 * _ratios[1]) / _ratios[1] * 360;
    if (d_az > 360)
        d_az = fmod(d_az, 360);
    if (d_az < -360)
        d_az = -fmod(-d_az, 360);
    
    return drive_el_az(round(d_el * 100000.) / 100000., round(d_az * 100000.) / 100000., true);
}

float bring_to_home()
{
    return set_el_az(0, 0);
}

void switch_to_az_el() {
    _motors[0] = "X";
    _motors[1] = "Y";
    _centers[0] = 3029374; // el
    _centers[1] = 34538205;// az
    _ratios[0] = 189 * 524288;
    _ratios[1] = 765 * 524288;
    _pos_alias[0] = "P12290";
    _pos_alias[1] = "P12546";
}

void switch_to_el_az() {
    _motors[0] = "A";
    _motors[1] = "Z";
    _centers[0] = 26068697;
    _centers[1] = 2598727;
    _ratios[0] = 153 * 524288;
    _ratios[1] = 153 * 524288;
    _pos_alias[0] = "P13058";
    _pos_alias[1] = "P12802";
}

void get_motion_parameters(float& vel, float& acc, float& dec, float& stp)
{
    vel = _decode_response(send_ascii_command("? VEL"));
    acc = _decode_response(send_ascii_command("? ACC"));
    dec = _decode_response(send_ascii_command("? DEC"));
    stp = _decode_response(send_ascii_command("? STP"));
}

int startup()
{
    //First, connect to the controller
    WSADATA wsaData;
    struct addrinfo* result = NULL,
        * ptr = NULL,
        hints;
    int iResult;

    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        printf("WSAStartup failed with error: %d\n", iResult);
        return -1;
    }

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    // Resolve the server address and port
    iResult = getaddrinfo(DEFAULT_IP, DEFAULT_PORT, &hints, &result);
    if (iResult != 0) {
        printf("getaddrinfo failed with error: %d\n", iResult);
        WSACleanup();
        return 1;
    }

    // Attempt to connect to an address until one succeeds
    for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {

        // Create a SOCKET for connecting to server
        sock = socket(ptr->ai_family, ptr->ai_socktype,
            ptr->ai_protocol);
        if (sock == INVALID_SOCKET) {
            printf("socket failed with error: %ld\n", WSAGetLastError());
            WSACleanup();
            return 1;
        }

        // Connect to server.
        iResult = connect(sock, ptr->ai_addr, (int)ptr->ai_addrlen);
        if (iResult == SOCKET_ERROR) {
            closesocket(sock);
            sock = INVALID_SOCKET;
            continue;
        }
        break;
    }

    freeaddrinfo(result);

    if (sock == INVALID_SOCKET) {
        printf("Unable to connect to server!\n");
        WSACleanup();
        return 1;
    }

    switch_to_az_el();
    _dumb_transmit(sock, "PROG0\r\n");
    _dumb_transmit(sock, "DRIVE ON X Y Z A\r\n");
    set_motion_parameters(_vel, 10, 10, 10);

    if ((int)_decode_response(send_ascii_command("? started")) == 1)
    {
        float wait = bring_to_home();
        switch_to_el_az();
        wait = max(wait, bring_to_home());
        switch_to_az_el();
        Sleep(wait * 1000);
        return 0;
    }

    float stall = _cold_start();

    switch_to_el_az();
    stall = max(stall, _cold_start());

    Sleep(stall * 1000);
    set_angle(0, 0);
    switch_to_az_el();
    set_angle(0, 0);
    send_ascii_command("started=1");

    return 0;
}

void halt()
{
    send_ascii_command("HALT ALL");
}

void reboot() 
{
    _dumb_transmit(sock, "reboot\r\n");
    closesocket(sock);
    WSACleanup();
    Sleep(20000);
    startup();
}

float circw(float target_x, float target_y, float center_x, float center_y)
{
    float el = get_elevation();
    float az = get_azimuth();
    float v1x = el - center_x;
    float v1y = az - center_y;
    float v2x = target_x - center_x;
    float v2y = target_y - center_y;

    float t1 = atan2(v1y, v1x) / 2 / M_PI * 360;
    float t2 = atan2(v2y, v2x) / 2 / M_PI * 360;

    float circumference = sqrt(v2x * v2x + v2y * v2y) * 2 * M_PI;

    send_ascii_command("CIRCW " + _motors[0] + " (" + to_string(target_x) + "," + to_string(center_x) + ") " + _motors[1] + " (" + to_string(target_y) + "," + to_string(center_y) + ")");

    return circumference * fmod((t1 - t2), 360) / 360 / _vel;
}

float circcw(float target_x, float target_y, float center_x, float center_y)
{
    float el = get_elevation();
    float az = get_azimuth();
    float v1x = el - center_x;
    float v1y = az - center_y;
    float v2x = target_x - center_x;
    float v2y = target_y - center_y;

    float t1 = atan2(v1y, v1x) / 2 / M_PI * 360;
    float t2 = atan2(v2y, v2x) / 2 / M_PI * 360;

    float circumference = sqrt(v2x * v2x + v2y * v2y) * 2 * M_PI;

    send_ascii_command("CIRCCW " + _motors[0] + " (" + to_string(target_x) + "," + to_string(center_x) + ") " + _motors[1] + " (" + to_string(target_y) + "," + to_string(center_y) + ")");

    return circumference * fmod((t1 - t2), 360) / 360 / _vel;
}

//x means elevation and y means azimuth
static vector<float> moves_x_queue;
static vector<float> moves_y_queue;

void add_moves(vector<float> moves_x, vector<float> moves_y)
{
    moves_x_queue.insert(moves_x_queue.end(), moves_x.begin(), moves_x.end());
    moves_y_queue.insert(moves_y_queue.end(), moves_y.begin(), moves_y.end());
}

void add_move(float x, float y)
{
    moves_x_queue.push_back(x);
    moves_y_queue.push_back(y);
}

void program_moves()
{
    float vel, acc, dec, stp;
    get_motion_parameters(vel, acc, dec, stp);
    float el = get_elevation();
    float az = get_azimuth();

    send_ascii_command("NEW");
    send_ascii_command("PROGRAM");

    try {
        send_ascii_command("STP 0");
    }
    catch (exception e) {
        send_ascii_command("ENDP");
        throw e;
    }

    for (int i = 0; i < moves_x_queue.size(); i++) {
        //send_ascii_command(f'{_motors[0]}{round(move[0], 4)} {_motors[1]}{round(move[1], 4)}')
        float x = round(moves_x_queue[i] * 1000.) / 1000.;
        float y = round(moves_y_queue[i] * 1000.) / 1000.;
        send_ascii_command(_motors[0] + to_string(x) + " " + _motors[1] + to_string(y));
    }

    moves_x_queue.clear();
    moves_y_queue.clear();
    send_ascii_command("STP " + to_string(stp));
    send_ascii_command("ENDP");
}

void run_moves()
{
    send_ascii_command("RUN PROG0");
}

void velocity_steer_run()
{
    Sleep(1000 * set_el_az(moves_x_queue[0], moves_y_queue[1]));
    float vel, acc, dec, stp;
    get_motion_parameters(vel, acc, dec, stp);

    send_ascii_command("jog acc x" + to_string(acc) + " y" + to_string(acc) + " z" + to_string(acc) + " a" + to_string(acc));
    send_ascii_command("jog dec x" + to_string(dec) + " y" + to_string(dec) + " z" + to_string(dec) + " a" + to_string(dec));
    send_ascii_command("jog vel x" + to_string(vel) + " y" + to_string(vel) + " z" + to_string(vel) + " a" + to_string(vel));

    int i = 0;
    for (i = 1; i < moves_x_queue.size() - 1; i++) {
        float x = moves_x_queue[i];
        float y = moves_y_queue[i];

        float vecx = x - moves_x_queue[i - 1];
        float vecy = y - moves_y_queue[i - 1];

        float mag = sqrt(vecx * vecx + vecy * vecy);
        vecx = vecx / mag * vel;
        vecy = vecy / mag * vel;

        send_ascii_command("jog vel " + _motors[0] + to_string(vecx) + " " + _motors[1] + to_string(vecy));
        send_ascii_command("jog " + (string)((vecx >= 0) ? "fwd" : "rev") + " " + _motors[0]);
        send_ascii_command("jog " + (string)((vecy >= 0) ? "fwd" : "rev") + " " + _motors[1]);

        float next_vecx = moves_x_queue[i + 1] - x;
        float next_vecy = moves_y_queue[i + 1] - y;
        mag = sqrt(next_vecx * next_vecx + next_vecy * next_vecy);
        next_vecx = next_vecx / mag * vel;
        next_vecy = next_vecy / mag * vel;

        float dvecx = next_vecx - vecx;
        float dvecy = next_vecy - vecy;

        if (abs(vecx) > abs(vecy)) {
            float el = get_elevation();
            while (vecx < 0 ? el > x - dvecx / 2 / acc : el < x - dvecx / 2 / acc)
                el = get_elevation();
        }
        else {
            float az = get_azimuth();
            while (vecx < 0 ? az > y - dvecy / 2 / acc : az < y - dvecy / 2 / acc)
                az = get_azimuth();
        }
    }

    send_ascii_command("jog abs " + _motors[0] + to_string(moves_x_queue[moves_x_queue.size() - 1]) + " " + _motors[1] + to_string(moves_y_queue.size() - 1));
    send_ascii_command("jog off " + _motors[0] + " " + _motors[1]);
    moves_x_queue.clear();
    moves_y_queue.clear();
}