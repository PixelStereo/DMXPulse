# DMXPulse user guide

## Installation

Get the package matching your device from [Releases](https://github.com/PixelStereo/DMXPulse/releases).

- **macOS:** download the DMG for Apple Silicon (`arm64`) or Intel (`x86_64`), open it, and drag DMXPulse to Applications. The package includes Qt. Current automated packages are ad-hoc signed, not notarized; macOS may require you to approve opening the application in Privacy & Security.
- **Windows:** extract the whole ZIP and launch `DMXPulse.exe`. Keep the DLLs and plugin folders alongside it.
- **Linux:** install the `amd64.deb` with your graphical package installer or `sudo apt install ./DMXPulse-linux-amd64.deb`. Dependencies are installed by the package manager. A graphical desktop is required.
- **Raspberry Pi:** use 64-bit Raspberry Pi OS Bookworm with a graphical desktop and install the `arm64.deb`. This package is not for 32-bit Raspberry Pi OS.
- **Android:** install the APK after allowing installation from your chosen download source. A persistent release signing key is recommended for updates. Packages signed with an ephemeral CI test key may require uninstalling the previous package before upgrading, which clears its settings.
- **iOS:** download the device IPA. `DMXPulse-ios-unsigned.ipa` contains the app under `Payload/DMXPulse.app` and must be signed/provisioned before installation; changing the archive format alone does not make it installable. When Apple signing is configured, the release contains `DMXPulse-ios.ipa` instead, with installation determined by its provisioning profile. Simulator builds are used only for CI tests and are not released. This is not an App Store listing.

## Read a universe

Choose the correct interface, Art-Net or sACN, and a universe. Click **Start listening**, then select a detected universe. When exactly one live universe is detected after listening starts, it is selected and displayed automatically, including its protocol. Later arrivals do not replace that view. Choosing a universe manually or enabling transmission keeps your selection stable. Art-Net universe addresses are zero-based: 0–32767. sACN supports 1–63999.

Each channel has its number above and its received value below. The background follows the value from black (0) to white (255). **—** means no data has been received for the selected protocol and universe.

Input is marked stale after 2.5 seconds without a new packet. Last values remain visible; a stale source disappears after 60 seconds. Selecting a different universe does not convert output values into input.

Desktop users can click a channel or navigate with the arrow keys. The universe list stays on the left and the output encoder and transmission controls stay on the right. There are no workspace tabs. On smaller mobile screens, the panels stack vertically in a scrollable workspace.

## One view per universe

Each protocol/universe pair appears once, regardless of how many senders are present. The source count includes DMXPulse's own output only when it is actually received. Hover over a universe to see its senders.

Choose the receive merge mode above the grid. The choice is saved **per universe and protocol**: Art-Net universe 1 and sACN universe 1 can use different modes. Switching universes restores its own choice; new universes default to HTP. On upgrading from a global merge setting, that setting is retained for the previously selected universe only.

Available modes:

- **HTP** (default): the highest received value wins for each channel. An output edit below another source's level will therefore not lower the displayed result.
- **LTP**: the last source to **change** a channel owns that channel. Repeated packets with unchanged values do not take ownership. A source's first packet establishes initial values for all channels. Arrival order breaks simultaneous changes.

For sACN, only live sources with the highest per-universe priority contribute, in either mode. Art-Net and sACN remain separate universes. Sources stop contributing after 2.5 seconds; the most recent merged result is retained and marked stale when none remain. Streams are retained in the list for 60 seconds. The application supports up to 2,048 concurrent received streams across all universes. Per-slot sACN priorities are not supported.

## Network log

**View → Network log** opens a dockable journal. Drag its title bar to detach it into a separate window or dock it on the side. Each accepted input frame has a local timestamp with milliseconds, protocol, universe, sender address/port, sequence and a summary of changed channels. sACN entries also include source name, CID and priority. Transmission entries show the destination; a successful send is distinct from receiving its return.

The journal keeps the latest **2,000 entries** in memory. Channel summaries show the first eight changes and the total count when more channels change. **Pause** stops recording new entries; **Clear** empties the journal; **Save…** exports the retained entries. This is a diagnostic journal, not a lossless packet capture. Its docked/floating position and visibility are restored on launch.

## Prepare and send output

Select a channel, then set **Output value** with the rotary encoder or number field. Drag vertically to change the level, or scroll in steps of four. Hold Shift for fine control (one step with the wheel). Use **−10 / −1 / +1 / +10** for precise adjustments, or the **0 / 25% / 50% / 75% / Full** presets. Arrow keys adjust by one, Page Up/Down by ten, Home sets zero and End sets full. Double-click the encoder to focus exact numeric entry. With **Follow received input** enabled, unedited channels follow the external received signal using the selected merge mode. Each edited channel overrides that baseline until **Release channel overrides** is pressed. DMXPulse's returned output never feeds this baseline, which prevents a self-feedback loop. Turn following off to use a manual output buffer. **Zero all channels** overrides the entire output buffer with zeros.

For Art-Net, enter a unicast or broadcast IPv4 destination. For sACN, the destination is the universe's multicast group; the priority control is enabled. Enable transmission to send all 512 channels approximately 30 times per second.

The receive grid changes only when a packet is received. The application's own returned stream is labeled **DMXPulse output** in the journal and contributes to the same universe view using the selected HTP/LTP mode. Art-Net senders on one computer are distinguished by source IP and UDP port. sACN senders are distinguished by CID and universe.

Stopping transmission does not guarantee a blackout: receivers determine their own hold/fade behavior. sACN sends three termination packets. **Zero all channels**, while transmission is enabled, sends zeros on the next cycle. In HTP mode another source may keep the merged received level above zero.

## Same-computer loopback

Choose the local `127.0.0.1` interface, Art-Net, and an output destination of `127.0.0.1`. Prepare values, then enable transmission. Only returned network packets populate the grid.

When using another sender on the same computer, start DMXPulse listening after that sender. Applications sharing a unicast UDP port do not necessarily each receive a copy; Windows in particular does not guarantee delivery between duplicate binds. If the sender reopens its port, restart listening. A listener for the selected adapter's specific IPv4 address helps local unicast coexist with a general broadcast listener.

## Diagnose missing input

Open **View → Network diagnostics** from the application menu.

- **UDP stays at zero:** check the sender's destination, adapter, port and OS local-network permission.
- **Other interface increases:** select the adapter used by the sender.
- **Unsupported increases:** traffic reached the port but is not a supported DMX data packet; discovery and control packets are not channel data.
- **Sources exist, grid is empty:** choose the protocol/universe you want to inspect.
- **Sent increases but input does not change:** transmission succeeded at the socket; it does not prove the destination delivered a return packet. Check routing and inspect the returning source in the network log.

Art-Net uses UDP 6454; sACN uses UDP 5568. Allow local-network access and appropriate firewall traffic. A switched network does not expose unicast packets sent exclusively to another device.

## Mobile networking

Use Wi-Fi or a supported wired network adapter on the lighting network. The application requests Android's Wi-Fi multicast lock while listening. Grant local-network access on iOS. iOS broadcast and multicast require Apple's approved multicast entitlement in the signing profile; unicast is the fallback when that entitlement is unavailable. Use the application in the foreground; background delivery/transmission is not guaranteed by mobile operating systems.

## Saved settings

Workspace settings are saved on close and when a mobile application becomes inactive. They include the adapter identity, protocol, universe, destination, priority, 512 output values and channel overrides, input following, merge mode, selected channel, window/panel layout, journal position and startup choices. A missing saved adapter prevents automatic network restart.

**Startup → Resume listening** is on by default. **Startup → Resume transmission** is off by default; enable it to restore the previous active transmission state as well. Output values are restored either way.

## Protocol scope

DMXPulse receives and sends ArtDmx and sACN level data (start code 0). It merges received levels with HTP or per-channel change-based LTP. There is no RDM, ArtPoll equipment inventory, sACN synchronized playback, universe-discovery scanner or IPv6 support. sACN preview/synchronized data and alternate start codes are ignored. The universe list and journal describe streams actually received by this application. Art-Net source identity uses IP and UDP port to distinguish applications on the same host; this is a monitor convention, rather than Art-Net node discovery/Physical-port identity.
