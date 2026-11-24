# Third-party notices — Holiday Theme Switcher v1.0.0

## Source code and derivative work

The source code is distributed under the **GNU General Public License version 3 only (GPL-3.0-only)**; the complete license is in [LICENSE](LICENSE).

- **CatsaCode** and **laynia (vcmikuu)**: creators of the original [Christmas Theme](https://github.com/vcmikuu/ChristmasTheme). They [confirmed reuse and modification under GPLv3 with appropriate credit](https://github.com/vcmikuu/ChristmasTheme/issues/1#issuecomment-6711835142). This switcher is an independent derivative and is not endorsed by the upstream authors.
- **Abomination2169**: Halloween, Thanksgiving, New Year's adaptations and unified theme-switching work, developed with AI assistance in 2026. Relevant standalone repositories: [Halloween](https://github.com/Abomination2169/Halloween-Theme), [Thanksgiving](https://github.com/Abomination2169/Thanksgiving-Theme), [New Year's](https://github.com/Abomination2169/New-Years-Theme).
- Build scaffolding adapted from [Quest Mod Template](https://github.com/Lauriethefish/quest-mod-template), credited to **Lauriethefish**, **danrouse**, and **Bobby Shmurner** (Unlicense). The project uses QPM, beatsaber-hook, Scotland2, Paper2, BSML, custom-types and other separately licensed dependencies; their licenses remain with their respective projects.

## Audio bundled into the QMOD

- **Halloween:** `Ghostly_Horror_Menu.ogg`, converted from **“Ghostly Horror Music for Haunted House Scenes” by Alex Morgan**, licensed **CC BY 4.0**. Attribution and license: [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/); see [Halloween Theme notices](https://github.com/Abomination2169/Halloween-Theme/blob/main/THIRD_PARTY_NOTICES.md). The recording was converted to OGG.
- **Thanksgiving:** `Thanksgiving-Gobble-Quiet-Phrase.ogg`, edited from **Gobbler.ogg** by **bod**, a public-domain recording originally from PDSounds.org, archived at [Wikimedia Commons](https://commons.wikimedia.org/wiki/File:Gobbler.ogg). See [Thanksgiving Theme notices](https://github.com/Abomination2169/Thanksgiving-Theme/blob/main/THIRD_PARTY_NOTICES.md).
- **Christmas Eve:** See the [Christmas Eve audio credits](https://github.com/Abomination2169/Christmas-Eve-Theme/blob/28c341d4661e97f0424f0cd754da746f3a7c58cf/MUSIC_LICENSES.md) for bundled audio attribution and provenance. The maintainer confirmed redistribution permission for the HD-Studio and Christmas Lights menu recordings in this release.
- **New Year's:** the mod reads user-installed custom-song audio from SongCore/SongLoader folders at runtime. It does **not** redistribute the user's custom songs.

## Visual and environment assets

- Halloween pumpkin artwork and Thanksgiving pumpkin/harvest artwork are fetched from the corresponding Abomination2169 standalone theme repositories. The bundled autumn leaf sprite is part of this repository. See the standalone repositories for their asset histories.
- Christmas Eve assets, including winter artwork, are sourced from the Christmas Eve release source pinned below.
- New Year's `new_year_crystal_ball.png` and `confetti_streamers.png` are fetched from [New Year's Theme](https://github.com/Abomination2169/New-Years-Theme) at the commit corresponding to its v1.0.0 tag; created for that project with AI assistance.
- The implementation may use Beat Saber's existing runtime visual effects, but does not package the game's original proprietary soundtracks or custom-song library.

## Build-time asset provenance

The GitHub Actions workflow downloads the above audio and artwork during the build. **A source checkout alone does not contain every binary input needed to reproduce a QMOD**. The release-candidate workflow pins upstream Git commit refs; archive the SHA-256 hashes of downloaded inputs for each published release. See [RELEASE_AUDIT.md](RELEASE_AUDIT.md).

### Pinned source revisions for v1.0.0 candidate

- Halloween Theme: `14c5220b042e6ac84f37978ca06aeb6dd3dd9c1f`
- Thanksgiving Theme: `d30af27ff583e86a69eae8b8fa136c5432fbae34`
- Christmas Eve Theme: `28c341d4661e97f0424f0cd754da746f3a7c58cf`
- New Year's Theme: `9509a3efa1cc4ffe0bbab5c5e2f44e91337ed9b5`
