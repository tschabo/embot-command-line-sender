#include <iostream>
#include "serial/serial.h"
#include "pes.h"
#include <fmt/core.h>
#include <unistd.h>
#include <fstream>

#include <cxxopts.hpp>

float x_offset = 0.0;
float y_offset = 0.0;

// Calculate ticks per rotation (one stitch)
// facts (for my setup):
// 200 steps per one motor rotation
// microstepping: 16 ticks per step
// 16 teeth pulley on the motor
// 58 teeth pulley on the sewing machine
// -> 200*16*(58/16) = 11600
const int ticks_per_stitch = 11600;

// Calculate the part of the stitch, when the hoop is able to move.
// Only a quater of one stitch rotation can be used to move the hoop.
// The beginning of this Part is, when the needle is on it's highes position.
// This is the point when the sewing thread is free in will not break.
const int ticks_hoop_moving = (ticks_per_stitch / 4);

// ... the spare ticks belong to the part, when the hoop is not moving.
// If there is a remainder of the division, add it to this part.
// ticks_hoop_moving + ticks_hoop_not_moving has to be exactly ticks_per_stitch,
// otherwise the stitches drift away and it's possible that the needle is in the Fabric,
// when the hoop moves.
const int ticks_hoop_not_moving = (ticks_per_stitch / 4) * 3 + ticks_per_stitch % 4;

// This is the max speed value on which the stepper motor of the sewing machine
// will properly work.
// You have to try it out.
// This value works on my setup.
const int max_speed = 900;

void send_one(serial::Serial &ser, const stitch &s, int mot)
{
    auto cmmd = fmt::format(">m{};{};{};{};", (x_offset + s.x) / 10, (y_offset + s.y) / 10, int(mot), s.speed);
    std::cout << (cmmd) << "\n";
    ser.write(cmmd);
    ser.flush();
    while (ser.available() < 1)
    {
    }
    auto x = ser.read();
    std::cout << (x) << "\n";
    if (x[0] == '!')
    {
        while (ser.available() < 1)
        {
        }
        x = ser.read();
        std::cout << (x) << "\n";
    }
}

// Precalculate speed for each stitch
// Ramp up speed after- and down before jump stitches
void calc_speed(pes &pattern)
{

    for (auto &block : pattern.blocks)
    {
        for (auto &stitch : block.stitches)
        {
            stitch.speed = max_speed;
        }
    }
    for (auto &block : pattern.blocks)
    {
        for (int idx = 0; idx < block.stitches.size(); ++idx)
        {
            auto &current = block.stitches[idx];

            // We treat the first and stitch as if it were a jump stitch.
            // In this cases we need low speed.
            if (idx == 0 || idx == (block.stitches.size()-1))
                current.jumpstitch = 1;

            if (current.jumpstitch)
            {
                // This is how it should look after this block has been run through
                //
                // ____       ____  max speed
                //     \     /
                //      \   /
                //       \-/        min speed for jump stitch
                //
                //        ^
                //        |
                //     current
                //     position

                current.speed = 100;

                // go backwards from current position -> ramp down
                stitch previous = current;
                for (int idx_reverse = idx - 1;; --idx_reverse)
                {
                    if (idx_reverse < 0)
                        break;

                    auto &curent_stitch = block.stitches[idx_reverse];

                    int val = previous.speed + 100;

                    if (val > max_speed)
                        val = max_speed;

                    if (curent_stitch.speed > val)
                        curent_stitch.speed = val;
                    else
                        break;

                    if (val == max_speed)
                        break;

                    previous = curent_stitch;
                }

                // go forward from current position -> ramp up
                previous = current;
                for (int idx_forward = idx + 1;; idx_forward++)
                {
                    if (idx_forward >= block.stitches.size())
                        break;
                    
                    auto &current_stitch = block.stitches[idx_forward];
                    
                    int val = previous.speed + 100;
                    
                    if (val > max_speed)
                        val = max_speed;
                    
                    current_stitch.speed = val;

                    if (val == max_speed)
                        break;
                    previous = current_stitch;
                }
            }
        }
    }
}

int main(int argc, char **argv)
{
    cxxopts::Options options("stitcher", "Sends data from a pes file to the embroidery machine");
    try
    {
        options.add_options()("f,file", "path to the pes file", cxxopts::value<std::string>())("s,serial", "serial port", cxxopts::value<std::string>());

        auto result = options.parse(argc, argv);

        auto ser = serial::Serial(result["serial"].as<std::string>());

        auto buffer = read_file(result["file"].as<std::string>());
        pes pattern = parse_pes(buffer);
        buffer = {};

        calc_speed(pattern);

        ser.write(">e");
        sleep(1);
        while (ser.available() < 1)
        {
        }
        std::cout << (ser.read()) << "\n";

        if (pattern.min_x < 0)
            x_offset = -pattern.min_x;
        if (pattern.min_y < 0)
            y_offset = -pattern.min_y;

        for (auto it_blocks = pattern.blocks.begin(); it_blocks != pattern.blocks.end(); ++it_blocks)
        {
            auto ansiEscapedColor = fmt::format("\x1B[48;2;{};{};{}m   \033[0m\n", (*it_blocks).block_color.r, (*it_blocks).block_color.g, (*it_blocks).block_color.b);
            std::cout << "\nNext color: " << ansiEscapedColor << "hit return when ready\n";
            while (std::cin.get() != '\n')
            {
            };

            for (auto it_stitches = (*it_blocks).stitches.begin(); it_stitches != (*it_blocks).stitches.end(); ++it_stitches)
            {
                if ((*it_stitches).jumpstitch == 0)
                {
                    send_one(ser, (*it_stitches), ticks_hoop_moving);
                    send_one(ser, (*it_stitches), ticks_hoop_not_moving);
                }
                else
                {
                    send_one(ser, (*it_stitches), 0);
                    send_one(ser, (*it_stitches), ticks_per_stitch);
                }
            }
        }
        ser.write(">d");
        while (ser.available() < 1)
        {
        }
        std::cout << ser.read() << "\n";
        std::cout.flush();
    }
    catch (const std::exception &e)
    {
        std::cout << e.what() << "\n"
                  << options.help();
        return -1;
    }
    return 0;
}
