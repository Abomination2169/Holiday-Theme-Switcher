# Holiday Theme Switcher

Switch between seasonal Beat Saber menu environments instantly, or let the calendar select a theme automatically.

**Experimental compatibility:** Meta Quest 3 · Beat Saber **1.45.3_30054** · Quest QMOD `seasonalthemes` · **v1.0.0-1453-dev.1**

## Themes

- **Default 🎮** — Beat Saber's normal menu appearance and soundtrack.
- **Halloween 🎃** — spooky lighting, pumpkins, falling autumn leaves, and themed menu music.
- **Thanksgiving 🦃** — golden harvest decorations, falling leaves, and occasional turkey gobbles.
- **Christmas Eve ❄️** — layered snowfall, snowdrifts, a mountain backdrop, and a results-screen aurora.
- **New Years 🥂** — midnight lighting, crystal ball, confetti, fireworks, and a menu track selected from the user's installed custom songs.
- **Dynamic 📅** — chooses the appropriate theme using the headset's local date.

### Dynamic calendar

| Dates | Theme |
| --- | --- |
| October 1–31 | Halloween |
| November 1–30 | Thanksgiving |
| December 1–30 | Christmas Eve ❄️ |
| December 31–January 31 | New Years |
| February 1–September 30 | Default |

Manual selections stay selected until changed. A saved original Christmas selection opens Christmas Eve. Dynamic mode rechecks the calendar while outside gameplay.

## 1.45.3 test notes

Unity 6 may skip unavailable legacy particle settings. Check snowfall, autumn leaves, and confetti on Quest, especially when switching themes. Menu floor color restoration runs in `LateUpdate` on this branch. Check the floor after returning from a map with custom lighting.

## Installation and use

1. Download the **experimental 1.45.3 QMOD** from this branch’s successful GitHub Actions build.
2. Install it using your normal Quest Beat Saber mod installer for **1.45.3_30054**.
3. **Disable standalone Halloween, Thanksgiving, Christmas, Christmas Eve, and New Year's theme mods** before using the switcher. They can compete for the same menu effects and audio.
4. Open **Holiday Theme Switcher** from the Beat Saber main menu, then choose a theme or **Dynamic**.

The selection is stored at `/sdcard/ModData/com.beatgames.beatsaber/Mods/SeasonalThemes/theme.txt`. A fresh installation with no saved selection currently starts in Halloween mode; choose Default or Dynamic to change it.

New Year's music uses **existing user-installed custom songs**; no custom-song audio is included. The confetti and fireworks can remain inactive on the song-selection screen after gameplay and resume on returning to the main menu. This is an accepted behavior for this release.

## Source, licensing and credits

The combined source code is released under **GNU GPLv3 (GPL-3.0-only)**. Read [LICENSE](LICENSE) and [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for third-party code, assets, audio provenance, attribution, and outstanding verification items.

The project incorporates work derived from [Christmas Theme](https://github.com/vcmikuu/ChristmasTheme) by **laynia (vcmikuu)** and **CatsaCode**, with [GPLv3 reuse authorization and credit requirements](https://github.com/vcmikuu/ChristmasTheme/issues/1#issuecomment-6711835142), alongside [Halloween Theme](https://github.com/Abomination2169/Halloween-Theme), [Thanksgiving Theme](https://github.com/Abomination2169/Thanksgiving-Theme), and [New Year's Theme](https://github.com/Abomination2169/New-Years-Theme). Seasonal adaptations and switcher integration by **Abomination2169**, developed with AI assistance in 2026. This is not an official upstream Christmas Theme release.

**For source corresponding to a release QMOD, use its exact release tag/commit and workflow**, not an unrelated branch. The workflow retrieves assets from pinned upstream commits at build time.

This mod is for Beat Saber on Quest and is not affiliated with Beat Games or Meta.
