# FF7RebirthFovChanger

The default fov of FF7 Rebirth is a pretty absurd 60 degrees. 
This is a simple C++ Console program to change the field of view in the PC version of Final Fantasy VII Rebirth.

Shoutout to Cheat Engine's pointer scan feature.

## Usage

Run the executable and press the hotkeys to change the fov. The default hotkeys are:

 - Numpad + : Increase FOV by 5
 - Numpad - : Decrease FOV by 5
 - Numpad * : Reset to 60

The program detects if the game is closed and will gracefully exit.

## Building

No external dependencies so building is pretty straightforward.
Just clone the repo, open the solution in Visual Studio and build the project.
The executable will be in the bin folder.

## Other

Finds: 
- [[["ff7rebirth_.exe" + 0x092A0388] + 0xB8] + 0x40] + 0x470
- [[["ff7rebirth_.exe" + 0x092A0388] + 0xB8] + 0x40] + 0x340
- [["ff7rebirth_.exe" + 0x090A71E8] + 0x50] + 0x438
which are some of the static pointer chain addresses to stored fov values.

WIP: Still working on a generalized camera fov finder.
There seem to be many types of camera objects (?) that contain their own fov value. For example, the three cameras currently referenced in the code don't cover the Sephiroth combat section at the beginning after Zach's section.

Cheat Engine table included for reference.