# Robot arm controller

Arduino firmware and a browser control app for a 6-servo robotic arm. The servos connect straight to Arduino pins, with the base on pin 7 and the gripper on pin 2.

The app runs in Chrome or Edge on a desktop computer, talks to the Arduino over USB using the Web Serial API, and needs no installation. It works with two sketches: the original sketch this project started from, and a new one that adds easing, limits and position reporting.

## Contents

- [What is in this repository](#what-is-in-this-repository)
- [Hardware](#hardware)
- [Quick start](#quick-start)
- [Using the app](#using-the-app)
- [Controller (gamepad) support](#controller-gamepad-support)
- [The two sketches](#the-two-sketches)
- [Serial protocols](#serial-protocols)
- [Troubleshooting](#troubleshooting)
- [Known limitations](#known-limitations)
- [Sources](#sources)
  
## What is in this repository

| Path | What it is |
|---|---|
| `firmware/robot_arm/robot_arm.ino` | New sketch: smooth motion, per-joint limits, stop and release, position reports. 115200 baud. |
| `firmware/original_sketch/original_sketch.ino` | The original sketch, unmodified, kept for compatibility. 9600 baud. |
| `app/arm_control.html` | The control app: one self-contained HTML file with no dependencies. |

## Hardware

Six servos, wired directly to the Arduino signal pins:

| Joint | Index (app and new sketch) | Pin | Index (original sketch) |
|---|---|---|---|
| Base | 0 | 7 | 6 |
| Shoulder | 1 | 6 | 5 (also drives a mirrored servo on pin 8) |
| Elbow | 2 | 5 | 4 |
| Wrist pitch | 3 | 4 | 3 |
| Wrist roll | 4 | 3 | 2 |
| Gripper | 5 | 2 | 1 |

The role of each joint (for example which one is the wrist pitch) is assumed from the pin order. If your arm differs, change `SERVO_PINS` in the sketch and the `JOINTS` list in the app.

### Power

**Do not power six servos from the Arduino's 5V pin or from USB.** Use a separate 5 to 6 V supply (4 A or more is a safe starting point), join its ground to an Arduino GND pin, and consider a 470 to 1000 µF capacitor across the servo supply near the arm. Without this the board browns out and resets, and servos twitch or wander.

## Quick start

1. **Wire and power the arm** as above.
2. **Upload a sketch** with the Arduino IDE. The folder name must match the sketch name, so open the `.ino` from inside its own folder:
   - `firmware/robot_arm/robot_arm.ino` for the new sketch, or
   - `firmware/original_sketch/original_sketch.ino` to keep using the original.
3. **Close the Arduino IDE Serial Monitor.** It holds the port, so the app could not connect.
4. **Open `app/arm_control.html`** in Chrome or Edge on a desktop computer.
5. In the header, set **Sketch** to match what you uploaded ("Original, 9600 baud" or "New, 115200 baud"), then click **Connect** and pick the Arduino's port.
6. **Set the gripper's range first** (see [Joint ranges](#joint-ranges)), then move the sliders.

If Connect does nothing, serve the folder locally and open it from there, because Web Serial needs a secure context:

```
python3 -m http.server 8000
# then open http://localhost:8000/app/arm_control.html
```

The same works from GitHub Pages: enable Pages for this repository and open the app's URL over HTTPS.

### First power-up

- The servos jump to 90° when the new sketch starts. Keep the arm clear the first time.
- With the original sketch, each joint jumps to the slider value on its first move, because that sketch cannot report where the arm is.
- Keep a hand near the power switch while testing limits.

## Using the app

### Joints and live view

Joints are listed from the gripper (top) to the base (bottom). Each row has a slider, ± buttons for 1° steps, and an angle box. The schematic on the left follows the sliders. It assumes 90° is the neutral pose for every joint, so your arm may bend the other way.

### Joint ranges

Every joint has its own range, with a lower and an upper limit. Type the limits, or jog the joint to a safe end and press **Min = now** or **Max = now**. **Full** resets the joint to 0 to 180. The darker part of a slider track is the allowed range, and sliders and angle boxes stop at the limits. Ranges are saved in the browser. The app enforces them in both modes, and the new sketch enforces them on the Arduino too.

Set the gripper first: find where it just starts to strain at each end, then back each limit off a few degrees.

### Speed, Home, Stop, Release

- **Speed** (1 to 10) is how far a joint moves per 15 ms step, from about 67°/s to about 667°/s. The default is 2 (about 133°/s). The new sketch does the easing on the Arduino; in original mode the app does it.
- **Home** moves every joint to 90° (within its limits).
- **Stop** halts the arm where it is.
- **Release servos** cuts holding torque, so the arm may sag. New sketch only. The next move re-engages the servos.

### Poses and sequences

**Save current pose** stores all six angles. Poses can be renamed, updated to the current angles, reordered, deleted, exported to a JSON file and imported back. **Play sequence** runs the poses top to bottom, waiting for each move to finish and then pausing (default 500 ms), with an optional repeat. Poses and settings are stored in the browser's local storage.

### Serial log

The collapsed log at the bottom shows what is sent and received, with a box for sending a raw command.

## Controller (gamepad) support

Plug in a standard Xbox-style or PlayStation controller and press any button on it; the Controller section shows when it is detected. Names below follow an Xbox controller (PlayStation: LB/RB are L1/R1, LT/RT are L2/R2, A/B/X/Y are cross/circle/square/triangle).

| Input | Action |
|---|---|
| Left stick, left and right | Base |
| Left stick, up and down | Shoulder |
| Right stick, up and down | Elbow |
| LB and RB | Wrist pitch, down and up |
| Right stick, left and right | Wrist roll |
| LT and RT | Gripper, down and up |
| D-pad left and right | Arm speed, slower and faster |
| A | Home |
| B | Stop |
| X | Save current pose |
| Y | Play or stop the sequence |

Right and up raise an angle. Each joint has a **Reverse direction** option, and a **Sensitivity** slider sets the top rate (10 to 180°/s, default 60). Joints stay inside their limits, and the sticks are ignored while a sequence plays. Keep the browser tab in the foreground, because browsers pause controller input in background tabs.

## The two sketches

| | `robot_arm.ino` (new) | `original_sketch.ino` |
|---|---|---|
| Baud rate | 115200 | 9600 |
| Motion | Eased on the Arduino | App sends small steps |
| Limits | Enforced on the Arduino and in the app | In the app only |
| Reports position and "move finished" | Yes | No (the app tracks it itself) |
| Release servos | Yes | No |
| Mirrored shoulder servo on pin 8 | No (see below) | Yes |

In original mode the app does the easing, limits and move-complete detection itself, paced to fit within 9600 baud. The sketch never replies, so the app cannot confirm that commands arrived.

## Serial protocols

### New sketch (`robot_arm.ino`, 115200 baud, newline-terminated text)

| Command | Meaning |
|---|---|
| `S <i> <angle>` or `<i> <angle>` | Move joint `i` (0 to 5) to `angle` (0 to 180) |
| `A a0 a1 a2 a3 a4 a5` | Move all six joints |
| `H` | Home (all 90) |
| `X` | Stop where the arm is |
| `V <1-10>` | Speed, in degrees per 15 ms tick |
| `L <i> <min> <max>` | Set joint `i`'s limits |
| `D` | Release (detach) all servos |
| `P` | Report positions |
| `I` | Identify |
| `?` | List commands |

Replies: `OK`, `ERR <reason>`, `POS a0 a1 a2 a3 a4 a5` (sent once a move has finished), `READY ARM6` after boot, and `ARM6 v1 pins 7 6 5 4 3 2` in answer to `I`. Angles outside a joint's range are clamped.

### Original sketch (9600 baud)

One line per move, `<index> <angle>\n`, where the index is 1 to 6 (see the [hardware table](#hardware)). Index 5 also writes `180 - angle` to the mirrored servo on pin 8. No replies are sent.

## Troubleshooting

- **App cannot open the port.** Close the Arduino IDE Serial Monitor and any other program using it.
- **"The Arduino did not answer" (new sketch).** Check that the sketch is uploaded, the board is the one you picked, and **Sketch** is set to "New, 115200 baud".
- **Nothing moves in original mode.** Check **Sketch** is set to "Original, 9600 baud". The original sketch never replies, so the app cannot tell you if commands are not arriving.
- **Servos twitch, wander or the board resets.** Almost always power: use a separate supply, join the grounds, add a capacitor, and check connectors.
- **One servo wanders while the others hold.** Swap its signal wire to a working pin to see whether the servo or the wire is at fault; run it with no load; check it is not pushing against an end stop (narrow its range); and consider a worn servo, especially a cheap one. If narrowing does not help, try 600 and 2300 for `PULSE_MIN_US` and `PULSE_MAX_US` in the new sketch.

## Known limitations

- **No hardware testing yet** (see Status).
- **The new sketch drives six servos only.** If your shoulder has a mirrored second servo on pin 8, use the original sketch, or add mirror support to `robot_arm.ino`.
- **Joint roles are assumed.** The schematic and the controller's direction conventions may not match your arm; use Reverse direction and the app's ranges to adapt.
- **Original mode cannot read the arm's position,** so each joint may jump on its first move after connecting.
- **Browser support:** Web Serial works in Chrome and Edge on desktop only. Firefox and Safari do not support it.

## Sources

- **Reference arm:** <https://www.youtube.com/watch?v=R7pfTQcmXE4> is the video of the arm this project is meant to control. It could not be opened when this project was built (the fetch was blocked), so nothing in this repository was taken from it. The six-joint layout comes from the wiring description supplied by the project owner: six servos on pins 7 down to 2, base first and gripper last.
- **Original sketch:** `firmware/original_sketch/original_sketch.ino` was supplied by the project owner and is included unmodified. Its original author and licence are not recorded. Its serial format (`<index> <angle>` at 9600 baud, indices 1 to 6, a mirrored servo on pin 8) is what the app's original mode implements.
- **Documentation for the interfaces used** (listed as the references the features follow):
  - Arduino `Servo` library: <https://docs.arduino.cc/libraries/servo/>
  - Web Serial API (MDN): <https://developer.mozilla.org/en-US/docs/Web/API/Web_Serial_API>
  - Gamepad API (MDN): <https://developer.mozilla.org/en-US/docs/Web/API/Gamepad_API>
  - W3C Gamepad specification, standard button and axis layout: <https://w3c.github.io/gamepad/#remapping>
- **Wiring and power guidance** (separate servo supply, common ground, decoupling capacitor) is general servo practice and is not tied to a particular source.
