# SpaceVue

SpaceVue is a Nintendo Switch homebrew app for quickly checking storage usage on the console. It shows free, used, and total capacity for the internal NAND user partition and the microSD card in a clean dashboard view.

## Features

- Internal NAND and microSD storage overview
- Free, used, and total space shown in GiB
- Usage progress bars for each storage device
- Combined capacity summary
- Storage health labels:
  - `OK`
  - `LOW`
  - `CRITICAL`
  - `UNAVAILABLE`
- Manual and automatic refresh
- Switchable accent themes

## Installation

1. Open the GitHub repository page for SpaceVue.
2. Go to the **Releases** tab.
3. Download the latest release.
4. If the release is packaged as a `.zip`, extract it.
5. Copy `SpaceVue.nro` to your Nintendo Switch SD card:

```text
/switch/SpaceVue.nro
```

6. Put the SD card back into your Switch.
7. Open the Homebrew Menu.
8. Launch **SpaceVue**.

## Controls

| Button | Action |
| --- | --- |
| `A` | Refresh storage values |
| `-` | Change accent theme |
| `+` | Exit |

SpaceVue also refreshes storage values automatically while it is open.

## What The App Shows

SpaceVue displays two main storage panels:

- **Internal NAND**: the Switch internal user storage partition
- **microSD**: the inserted SD card storage

Each panel shows:

- available free space
- total capacity
- used space
- used percentage
- storage health state

The footer shows combined capacity, free space, and used space across all available storage devices.

## Building From Source

Most users should install SpaceVue from the latest GitHub release. Building from source is only needed if you want to modify the app or create your own build.

### Requirements

- devkitPro
- devkitA64
- libnx
- SDL2 for Switch
- SDL2_ttf for Switch
- `make`

Install the required Switch development packages using the official devkitPro setup for your platform.

### Build

From the project root, run:

```sh
make
```

The build output is written to:

```text
out/<version>/SpaceVue.nro
```

For the current source version, that is:

```text
out/3.0.1/SpaceVue.nro
```

To clean generated build files:

```sh
make clean
```

## Project Structure

```text
source/        C++ source code
romfs/         Runtime assets embedded into the NRO
data/          Binary font data used by the build
icon.jpg       Homebrew Menu icon
Makefile       devkitPro/libnx build configuration
```

## Notes

- Space values are displayed in GiB.
- If a storage device cannot be mounted or read, SpaceVue shows an unavailable or error state instead of guessing.
- The app is intended for Nintendo Switch homebrew environments.

## Credits

SpaceVue is made by **estyxq**.
