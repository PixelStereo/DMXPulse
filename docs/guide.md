# DMXPulse user guide

## Installation

Get the package matching your device from [Releases](https://github.com/PixelStereo/DMXPulse/releases). Verify `SHA256SUMS` if you need to check a download's integrity.

- **macOS:** open the DMG and drag DMXPulse to Applications, or extract the ZIP. The package includes Qt. Current automated packages are ad-hoc signed, not notarized; macOS may require you to approve opening the application in Privacy & Security.
- **Windows:** extract the whole ZIP and launch `DMXPulse.exe`. Keep the DLLs and plugin folders alongside it.
- **Linux:** install the `amd64.deb` with your graphical package installer or `sudo apt install ./DMXPulse-linux-amd64.deb`. Dependencies are installed by the package manager. A graphical desktop is required.
- **Raspberry Pi:** use 64-bit Raspberry Pi OS Bookworm with a graphical desktop and install the `arm64.deb`. This package is not for 32-bit Raspberry Pi OS.
- **Android:** install the APK after allowing installation from your chosen download source. A persistent release signing key is recommended for updates. Packages signed with an ephemeral CI test key may require uninstalling the previous package before upgrading, which clears its settings.
- **iOS:** the simulator archive is for the Intel iOS Simulator. The unsigned device archive is not directly installable. A signed IPA is provided only when release signing is configured; its permitted devices/distribution method are determined by the provisioning profile. This is not an App Store listing.

## Read a universe

Choose the correct interface, Art-Net or sACN, and a universe. Click **Start listening**, then select a source. Art-Net universe addresses are zero-based: 0–32767. sACN supports 1–63999.

Each channel has its number above and its received value below. The background follows the value from black (0) to white (255). **—** means no data has been received for the selected source and universe.

Input is marked stale after 2.5 seconds without a new packet. Last values remain visible; a stale source disappears after 60 seconds. Selecting a different universe does not convert output values into input.

Desktop users can click a channel or navigate with the arrow keys. Clicking a channel opens the **Channel** inspector. The **Sources** tab keeps sender selection separate. On mobile, the monitor uses larger scrollable tiles; **Inspect** contains both tabs.

## Prepare and send output

Select a channel, then set **Output value** with the rotary encoder or number field. Drag vertically to change the level, or scroll in steps of four. Hold Shift for fine control (one step with the wheel). Use **−10 / −1 / +1 / +10** for precise adjustments, or the **0 / 25% / 50% / 75% / Full** presets. Arrow keys adjust by one, Page Up/Down by ten, Home sets zero and End sets full. Double-click the encoder to focus exact numeric entry. **Copy input to output** copies all 512 received values. **Clear output** sets the entire output buffer to zero.

For Art-Net, enter a unicast or broadcast IPv4 destination. For sACN, the destination is the universe's multicast group; the priority control is enabled. Enable transmission to send all 512 channels approximately 30 times per second.

The receive grid changes only when a packet is received. The application's own returned stream is selected when it is first observed after transmission starts; it is labeled **DMXPulse output**. Art-Net senders on one computer are distinguished by source IP and UDP port. sACN senders are distinguished by CID and universe.

Stopping transmission does not guarantee a blackout: receivers determine their own hold/fade behavior. sACN sends three termination packets. **Clear output**, while transmission is enabled, sends zeros on the next cycle.

## Same-computer loopback

Choose the local `127.0.0.1` interface, Art-Net, and an output destination of `127.0.0.1`. Prepare values, then enable transmission. Only returned network packets populate the grid.

When using another sender on the same computer, start DMXPulse listening after that sender. Applications sharing a unicast UDP port do not necessarily each receive a copy; Windows in particular does not guarantee delivery between duplicate binds. If the sender reopens its port, restart listening. A listener for the selected adapter's specific IPv4 address helps local unicast coexist with a general broadcast listener.

## Diagnose missing input

Open **View → Network diagnostics** (or **Settings** on mobile).

- **UDP stays at zero:** check the sender's destination, adapter, port and OS local-network permission.
- **Other interface increases:** select the adapter used by the sender.
- **Unsupported increases:** traffic reached the port but is not a supported DMX data packet; discovery and control packets are not channel data.
- **Sources exist, grid is empty:** choose the source/universe you want to inspect.
- **Sent increases but input does not change:** transmission succeeded at the socket; it does not prove the destination delivered a return packet. Check routing and select the actual returning source.

Art-Net uses UDP 6454; sACN uses UDP 5568. Allow local-network access and appropriate firewall traffic. A switched network does not expose unicast packets sent exclusively to another device.

## Mobile networking

Use Wi-Fi or a supported wired network adapter on the lighting network. The application requests Android's Wi-Fi multicast lock while listening. Grant local-network access on iOS. iOS broadcast and multicast require Apple's approved multicast entitlement in the signing profile; unicast is the fallback when that entitlement is unavailable. Use the application in the foreground; background delivery/transmission is not guaranteed by mobile operating systems.

## Saved settings

Workspace settings are saved on close and when a mobile application becomes inactive. They include the adapter identity, protocol, universe, destination, priority, 512 output values, selected channel, window/panel layout and startup choices. A missing saved adapter prevents automatic network restart.

**Startup → Resume listening** is on by default. **Startup → Resume transmission** is off by default; enable it to restore the previous active transmission state as well. Output values are restored either way.

## Protocol scope

DMXPulse receives and sends ArtDmx and sACN level data (start code 0). Sources remain separate. There is no HTP/LTP merge, RDM, ArtPoll equipment inventory, sACN synchronized playback, universe-discovery scanner or IPv6 support. sACN preview/synchronized data and alternate start codes are ignored. The source list describes streams actually received by this application.
