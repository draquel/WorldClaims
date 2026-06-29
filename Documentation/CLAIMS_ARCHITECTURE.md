# World Claims — Architecture & Design

**Status**: Phase 6 substantially DONE — 6a registry, 6b PCG consumption (Suppress/OwnGraph/Blend), 6c CPU generation conditioning (both heightmap modes), 6d seed-based POI placement, 6e runtime placements (`VoxelWorldPOI` plugin) — all built & verified. Remaining: 6c SphericalPlanet (radial model) + GPU conditioning, Phase 7 (claim/edit dirty → throttled PCG re-decorate), and the consumers (Map/Compass icons, AI avoidance).
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

### How a claim becomes PCG-readable (the tag bridge)

`UWorldClaimComponent::BeginPlay` mirrors the claim's data onto its **owning actor** so PCG needs no
dependency on this plugin:
- the umbrella `PCGActorTag` (default `Claim`), **plus**
- each gameplay tag in `ClaimTags` as an **actor FName tag** (e.g. `Claim.Decoration.Suppress`, `Claim.POI.City`).

The **footprint** is the actor's bounds: `AWorldClaimVolume` carries a runtime, hidden, non-colliding
`UBoxComponent` sized to `ClaimExtent`, and PCG's `PCGHelpers::GetActorBounds` includes non-colliding
components — so `Get Actor Data` reads the exact footprint. Native `Claim.*` tags are registered in
[WorldClaimTags.h](../Source/WorldClaims/Public/WorldClaimTags.h).

### Suppress recipe — verified 6b

A decoration graph clears its ambient scatter inside suppress claims with two stock nodes spliced before the
spawner:

```
Voxel Surface Sampler ─── Source ──▶ Difference (density: Binary) ──▶ Static Mesh Spawner
Get Actor Data ──────── Differences ──▶  ┘
  (AllWorldActors, ByTag = Claim.Decoration.Suppress, GetSinglePoint → one bounded point per claim)
```

`Difference` removes every sampler point inside a claim's footprint. Verified in PIE (`PCG_VoxelClaimTest`
over `VoxelWorldsTest`): with a `Claim.Decoration.Suppress` volume present, **0** of 120 cube instances fell
inside its ±1200 footprint; destroying the claim and regenerating put **49** back — decoration outside the
footprint unchanged. (Cosmetic: PCG warns it sanitized the dotted tag name into an attribute name; the actor
match itself is unaffected.)

### OwnGraph recipe — verified 6b

The claim's footprint suppresses ambient **and** gets its own decoration. Two branches off the sampler,
both reading `Claim.Decoration.OwnGraph` claims, merged at the output:

```
Sampler ─ Difference (Differences = OwnGraph claims) ──▶ ambient spawner   (ambient OUTSIDE footprint)
Sampler ─ Intersection (Source1 = OwnGraph claims) ─▶ To Point ─▶ own spawner   (own decoration INSIDE)
```

`Intersection` is the inverse of `Difference` — it keeps only points inside the footprint. The own spawner
is a distinct mesh here (the placeholder for a per-POI subgraph; per-type routing reuses the biome-dispatcher
pattern, a 6d concern). Verified in PIE (`PCG_VoxelClaimOwnGraph`): inside the footprint **0** ambient cubes /
**49** own meshes; **0** own meshes leaked outside.

### Blend recipe — verified 6b

Ambient *fades* inside the footprint instead of a hard cut. The inside points are thinned and merged back
with the untouched outside:

```
Sampler ─ Difference (Blend claims) ─────────────────────────┐  (outside: full)
Sampler ─ Intersection (Blend claims) ─ To Point ─ Random Choice (ratio 0.3, take Chosen) ─┤─ Merge ─▶ spawner
```

Verified in PIE (`PCG_VoxelClaimBlend`): inside the footprint ambient retained at ~**37 %** area-density vs
full outside — between Suppress (0 %) and ambient (100 %). (This is a uniform partial fade; a true
edge-graded falloff ring — Biome Core's exclusion-spline falloff is the reference — is the production
refinement.) **Tooling note:** wiring `Intersection`→`Random Choice` auto-inserts a *type filter* that drops
the intersection's lazy spatial output; insert an explicit **To Point** before `Random Choice` instead.

## Terrain conditioning

