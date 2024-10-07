#ifndef MIATYPES_H
#define MIATYPES_H

namespace i8080_arcade
{

/** Meen i8080 arcade events

    Events that can be raised from user interaction.
*/
enum MIA_Event
{
    None,            //< No event has occurred.
    Quit,            //< Quit the emulation.
    Reset,           //< Reset the running rom (drops all state).
    NextRom,         //< Unload the running rom and load the next rom in the config file.
    PreviousRom,     //< Unload the running rom and load the previous rom in the config file.
};

} // namespace i8080_arcade
#endif // MIATYPES_H
