<p align="center"><img src="docs/mark.svg" width="84" alt="DMXPulse"></p>
<h1 align="center">DMXPulse</h1>
<p align="center"><strong>See the signal. Find the problem. Take control.</strong></p>
<p align="center">A clear view of your Art-Net and sACN lighting network.<br>512 channels. Live values. No guesswork.</p>
<p align="center">
  <a href="#your-first-signal">Quick start</a> ·
  <a href="docs/guide.md">User guide</a> ·
  <a href="docs/build.md">Build from source</a>
</p>

<!-- release-downloads:start -->
**LATEST VERSION · [v0.3.1](https://github.com/PixelStereo/DMXPulse/releases/tag/v0.3.1)**

[![macOS Apple Silicon](https://img.shields.io/badge/macOS-Apple_Silicon-6b7787?style=for-the-badge&logo=apple&logoColor=white&labelColor=1a1c20)](https://github.com/PixelStereo/DMXPulse/releases/download/v0.3.1/DMXPulse-macos-arm64-v0.3.1.zip)&nbsp;&nbsp;[![macOS Intel](https://img.shields.io/badge/macOS-Intel-515b69?style=for-the-badge&logo=apple&logoColor=white&labelColor=1a1c20)](https://github.com/PixelStereo/DMXPulse/releases/download/v0.3.1/DMXPulse-macos-x64-v0.3.1.zip)&nbsp;&nbsp;[![Windows Installer](https://img.shields.io/badge/Windows-Installer-2b6fb6?style=for-the-badge&logo=windows&logoColor=white&labelColor=1a1c20)](https://github.com/PixelStereo/DMXPulse/releases/download/v0.3.1/DMXPulse-windows-x64-v0.3.1-Setup.exe)&nbsp;&nbsp;[![Linux AppImage](https://img.shields.io/badge/Linux-Debian_package-b8821c?style=for-the-badge&logo=linux&logoColor=white&labelColor=1a1c20)](https://github.com/PixelStereo/DMXPulse/releases/download/v0.3.1/DMXPulse-linux-x64-v0.3.1.deb)&nbsp;&nbsp;[![Raspberry Pi AppImage](https://img.shields.io/badge/Raspberry_Pi-Debian_package-a22846?style=for-the-badge&logo=raspberrypi&logoColor=white&labelColor=1a1c20)](https://github.com/PixelStereo/DMXPulse/releases/download/v0.3.1/DMXPulse-linux-arm64-v0.3.1.deb)
<!-- release-downloads:end -->

<p align="center">
  <a href="https://github.com/PixelStereo/DMXPulse/actions/workflows/release.yml"><img alt="Build and release" src="https://github.com/PixelStereo/DMXPulse/actions/workflows/release.yml/badge.svg"></a>
  <img alt="License AGPL-3.0" src="https://img.shields.io/badge/license-AGPL--3.0-8aa99a">
  <img alt="Protocols Art-Net and sACN" src="https://img.shields.io/badge/Art--Net%20%C2%B7%20sACN-network%20DMX-333a37">
</p>

![DMXPulse — live universe, grayscale levels, and the channel editor comparing network and output](docs/workspace.png)

## The whole universe, at a glance

Read a channel number and its value without opening another window. Every tile follows the actual received level: **black at 0, white at 255**, with text that stays readable across the range.

Switch on **Active channels only** to focus on received levels above zero. The view keeps the original DMX addresses, updates as levels change, and remembers your preference.

| Monitor | Inspect | Send |
|:---|:---|:---|
| See all 512 channels in one grayscale view. | Click a channel to compare what arrives with what you send. | Shift-click to edit a whole range at once: dial, fine steps or an exact value. |
| Choose an interface, protocol and universe. | Merge received sources with HTP or LTP. | Follow input and override individual channels. |
| Spot live, stale and missing input immediately. | Open network diagnostics when you need more detail. | Enable transmission explicitly, at approximately 30 Hz. |

**The grid tells the truth.** It only shows data received through the network. Setting a channel's output level in its editor prepares the output; it changes the grid only when the resulting packets are actually received.

## Take it where you work

Packages are attached to [tagged releases](https://github.com/PixelStereo/DMXPulse/releases).

| Device | Package |
|:---|:---|
| Mac · Apple Silicon or Intel | One `.zip` per architecture, with the Qt runtime included |
| Windows · 64-bit | `-Setup.exe` installer (no administrator rights), with runtime dependencies |
| Linux · Debian 12 / compatible Ubuntu desktop | `amd64.deb` |
| Raspberry Pi · 64-bit Raspberry Pi OS Bookworm desktop | `arm64.deb` |

macOS packages use an ad-hoc signature unless a distribution signing pipeline is added. See the release notes and [installation guide](docs/guide.md#installation) before installing.

## Your first signal

1. **Choose your interface.** Select the adapter connected to your lighting network.
2. **Choose a protocol and universe.** Art-Net starts at universe **0**; sACN starts at **1**.
3. **Start listening.** A sole incoming universe is selected automatically; otherwise, select the universe to inspect.

For a same-device loopback test, choose the local `127.0.0.1` adapter and use `127.0.0.1` as the Art-Net output destination. Click a channel to prepare its value, then enable transmission. A returned packet is required before that value appears in the grid.

## Pick up where you left off

Your adapter, universe, addresses, levels, selected channel and workspace layout are saved automatically. Startup options let you resume listening and optionally transmission. The universe list and the monitor share one workspace; click a channel to set its output level. On phones and tablets, the panels stack vertically.

## Small details that matter

- One view per universe, with a saved HTP/LTP choice for each universe and sACN priority handling.
- A detachable timestamped journal identifies incoming sources and outgoing packets.
- No packet means **—**, rather than a value that looks like a received zero.
- Stale input is clearly marked while the last values remain visible.
- Multicast reception follows the selected sACN universe.
- Output and reception remain independent, even during local loopback.

Need a hand? Read the [user guide](docs/guide.md) or [report an issue](https://github.com/PixelStereo/DMXPulse/issues). Developers and release maintainers can find everything in [docs/build.md](docs/build.md).

---

Made by **PixelStereo** · [AGPL-3.0 license](LICENSE) · [Third-party notices](THIRD-PARTY-NOTICES.md)

Art-Net™ Designed by and Copyright Artistic Licence.
