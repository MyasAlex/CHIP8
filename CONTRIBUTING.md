# Contributing

Bug reports, compatibility findings and focused pull requests are welcome. English and Russian reports are both fine.

For a bug, include the application version, Windows version, ROM name or legal source link, compatibility profile, instruction speed, display mode, steps to reproduce and expected behavior. Avoid uploading ROMs that you cannot redistribute.

Before changing the core, read [BUILDING.md](docs/BUILDING.md). Keep guest state separate from presentation heuristics and keep timers at 60 Hz. Run CTest; CPU or compatibility changes should include a focused regression check when the behavior is not already covered. UI and documentation fixes can use a relevant manual check.

Submit a pull request with the concrete problem, resulting behavior and validation. Keep unrelated refactors separate so changes are easy to review. Check the licenses in [THIRD_PARTY.md](THIRD_PARTY.md) before modifying external assets.
