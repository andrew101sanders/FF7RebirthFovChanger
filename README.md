# FF7RebirthFovChanger

The default fov of FF7 Rebirth is a pretty absurd 60 degrees. 
This is a simple C++ Console program to change the field of view in the PC version of Final Fantasy VII Rebirth.

Shoutout to emoose's [FFVIIHook](https://www.nexusmods.com/finalfantasy7rebirth/mods/4) and Cheat Engine's pointer scan feature.

## Usage

Run the executable and press the hotkeys to change the fov. 
Use __FF7RebirthFovMod.ini__ to set custom hotkeys and initialFov.
If the file does not exist, the program will create it with default values.

The default hotkeys are:

 - Keypad + : Increase FOV by 5
 - Keypad - : Decrease FOV by 5
 - Keypad * : Set to 60
 - Keypad / : Reset to 0 (Default FOV Cvar)

The game uses 0 as the default FOV Cvar value, which lets the game know to use default camera FOVs.
For example, Cloud's section in Nibelheim has a default FOV of 65, but open world sections are generally 60.
Setting this Cvar value to something other than 0 will control those FOV values.

The program detects if the game is closed and will gracefully exit.

## Building

No external dependencies so building is pretty straightforward.
Just clone the repo, open the solution in Visual Studio and build the project.
The executable will be in the bin folder.

## Other

Finds: 
- [[["ff7rebirth_.exe" + 0x09020218] + 0x5F0] + 0xE8] + 0x244

which is a static pointer chain to the fov cvar value.

Cheat Engine table included for reference.