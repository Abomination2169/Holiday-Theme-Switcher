# Holiday Theme Switcher — release candidate verification

Target: **Beat Saber 1.40.8_7379 on Quest 3**. Install the QMOD produced by the matching release-candidate build; disable the standalone seasonal mods.

## Headset smoke test

1. Start Beat Saber, press Continue, and open the main menu.
2. Open Holiday Theme Switcher and verify all six choices: Default, Halloween, Thanksgiving, Christmas Eve, New Years, Dynamic.
3. Switch between themes and verify that decorations, lighting, logo treatment, and music change without requiring a restart.
4. On each theme, interrupt menu audio with a song preview; after the preview, verify the prior menu soundtrack resumes at its previous position. Thanksgiving's gobble effect is separate.
5. Start an official map on Default and a custom map on a holiday theme. Return to the menu and check for crashes, missing menu music, and unintended effects during gameplay.
6. Verify Dynamic's date schedule, and that a manual choice remains manual.
7. For New Years, verify fireworks/confetti in the main menu; after a song they may remain absent on song selection until returning to the main menu (accepted for v1.0.0).
8. Check that standalone seasonal mods are not enabled concurrently.

## Release gate

A passing CI build validates compilation and package structure, **not** all headset behavior or third-party audio rights. Do not publish until the remaining items in [RELEASE_AUDIT.md](RELEASE_AUDIT.md) have been resolved or explicitly accepted by the maintainer.
