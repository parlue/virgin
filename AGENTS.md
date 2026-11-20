# Virgin release workflow

When completing a new Virgin version, use the `virgin-release` skill at
`C:/Users/DirkSommerfeld/.codex/skills/virgin-release/SKILL.md` when available.
Repository: https://github.com/parlue/virgin (main).

The user requested that completed new versions also reach GitHub and that old
download packages are removed from the current repository tree. Keep source,
licenses, documentation and Git history. Keep local backups. Store distributable
archives in GitHub Releases, not as duplicate binaries in Git.

Check the version, manifest, included test reports and SHA-256 before publishing.
Publish matching source first, then upload the archive and checksum using
`tools/publish_release.py`. Verify the public download before reporting success.
Never overwrite an existing version with different bytes; stop on a conflicting
tag, upload or concurrent upstream change. A skill runs during Codex tasks; it is
not a background watcher.
