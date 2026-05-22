# Packaging Notes for Downstream Distributors

This document is the upstream-to-packager interface. It records every change
on `main` that downstream packaging recipes (`.spec`, `PKGBUILD`, `debian/*`,
Flatpak manifest, etc.) need to react to.

The project is currently packaged in four OBS namespaces under
[`home:AndnoVember`](https://build.opensuse.org/project/show/home:AndnoVember):

| Distro    | OBS project                                                                              |
| --------- | ---------------------------------------------------------------------------------------- |
| Arch      | [home:AndnoVember:Arch/sudoku-cpp](https://build.opensuse.org/package/show/home:AndnoVember:Arch/sudoku-cpp) |
| Fedora    | [home:AndnoVember:Fedora/sudoku-cpp](https://build.opensuse.org/package/show/home:AndnoVember:Fedora/sudoku-cpp) |
| LXQt:Qt6  | [home:AndnoVember:LXQt:Qt6/sudoku-cpp](https://build.opensuse.org/package/show/home:AndnoVember:LXQt:Qt6/sudoku-cpp) |
| Debian    | [home:AndnoVember:Debian/sudoku-cpp](https://build.opensuse.org/package/show/home:AndnoVember:Debian/sudoku-cpp) |

A Flathub submission is being staged but not yet published; see the
[Flathub-prep notes](#flathub-prep-status) below.

---

## Install tree (current, post-rename)

What `make install` lands under `${CMAKE_INSTALL_PREFIX}` (typically `/usr`):

| Path                                                                          | Source                                                          |
| ----------------------------------------------------------------------------- | --------------------------------------------------------------- |
| `bin/sudoku`                                                                  | the executable (target name unchanged)                          |
| `share/sudoku/translations/sudoku_<code>.qm`                                  | compiled translation catalogs (one per `.ts` file)              |
| `share/applications/io.github.darkstar79.Sudoku_CPP.desktop`                  | [resources/linux/...](../resources/linux/io.github.darkstar79.Sudoku_CPP.desktop) |
| `share/metainfo/io.github.darkstar79.Sudoku_CPP.metainfo.xml`                 | [resources/linux/...](../resources/linux/io.github.darkstar79.Sudoku_CPP.metainfo.xml) |
| `share/icons/hicolor/scalable/apps/io.github.darkstar79.Sudoku_CPP.svg`       | [resources/icons/...](../resources/icons/io.github.darkstar79.Sudoku_CPP.svg) |

The Linux install rules live in [CMakeLists.txt](../CMakeLists.txt#L262-L274).
The reverse-DNS app ID (`io.github.darkstar79.Sudoku_CPP`) is the only
non-trivial filename in the tree; everything else is named `sudoku*`.

---

## Active migrations

### App-ID rename — `org.sudoku_cpp.Sudoku` → `io.github.darkstar79.Sudoku_CPP`

Landed on `main` as part of the Flathub-prep PR. Touches three installed
filenames and the `Icon=` value inside the `.desktop`.

**What changed at the install-tree level:**

| Artifact            | Old install path                                                            | New install path                                                                                  |
| ------------------- | --------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------- |
| `.desktop`          | `share/applications/org.sudoku_cpp.Sudoku.desktop`                          | `share/applications/io.github.darkstar79.Sudoku_CPP.desktop`                                      |
| AppStream metainfo  | `share/metainfo/org.sudoku_cpp.Sudoku.metainfo.xml`                         | `share/metainfo/io.github.darkstar79.Sudoku_CPP.metainfo.xml`                                     |
| Scalable icon (SVG) | `share/icons/hicolor/scalable/apps/org.sudoku_cpp.Sudoku.svg`               | `share/icons/hicolor/scalable/apps/io.github.darkstar79.Sudoku_CPP.svg`                           |

**Plus**, inside the `.desktop` file, the `Icon=` line moved from
`Icon=org.sudoku_cpp.Sudoku` to `Icon=io.github.darkstar79.Sudoku_CPP`.

**What did *not* change:** the executable name (`sudoku`), its install path
(`bin/sudoku`), the binary's expected runtime data dir (`share/sudoku/`),
the upstream tarball name, and the upstream project name in `CMakeLists.txt`
(`project(sudoku VERSION ...)`).

**Action checklist per distro:**

- [ ] **Fedora / LXQt:Qt6 `.spec`** — update the three filenames in the
      `%files` section. If the spec patches or asserts on the `.desktop`
      `Icon=` value, update the patch/assertion.
- [ ] **Arch `PKGBUILD`** — same three filenames in the `package()` install
      step (if listed explicitly) or in any `install -Dm644` lines.
- [ ] **Debian `debian/install` / `debian/*.install`** — update path globs.
      If `debian/*.lintian-overrides` references the old ID, rename.
- [ ] **All distros** — re-validate post-build:
      ```sh
      appstreamcli validate /usr/share/metainfo/io.github.darkstar79.Sudoku_CPP.metainfo.xml
      desktop-file-validate /usr/share/applications/io.github.darkstar79.Sudoku_CPP.desktop
      ```

The rename is driven by Flathub's
[reverse-DNS naming convention](https://docs.flathub.org/docs/for-app-authors/requirements/#correct-application-id)
for GitHub-hosted projects without a custom domain. It is a one-shot change;
the new ID is final.

---

## Recent packager-relevant changes

Newest first. Each entry links the commit that introduced the change and
the upstream `CHANGELOG.md` line.

### 2026-05 — App-ID rename + metainfo overhaul

- Commits: [`24921d9`](https://github.com/darkstar79/sudoku-cpp/commit/24921d9) (rename), [`eaf5f77`](https://github.com/darkstar79/sudoku-cpp/commit/eaf5f77) (metainfo sandbox + source switch), plus follow-ups on the same branch.
- CHANGELOG: see `[Unreleased] → Changed → Reverse-DNS app ID`.
- Action: see [App-ID rename](#app-id-rename--orgsudoku_cppsudoku--iogithubdarkstar79sudoku_cpp) above.

### 2026-05 — Bundled-dep bumps (Flatpak + Conan only)

- libsodium `1.0.18` → `1.0.21`, yaml-cpp `0.8.0` → `0.9.0`.
- **Distro packagers using system libs are unaffected.** The bumps only
  touch [conanfile.py](../conanfile.py) (Windows build) and the Flatpak
  module set in [flatpak/io.github.darkstar79.Sudoku_CPP.yml](../flatpak/io.github.darkstar79.Sudoku_CPP.yml).
  Linux distro builds use system `libsodium-devel` / `libyaml-cpp-devel`
  and pick up whatever version the distro ships.
- Action: none for `.spec` / `PKGBUILD` / `debian/control`.

### 2026-05 — Dynamic locale discovery (no C++ change required to add a language)

- Commit: [`3bbcf63`](https://github.com/darkstar79/sudoku-cpp/commit/3bbcf63) (`feat(i18n): dynamic locale discovery + BCP 47 validation`).
- CHANGELOG: `[Unreleased] → Changed → Language selector`.
- **What changed:** the Settings → Language combo used to read from a
  hardcoded `{en, de}` map in C++ source. It now scans
  `${CMAKE_INSTALL_DATADIR}/sudoku/translations/` at runtime for
  `sudoku_<code>.qm` files and offers every locale found. Locale codes
  are validated against a BCP 47 subset (`^[a-z]{2,3}(_[A-Z]{2})?$`)
  before being interpolated into a filesystem path, so a malformed code
  can't escape the translations directory.
- **Implication for distro patches:** if a downstream patch added an
  additional locale by inserting into the old hardcoded map, that patch
  is now obsolete — drop it and ship the `.qm` file alone. The combo will
  pick it up automatically.
- **Adding a new locale from a distro patch** (e.g., a translation
  contributed downstream that hasn't reached upstream yet):
  1. Drop the translated `sudoku_<code>.ts` into `resources/translations/`.
  2. Add the path to the `SUDOKU_TS_FILES` list in
     [CMakeLists.txt:203-206](../CMakeLists.txt#L203-L206).
  3. Build — `qt_add_lrelease` produces the `.qm`, the install rule at
     [CMakeLists.txt:266-267](../CMakeLists.txt#L266-L267) ships it.
  4. No C++ change. No view-layer registration.

  Coordinate upstreaming via [docs/TRANSLATIONS.md](TRANSLATIONS.md).

---

## Flathub-prep status

Flathub submission is **staged but not active**. Two artifacts in-tree are
deliberately set to "pre-submission" state:

- [flatpak/io.github.darkstar79.Sudoku_CPP.yml](../flatpak/io.github.darkstar79.Sudoku_CPP.yml)
  uses `sources: [{type: dir, path: ..}]` (local working copy). The
  Flathub-required `type: git` block is present but commented; it will be
  uncommented and pointed at a release tag when submission happens.
- The `<screenshots>` URLs in the metainfo point at `raw.githubusercontent.com/.../main/...`.
  They will be re-pinned to a tag ref at submission.
- The `<release version="1.0.0" date="2026-03-20">` in the metainfo is a
  placeholder; the date will be corrected when the `1.0.0` tag is cut.

None of these affect distro packagers — they all concern the Flatpak / Flathub
path only.

---

## Anticipated future changes

Listed so distro recipes can plan, not because anything is scheduled.

### Repo rename: `sudoku_cpp` → `sudoku`

Upstream intends to rename the GitHub repository from `darkstar79/sudoku-cpp`
to `darkstar79/sudoku` (or similar — final name TBD). Expected fallout:

- `Source0:` / `Source:` URLs in `.spec` / `PKGBUILD` / `debian/watch` need
  to point at the new repo. GitHub redirects `git clone` for renamed repos,
  but URL-based tarball downloads in OBS recipes are safer to update
  explicitly than to rely on the redirect indefinitely.
- The Flatpak manifest's `type: git` `url:` will need updating in the same
  motion.
- **Out of scope** for the repo rename: the reverse-DNS app ID
  (`io.github.darkstar79.Sudoku_CPP`) is independent of the GitHub repo
  slug and will *not* change again. The executable name (`sudoku`),
  CMake project name (`sudoku`), and install tree under `share/sudoku/`
  are also already at the target name.

Upstream will announce on the OBS packager comms channel before
flipping the repo rename.

### Eventual 1.0.0 tag

CHANGELOG `[Unreleased]` will become `[1.0.0] — <date>`. The OBS namespace
currently ships date-stamped snapshots from `main`; the coordination
required to switch to versioned tags is open.

---

## Validation

Run after every install-tree-affecting change:

```sh
# AppStream metainfo
appstreamcli validate /usr/share/metainfo/io.github.darkstar79.Sudoku_CPP.metainfo.xml

# Desktop entry
desktop-file-validate /usr/share/applications/io.github.darkstar79.Sudoku_CPP.desktop

# (Fedora) rpmlint on the built package
rpmlint sudoku-cpp-<version>-<release>.<arch>.rpm
```

If any of these emit errors that look like they're caused by an upstream
change rather than a local recipe issue, file an issue at
[github.com/darkstar79/sudoku-cpp/issues](https://github.com/darkstar79/sudoku-cpp/issues)
referencing this document.

---

## Coordination

Upstream contact: open an issue on GitHub with the label `packaging`
(or @-mention `@darkstar79` in a PR review).

For changes upstream knows will hit downstream recipes, this document
is updated **in the same commit** as the breaking change. The expected
workflow is:

1. Upstream commits the change + adds/updates an entry in this file.
2. Upstream notifies packagers (issue, OBS comment, or direct contact).
3. Packagers update recipes referencing this doc and the linked commit.
4. Packagers ack — once all four namespaces are green, the entry moves
   from "Active migrations" to "Recent packager-relevant changes".