Two timeframes, both also registering a claim:

### Generation-time — deterministic POIs (cities): generator density-level hook

✅ **CPU path implemented (6c)** in VoxelWorlds: `FVoxelConditioningZone` + `IVoxelTerrainConditioner` in
[VoxelTerrainConditioning.h](../../VoxelWorlds/Source/VoxelGeneration/Public/VoxelTerrainConditioning.h);
`UVoxelChunkManager::AddConditioningZone` / `SetTerrainConditioner` gather zones per chunk; the InfinitePlane
CPU generator applies the height blend. Below is the original design (the shipped interface uses `FBox2D` for
the region and a center/inner-radius/falloff zone):

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

### Runtime — player placements (campsite): Edit Layer "System" edit ✅ (6e)

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
2. **6b — PCG consumption** ✅ **DONE** (all three policies): decoration graphs read claim actors by tag →
   exclusion (**Suppress**), own decoration (**OwnGraph**), partial fade (**Blend**). Native `Claim.*` tags +
   actor-tag mirroring + runtime footprint box; recipes (`Difference` / `Intersection` / `Random Choice`) all
   PIE-verified by per-zone instance counts. A true edge-graded Blend falloff is a later refinement.
3. **6c — Generation conditioning** — CPU **heightmap modes** ✅ **DONE**: `IVoxelTerrainConditioner` +
   `FVoxelConditioningZone` hook in VoxelWorlds generation. The chunk manager gathers zones per chunk into the
   generation request; the CPU generator blends the per-column terrain height toward each zone's target
   (smoothstep falloff) before the SDF — so density/material/surface all flatten, deterministically, no edit
   storage. Applied to **both heightmap modes — InfinitePlane and IslandBowl**
   (`VoxelWorlds.Generation.Conditioning.{FlattensTerrain,FlattensIslandBowl}`). **Deferred**: SphericalPlanet
   (radial surface — the XY-footprint+Z-height zone doesn't map; needs a radial-zone model) and the GPU compute
   path (its needs may shift with 6e/7's dynamic zones). The game-side **claims-driven** conditioner impl is 6d.
4. **6d — Seed-based POI placement** ✅ **DONE** (in-tree plugin `VoxelWorldPOI`, parent repo): deterministic
   seed-hash placement (`FPOIPlacement`) → a per-POI `FVoxelConditioningZone` (via `FPOITerrainConditioner :
   IVoxelTerrainConditioner`, flattening to the natural ground height) **and** an `AWorldClaimVolume`
   (`Claim.POI.City` + `Claim.Decoration.Suppress`). `UPOIPlacementSubsystem` resolves the chunk manager,
   registers the conditioner, and spawns POI claims. Unit-verified (`VoxelWorlds.POI.*`: determinism,
   cross-region consistency, conditioner zones); PIE-verified (seed 4325 → 11 deterministic POI claims
   registered + conditioner registered). The bridge that closes the loop: claims ↔ conditioning ↔ PCG.
5. **6e — Runtime placements** ✅ **DONE** (`VoxelWorldPOI`): `UVoxelWorldPOIPlacement::PlaceCampsite` flattens
   already-generated terrain to a target height in a cylinder via a **System edit on the existing Edit Layer**
   (`UVoxelEditManager::ApplyEdit`, `EEditSource::System`, batched in a Begin/EndEditOperation → dirties chunks →
   re-mesh — the same path as digging) **and** registers a `Claim.Construction.Camp` (+ `Decoration.Suppress`)
   claim. No new terrain mechanism — claims + the edit layer compose. PIE-verified: a campsite → flat pad
   re-meshed (2533 voxel edits) + claim registered. Forward link: claim-dirty → PCG re-decorate is Phase 7.
6. **Consumers**: Map/Compass icons from the registry; AI avoidance (later).

## Open questions

- Conditioner determinism/perf on async + GPU generation (per-chunk zone upload).
- Claim shape → PCG: actor AABB vs the actual volume/spline (precision); Biome Core exclusion-spline
  falloff as the reference for `Blend`.
- Priority resolution when many claims overlap (PCG difference ordering vs registry priority).
- Replication granularity for construction claims (actor replication vs a replicated registry array).
- Plugin name (`WorldClaims` vs `WorldAnnotations`); whether the integration glue is a thin plugin or the
  game module.
