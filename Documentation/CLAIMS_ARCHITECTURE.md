# World Claims — Architecture & Design

**Status**: Phase 6a (registry core) built & PIE-verified, committed (`eece999`). Phases 6b–6e below are designed, not yet built.
**Plugin**: `WorldClaims` (new, dedicated, game-level — name adjustable).
**Purpose**: A spatial registry of *claims* — tagged, prioritized world regions produced by POIs and
player constructions and consumed by PCG decoration, Map, Compass, and AI. Includes terrain conditioning
(flat ground under POIs/placements).
**Related**: VoxelWorlds [PCG_DECORATION_ARCHITECTURE.md](../../VoxelWorlds/Documentation/Research/PCG_DECORATION_ARCHITECTURE.md),
[EDIT_LAYER.md](../../VoxelWorlds/Documentation/Features/EDIT_LAYER.md),
[BIOME_SYSTEM.md](../../VoxelWorlds/Documentation/Features/BIOME_SYSTEM.md).

---

## TL;DR

A **Claim** is a semantic spatial region (a footprint with meaning). It does **not** modify voxels — it is
metadata. Terrain *shape* changes go through the **Edit Layer** (runtime) or the **generator conditioning
hook** (generation-time). A placement (a city, a campsite) produces **both**: a terrain change *and* a claim.

```
                         PLACEMENT (city / campsite / POI)
                          │                         │
            ┌─────────────┘                         └──────────────┐
            ▼ terrain SHAPE                                         ▼ SEMANTICS
   ┌──────────────────────────┐                        ┌────────────────────────┐
   │ gen-time: generator       │                        │ Claim (this region is   │
   │   conditioning hook        │                        │ a city/camp; priority,  │
   │   (deterministic POIs)     │                        │ tags, policy, display)  │
   │ runtime: Edit Layer        │                        └───────────┬────────────┘
   │   "System" flatten edit    │                                    │ consumed by
   │   (player placements)      │              ┌─────────────────────┼───────────────────┐
   └───────────┬───────────────┘              ▼                     ▼                   ▼
               │ dirties chunks            PCG decoration         Map / Compass        AI / nav
               ▼ → RE-MESH                 (exclude / own graph    (icon, label)       (avoid)
                                            / blend; RE-GEN)
```

- **Edit Layer = voxel shape** (player + system edits, runtime). Changing it → chunks **re-mesh** (existing).
- **Claims = semantics** (region meaning/policy/display). Changing them → PCG cells **re-decorate** (Phase 7).
- The terrain re-mesh and the decoration re-gen are **independent dirtying paths** for two different layers.

## Core model

```
FWorldClaim = {
  Shape        // backed by a UE volume / spline component (precise footprint; PCG- and AI-native)
  Priority     // higher wins where footprints overlap
  Tags         // FGameplayTagContainer: Claim.POI.City, Claim.Construction.Camp,
               //   Claim.Decoration.Suppress / .OwnGraph / .Blend, ...
  DisplayInfo  // icon / label / color for Map & Compass
  Owner        // the producing actor (POI, construction)
}
```

