# DMXPulse

A native Qt 6 / C++17 application for monitoring and editing network DMX on macOS, Windows and Linux.

![DMXPulse workspace with example input](docs/workspace.png)

## Workspace

- A 512-channel grayscale grid: channel number above, received value below. Black is 0; white is 255, with automatic text contrast.
- A source list that keeps each sender separate. No HTP/LTP merging.
- A channel inspector with received and output values. Click a tile or use the arrow keys to select a channel.
- A collapsible output panel with an Art-Net destination, sACN priority and transmission control.
- Optional network diagnostics in the **View** menu.

## Receiving

1. Select a network interface. **Refresh** discovers currently available IPv4 adapters.
2. Select Art-Net or sACN and a universe, then choose **Start listening**.
3. Select a detected source to inspect its universe. Art-Net numbering is zero-based (0–32767); sACN uses 1–63999.

Sources become stale after 2.5 seconds without updates and remain listed for 60 seconds. Last received values stay visible; the input badge indicates whether they are live.

Art-Net receives ArtDmx on UDP 6454. sACN receives unicast on UDP 5568 and subscribes to the selected universe multicast group. The list shows observed streams, not an inventory of all devices or universes on the network. Allow local network / UDP access in the OS firewall if needed.

### MadMapper on the same computer

Configure Art-Net unicast to `127.0.0.1` in MadMapper. Select the local `127.0.0.1 [local]` adapter in DMXPulse, then start listening **after** MadMapper. DMXPulse binds Art-Net specifically to `127.0.0.1:6454` on that adapter.

Two applications listening on one UDP port do not necessarily receive a copy of every unicast packet. If the sender reopens its socket, stop and restart listening. If port sharing fails, disable the sender's Art-Net input or use broadcast / sACN multicast appropriate to the interface. Packets sent exclusively to a different device are not visible to this application. **View → Network diagnostics** distinguishes absent traffic, other-interface traffic and unsupported packets.

## Editing and transmitting

1. Select a channel and adjust **Output value**, from 0 to 255. The slider changes only the output buffer. The grid always shows received DMX; editing, copying or clearing output never changes its values directly. To see an edit in the grid, enable transmission and receive the resulting packets on the selected source and universe.
2. **Copy input to output** copies the entire received universe into the output buffer. The buffer initially contains zeros and is not automatically merged with incoming values.
3. For Art-Net, enter a unicast or broadcast IPv4 destination. For sACN, output uses `239.255.HI.LO` for the selected universe.
4. Choose **Enable transmission** to send all 512 output values at approximately 30 Hz.

**Clear output** sets every output channel to zero. With transmission active, those values are sent at the next cycle. Stopping transmission leaves the final behavior to receivers; sACN sends three stream-termination packets.

## Saved workspace

DMXPulse saves settings automatically on close using Qt QSettings, under the `PixelStereo/DMXPulse` application namespace:

- Interface identity (stable adapter name), protocol, universe and destination address.
- Priority, all 512 output values and selected channel.
- Window geometry, source-panel width, panel visibility and diagnostics menu state.
- Listening / transmitting state and startup preferences.

**Startup → Resume listening** is enabled by default and resumes input if it was active when the application closed. **Startup → Resume transmission** is disabled by default; enable it to also restore active transmission on reopening. Output values are restored either way. If a saved adapter is unavailable, network activity does not restart automatically; select an available adapter first.

## Build

Requirements: CMake 3.21+, Qt 6.4+ with Widgets and Network (plus Test when tests are enabled), and a C++17 compiler. Use a Qt build matching the target platform and compiler.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/path/to/Qt/6.x/platform
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

- **macOS:** `build/DMXPulse.app`. For redistribution, bundle dependencies with `macdeployqt build/DMXPulse.app`, then sign / notarize as appropriate.
- **Windows:** use an MSVC developer terminal and matching Qt MSVC build. With a multi-configuration generator, run `build/Release/DMXPulse.exe`. Bundle dependencies with `windeployqt build/Release/DMXPulse.exe`.
- **Linux:** run `build/DMXPulse` with matching Qt runtime packages installed, or prepare a package for the target distribution.

Use `-DBUILD_TESTING=OFF` to build the application without QtTest.

The local application supplied alongside the source uses the Qt installation on the build machine; it is not a standalone distribution package.

## Validation and scope

QtTest checks packet round trips, universe limits, 512-channel data, truncated / malformed packets, local Art-Net and sACN reception, grayscale rendering data, channel editing, output loopback, a competing local listener, and settings restoration including automatic input restart. Network tests use isolated ports and temporary settings files.

Compilation and tests have been run on macOS Apple Silicon with Qt 6.12. Windows/Linux compilation and physical lighting-network validation remain to be done. Android and iOS are possible Qt targets, but a touch layout, mobile networking permissions, packaging and device tests are not implemented yet.

Not implemented: ArtPoll device discovery, RDM, HTP/LTP merging, sACN synchronization, universe discovery and IPv6. Synchronized sACN, preview packets and nonzero DMX start codes are ignored. Duplicate / old sequences are filtered.

See [VALIDATION.md](VALIDATION.md) for the review, regression coverage and sanitizer commands.

## Protocol references

Art-Net™ Designed by and Copyright Artistic Licence.

- [Art-Net developer documentation](https://artisticlicence.com/pages/data-and-dmx-support-documents)
- [ANSI E1.31-2018](https://tsp.esta.org/tsp/documents/docs/E1-31-2018.pdf)

The receiver also accepts this application’s own sACN data when it is delivered back through the network. A sACN sender has its own source identity: select that source to inspect its returned packets. No UI values are injected into the receive grid.
