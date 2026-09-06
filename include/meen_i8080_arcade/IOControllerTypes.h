/*
Copyright (c) 2021-2026 Nicolas Beddows <nicolas.beddows@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#ifndef IOCONTROLLERTYPES_H
#define IOCONTROLLERTYPES_H

namespace meen_i8080_arcade
{
    /** User input bit mask

        The targetted peripheral device implementation will set the input bit mask accordingly.
    */
    enum Input
    {
        None = 0x000000,
        OnePlayer = 0x000001,
        TwoPlayer = 0x000002,
        ThreeShips = 0x000004,
        FourShips = 0x000008,
        FiveShips = 0x000010,
        SixShips = 0x000020,
        P1Left = 0x000040,
        Credit = 0x000080,
        P1Right = 0x000100,
        ExtraShip = 0x000200,
        CoinInfo = 0x000400,
        P2Left = 0x000800,
        P2Fire = 0x001000,
        P2Right = 0x002000,
        Exit = 0x004000,
        LoadRom = 0x008000, // Load from a save file
        P1Fire = 0x010000,
        Tilt = 0x020000,
        SaveRom = 0x040000,
        NextRom = 0x080000,
        QuitRom = 0x100000,
        SelectRom = 0x200000, // Load from rom file
        PreviousRom = 0x400000
    };

    /** Game play screens

        The phases of the i8080 arcade emulator.

        @remark	   Addtional screens to add could include Highscore for example.
        @remark    The i8080 arcade emulator starts on the RomSelect screen, its position should not be changed.
    */
    enum Screen
    {
        RomSelect,  /**< The rom select screen where the user can select a rom to load. */
        Gameplay    /**< The emulated game play screen for the selected rom that was loaded in the rom select screen. */
    };

    struct BoundingBox
    {
        int x{};
        int y{};
        int w{};
        int h{};
    };

    struct ScreenTransition
    {
        Screen current; /**< The screen that we are currently on as documented in the Screen enum */
        Screen next;    /**< The screen that we are transitioning to as documented in the Screen enum */
    };

} // namespace meen_i8080_arcade

#endif // IOCONTROLLER_TYPES_H