Decoration **policy is gameplay tags**, not a PCG field — so the claims plugin stays **PCG-agnostic**
(it doesn't depend on PCG). PCG reads the tags; Map/Compass/AI ignore them.

## Plugin architecture (3 pieces, all generic)

- **`UWorldClaimRegistry`** — a world subsystem; the authoritative registry. Spatial query API
  ("claims overlapping this box/point, filtered by tag", with priority resolution). Map, Compass, AI call this.
- **`UWorldClaimComponent`** — attach to any actor to make it a claim; registers/unregisters with the
  registry, carries Shape/Tags/Priority/DisplayInfo. **Also tags its owning actor** so PCG can find it.
- **`AWorldClaimVolume`** — a convenience box/volume actor that *is* a claim (designer/hand-placed use).

**Dependencies**: `Core`, `CoreUObject`, `Engine`, `GameplayTags` only. **Not** PCG, **not** VoxelWorlds —
no circular dependencies. (Shapes: reuse UE volumes/splines per the shape decision.)

## Boundary-safe consumption

`VoxelWorlds` is independent of the game-framework plugins and vice versa, so consumption avoids code coupling:

| Consumer | Mechanism | Coupling |
|----------|-----------|----------|
| **PCG decoration** | Reads claim actors **by tag** (`Get Actor Data` filtered by `Claim.*`) → `Difference` (Suppress) / route to a graph (OwnGraph) / falloff (Blend) | none (PCG reads tags) |
| **Map / Compass / AI** | Call `UWorldClaimRegistry` query API directly (get DisplayInfo, priority, shape) | depend on WorldClaims (allowed) |
| **VoxelWorlds generation** | VoxelWorlds exposes a conditioning **interface**; game-level code implements it from claims | none (interface in VoxelWorlds, impl game-side) |

## Terrain conditioning

Two timeframes, both also registering a claim:

### Generation-time — deterministic POIs (cities): generator density-level hook

VoxelWorlds defines a boundary-safe interface, e.g. `IVoxelTerrainConditioner`:

```cpp
// In VoxelWorlds (boundary-safe; game implements it). Pure + thread-safe (generation is async).
struct FVoxelConditioningZone { /* footprint (center/radius or shape ref), TargetHeight, Falloff */ };
class IVoxelTerrainConditioner {
  virtual void GatherConditioning(const FBox& Region, TArray<FVoxelConditioningZone>& Out) const = 0;
};
```

During chunk generation the generator queries the registered conditioner for the chunk region and **blends
terrain height/density toward the zone's target** (with edge falloff): `H'(x,y) = lerp(H, TargetHeight,
Weight(x,y))`. The base **is** flat there — deterministic by construction, consistent everywhere as chunks
stream, no edit storage. (Decision: density-level, not deterministic edit-population.)

- **Heightmap modes** (InfinitePlane): straightforward height blend.
- **GPU generation**: the conditioner supplies the chunk's zones as compute-shader input (a small per-chunk
  buffer); the shader applies the blend. (Implementation note — the main added complexity of this path.)
- The game's conditioner implementation reads the seed-placed POIs (below) and returns their flatten zones.
  Must be a pure function of (seed, region) for determinism on the gen threads.

### Runtime — player placements (campsite): Edit Layer "System" edit

A player placing a campsite applies a `System` flatten **edit** via the existing `UVoxelEditManager` →
dirties the affected chunk(s) → **re-mesh** (existing mechanism; `EEditSource::System`). It *also* registers
a claim. No new terrain mechanism — reuses the Edit Layer. The claim drives PCG re-decoration (Phase 7) and
the map marker.

## Seed-based POI placement (deterministic)

POIs (cities, dungeons) are placed **deterministically from the world seed** (the tree-injection pattern):
a placement pass computes POI locations/footprints as a pure function of the seed, so every machine agrees
without replication. This placement is **game-level** (it knows "city" vs "dungeon"); it:
1. registers the POI's **claim** (semantics → PCG/Map/Compass), and
2. feeds the **conditioner** (footprint + target height → gen-time flattening).

VoxelWorlds stays POI-agnostic — it only calls the conditioning hook.

## Multiplayer

Server-authoritative. **Constructions** (dynamic — campsites, player builds) replicate as replicated claim
actors/components; their flatten edits go through the server-authoritative Edit Layer. **POIs** are
deterministic (seed-based) so they exist identically on every machine without per-instance replication.
Consumers (PCG/Map/Compass/AI) are local + deterministic off these replicated/deterministic inputs. PCG
output stays **visual only**; the POI structure / harvestables / the construction itself are gameplay state
via the Interaction/Inventory/Item plugins.

## Where each part lives

| Part | Home | Why |
|------|------|-----|
| Claims registry / component / volume + query API | **`WorldClaims` plugin** (generic) | reusable, no game/voxel specifics |
| Seed-based POI placement + conditioner impl + runtime placement glue | **game-level** (game module or a thin integration plugin, e.g. alongside `DungeonVoxelIntegration`) | knows POI types; bridges claims ↔ VoxelWorlds |
| `IVoxelTerrainConditioner` hook + generator call site | **VoxelWorlds** (submodule) | boundary-safe interface; impl supplied by game |
| PCG-side exclusion/policy graph patterns | **VoxelPCG / decoration graphs** | reads claims by tag |

## Phased build plan (Claims = Phase 6 of the PCG roadmap)

1. **6a — Registry core** ✅ **DONE** (`eece999`): `WorldClaims` plugin: `UWorldClaimRegistry` +
   `UWorldClaimComponent` + `AWorldClaimVolume` + `FWorldClaimDisplayInfo` + tag-based actor tagging +
   spatial query API (box / point / highest-priority, gameplay-tag filtered). Deps Core/Engine/GameplayTags
   only; `EnabledByDefault` (no uproject edit). PIE-verified: overlapping volumes register on BeginPlay,
   queries resolve count/point/box/priority, owners tagged `Claim`, clean unregister on EndPlay.
2. **6b — PCG consumption**: decoration graphs read claim actors by tag → exclusion (Suppress) /
   route (OwnGraph) / falloff (Blend). Verify: a claim volume clears/owns decoration in PIE.
3. **6c — Generation conditioning**: `IVoxelTerrainConditioner` hook in VoxelWorlds generation (CPU
   density-level first), a game-side impl from claims. Verify: a city claim → flat ground generated beneath.
4. **6d — Seed-based POI placement**: deterministic POI placement → claims + conditioning. Verify: POIs
   appear deterministically with flat ground + themed decoration.
5. **6e — Runtime placements**: campsite = System edit (flatten) + claim; ties into Phase 7 (claim dirty →
   PCG re-decorate). GPU conditioning path.
6. **Consumers**: Map/Compass icons from the registry; AI avoidance (later).

## Open questions

- Conditioner determinism/perf on async + GPU generation (per-chunk zone upload).
- Claim shape → PCG: actor AABB vs the actual volume/spline (precision); Biome Core exclusion-spline
  falloff as the reference for `Blend`.
- Priority resolution when many claims overlap (PCG difference ordering vs registry priority).
- Replication granularity for construction claims (actor replication vs a replicated registry array).
- Plugin name (`WorldClaims` vs `WorldAnnotations`); whether the integration glue is a thin plugin or the
  game module.
