# danios design — Music album folders (A4.1)

**Date:** 2026-07-14
**Extends:** [`apps/music.md`](apps/music.md) (A4, revised 2026-07-08). That spec pinned
`/music` as flat + non-recursive; this design supersedes that single point.
**Motivation:** folders inside `/music` are invisible today
(`lib/fs_names::filterAndSortNames` drops directories; `listFiles` reads one
level), and folders are the natural way to organize albums.

## What it is

Folders directly under `/music` are **albums**, one level deep. The Music app
gains an **albums view**: album folders first, then any loose `.mp3`s at the
`/music` root. Tapping an album shows its tracks (the existing tracks view);
tapping a track plays it, with the album as the playlist — next/previous and
auto-advance wrap **within the album**. Loose root tracks behave as one
implicit album. Folders inside albums are ignored (not listed, not scanned).

## UX rules

- **One player UI, swappable list.** The now-playing label, transport row and
  volume row stay on screen in both views; only the list content switches
  (albums ↔ tracks). Playback continues while browsing.
- **Navigation:** tapping an album enters its tracks view. Back
  (`App::handleBack`, SettingsApp precedent): tracks view → albums view →
  home. No in-list back row; the launcher's back arrow is the control.
- **Nothing preloaded.** After connect the app lands on the albums view with
  now-playing "-". Prev/next/play are no-ops until a first track is tapped
  (playlist starts empty — handlers guard on `playlist_.count()`).
- **Cross-album selection** = manual track change: new playlist set + ring
  flush (`open(..., flushRing=true)`), exactly like today's track taps.
- **Row cap:** the existing `kMaxListRows` (28) + "+N não listadas" overflow
  applies per view (albums view and each album's tracks view separately).
- **Album rows:** icon `LV_SYMBOL_DIRECTORY`, label = folder name verbatim
  (no extension stripping). Track rows unchanged.
- **Loose root tracks play directly** from the albums view (no intermediate
  view): tapping one sets the playlist to the loose-track set and plays it.
- **Empty states (PT):** empty album → "Nenhuma música neste álbum." inside
  the tracks view (back still works). Entirely empty `/music` (no albums, no
  loose tracks) → existing "Nenhuma música no cartão..." message.

## Architecture (follows A4 conventions)

| Unit | Change | Responsibility |
| --- | --- | --- |
| `lib/fs_names` | add `filterAndSortDirNames(entries)` | dirs-only mirror of `filterAndSortNames`: keep directories, drop hidden, sort byte-wise. Pure, native-tested. |
| `test/test_fs_names` | extend | cases: dirs kept / files dropped / hidden dirs dropped / sorted / empty. |
| `StorageService` | add `listDirs(const char* dir)` | same `FsEntry` walk as `listFiles`, through the new filter. Unmounted → empty vector. |
| `Playlist` | **untouched** | still basenames + wraparound + skip-bad. |
| `Mp3Player` | **untouched** | |
| `MusicApp` | view state | `enum class View { Albums, Tracks }`; `std::string pathPrefix_` (`"/music/"` for loose tracks, `"/music/<album>/"` inside an album) used by `openTrack` instead of the hardcoded prefix; albums view builder; `handleBack()` override; guards for the empty-playlist idle state. |

Data flow: albums view = `listDirs("/music")` + `listFiles("/music", ".mp3")`,
re-read from SD on every entry to the view (no caching — a directory listing
per tap is cheap and stays fresh across card edits). Album tap =
`listFiles("/music/<album>", ".mp3")` → `playlist_.setFiles(...)` happens only
when a **track** is tapped, so browsing never disturbs current playback; the
tracks view renders from a local listing until then.

*(Refinement note: `playlist_` keeps powering the currently-playing album while
the tracks view of another album is merely displayed. The tracks view therefore
renders from its own listing member, e.g. `browseTracks_`, and the view's album
prefix is tracked separately from the playing album's `pathPrefix_`.)*

## Error handling

| Situation | Behavior |
| --- | --- |
| Album folder deleted between listing and tap | `listFiles` returns empty → "Nenhuma música neste álbum." |
| Track file vanishes / unreadable | unchanged A4 path: `open()` fails → `markCurrentBad` → skip. |
| SD unmounted mid-session | unchanged (F3 behavior; listings come back empty). |

## Non-goals

Arbitrary nesting, album art, ID3 metadata, cross-album continuous play,
shuffle, persistence of the last album (A4 still owns **no NVS keys**).

## Testing

- Native: `filterAndSortDirNames` cases in `test/test_fs_names`.
- Device: extends the pending A4 Task 6 hardware pass (rides the `a4-music`
  branch — one gate covers both): album navigation, within-album wrap,
  cross-album switch flush, back-button chain, loose-tracks-only card,
  albums-only card, empty-album folder, >28 albums overflow row.

## E2E outcome

Card with `/music/Álbum A/*.mp3`, `/music/Álbum B/*.mp3` and two loose root
tracks → albums view lists Álbum A, Álbum B, then the loose tracks; an album's
tracks play in order, wrap within the album, and the back arrow walks
tracks → albums → home.
