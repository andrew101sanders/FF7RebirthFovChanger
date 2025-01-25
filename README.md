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