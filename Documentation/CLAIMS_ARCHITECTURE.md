# World Claims — Architecture & Design

> State as of 2026-11-02 (code review CR5).

**Purpose**: A spatial registry of *claims*: tagged, prioritized world regions produced by POIs and constructions and
consumed by PCG decoration, scatter suppression, edit protection, the map and wildlife spawning. Terrain conditioning
(flat ground under POIs) is the companion mechanism described here. The registry and the policy tags are this plugin;
the consumers live in VoxelWorldPOI (`Plugins/VoxelWorldPOI/Documentation/POI_SYSTEM.md`) and the conditioning hook in
VoxelWorlds.
**Related**: `Plugins/VoxelWorlds/Documentation/Research/PCG_DECORATION_ARCHITECTURE.md`,
`Plugins/VoxelWorlds/Documentation/Features/EDIT_LAYER.md`, `Plugins/VoxelWorlds/Documentation/Features/BIOME_SYSTEM.md`.

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
- **Claims = semantics** (region meaning/policy/display). Changing them → PCG components **re-decorate** and scatter
  exclusion volumes follow (the bridges in VoxelWorldPOI).
- The terrain re-mesh and the decoration re-gen are **independent dirtying paths** for two different layers.

## Core model

```
FWorldClaim = {
  Shape        // backed by a UE volume / spline component (precise footprint; PCG- and AI-native)
  Priority     // higher wins where footprints overlap
  Tags         // FGameplayTagContainer: Claim.POI.City, Claim.Construction.Camp,
               //   Claim.Decoration.Suppress / .OwnGraph / .Blend / .InheritBiome, ...
  DisplayInfo  // icon / label / color for Map & Compass
  Owner        // the producing actor (POI, construction)
}
```

Decoration **policy is gameplay tags**, not a PCG field — so the claims plugin stays **PCG-agnostic**
(it doesn't depend on PCG). PCG reads the tags; Map/Compass/AI ignore them.

## Plugin architecture (3 pieces, all generic)

- **`UWorldClaimRegistry`** — a world subsystem (game worlds); the authoritative registry. Spatial query API
  (`QueryClaimsInBox`, `GetHighestPriorityClaimAt`, `GetAllClaims`, tag-filtered) and `OnClaimsChanged(FBox)` on every
  register and unregister. The map markers, the edit validator, the scatter and PCG bridges and the wildlife spawner
  use it.
- **`UWorldClaimComponent`** — attach to any actor to make it a claim; registers/unregisters with the
  registry, carries `ClaimTags`, `Priority`, `DisplayInfo` (`FWorldClaimDisplayInfo`: label, icon, colour) and
  `ClaimExtent` (an oriented box in the component's frame). **Also tags its owning actor** so PCG can find it.
- **`AWorldClaimVolume`** — a convenience box/volume actor that *is* a claim (designer/hand-placed use).

**Dependencies**: `Core`, `CoreUObject`, `Engine`, `GameplayTags` only. **Not** PCG, **not** VoxelWorlds —
no circular dependencies. A claim's shape is the component's oriented box; the registry's box query tests an AABB of
it, while point tests invert the claim's transform.

## Boundary-safe consumption

`VoxelWorlds` is independent of the game-framework plugins and vice versa, so consumption avoids code coupling:

| Consumer | Mechanism | Coupling |
|----------|-----------|----------|
| **PCG decoration** | Reads claim actors **by tag** (`Get Actor Data` filtered by `Claim.*`) → `Difference` (Suppress) / route to a graph (OwnGraph) / falloff (Blend) | none (PCG reads tags) |
| **Map, edit protection, scatter, wildlife** | Game-level code (VoxelWorldPOI) calls the `UWorldClaimRegistry` query API and listens to `OnClaimsChanged` | depend on WorldClaims (allowed) |
| **VoxelWorlds generation** | VoxelWorlds exposes a conditioning **interface**; game-level code implements it from claims | none (interface in VoxelWorlds, impl game-side) |

### How a claim becomes PCG-readable (the tag bridge)

`UWorldClaimComponent::BeginPlay` mirrors the claim's data onto its **owning actor** so PCG needs no
dependency on this plugin:
- the umbrella `PCGActorTag` (default `Claim`), **plus**
- each gameplay tag in `ClaimTags` as an **actor FName tag** (e.g. `Claim.Decoration.Suppress`, `Claim.POI.City`).

The **footprint** is the actor's bounds: `AWorldClaimVolume` carries a runtime, hidden, non-colliding
`UBoxComponent` sized to `ClaimExtent`, and PCG's `PCGHelpers::GetActorBounds` includes non-colliding
components — so `Get Actor Data` reads the exact footprint. Native `Claim.*` tags are registered in
[WorldClaimTags.h](../Source/WorldClaims/Public/WorldClaimTags.h): `Claim`, `Claim.POI`, `Claim.POI.City`,
`Claim.POI.Dungeon`, `Claim.Construction`, `Claim.Construction.Camp`, `Claim.Decoration.Suppress`, `.OwnGraph`,
`.InheritBiome`, `.Blend`. Game-specific tags (`Claim.POI.Tower`, `Claim.Protection.NoEdit`, `Claim.Map.Hidden`, ...)
are declared by the game layer (VoxelWorldPOI's `POITags.h`).

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

Implemented in VoxelWorlds: `FVoxelConditioningZone` + `IVoxelTerrainConditioner` in
`Plugins/VoxelWorlds/Source/VoxelGeneration/Public/VoxelTerrainConditioning.h`; `UVoxelChunkManager::AddConditioningZone`
/ `SetTerrainConditioner` gather zones per chunk into the generation request, and the generator blends each column's
terrain height toward the zone's target (smoothstep falloff) before the SDF, so density, material and surface all
flatten, deterministically, with no edit storage. Every world mode: InfinitePlane and IslandBowl on the CPU and the
GPU (`GpuApplyConditioning`); on a planet a zone is a cap on the datum sphere (`FVoxelConditioningZone::Direction`,
great-circle radii, an altitude target), CPU and GPU, answered by `GatherConditioningOnSphere`. The game's conditioner
is VoxelWorldPOI's `FPOITerrainConditioner`. The design sketch (the shipped interface takes an `FBox2D` region and a
centre / inner-radius / falloff zone):

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

- **GPU generation**: the chunk's zones go to the compute shader as a small per-chunk buffer; the shader applies
  the same blend.
- The game's conditioner reads the seed-placed POIs (below) and returns their flatten zones. It must be a pure
  function of (seed, region) for determinism on the generation threads.

### Runtime — constructions: Edit Layer "System" edit

A runtime construction that needs flat ground applies a `System` flatten **edit** through `UVoxelEditManager`
(`EEditSource::System`, dirtying and re-meshing like any edit) and registers a claim; the claim drives the
re-decoration and the map marker. The project has no such construction at present: the campsite flatten built for it
had no caller and was deleted in CR1 (`UVoxelWorldPOIPlacement::PlaceCampsite`, D-CR17), and a campsite placed from a
kit stands on the ground as it is. System edits a game stamps (dungeons) begin their operations outside the undo
history, so a player's undo never takes them back.

## Seed-based POI placement (deterministic)

POIs (cities, dungeons) are placed **deterministically from the world seed** (the tree-injection pattern):
a placement pass computes POI locations/footprints as a pure function of the seed, so every machine agrees
without replication. This placement is **game-level** (it knows "city" vs "dungeon"); it:
1. registers the POI's **claim** (semantics → PCG/Map/Compass), and
2. feeds the **conditioner** (footprint + target height → gen-time flattening).

VoxelWorlds stays POI-agnostic — it only calls the conditioning hook.

## Multiplayer

Server-authoritative. **Constructions** (dynamic — player builds) replicate as replicated claim
actors/components; their flatten edits go through the server-authoritative Edit Layer. **POIs** are
deterministic (seed-based) so they exist identically on every machine without per-instance replication.
Consumers (PCG/Map/Compass/AI) are local + deterministic off these replicated/deterministic inputs. PCG
output stays **visual only**; the POI structure / harvestables / the construction itself are gameplay state
via the Interaction/Inventory/Item plugins.

## Where each part lives

| Part | Home | Why |
|------|------|-----|
| Claims registry / component / volume + query API | **`WorldClaims` plugin** (generic) | reusable, no game/voxel specifics |
| Seed-based POI placement, conditioner, the claim bridges (scatter, PCG, edit protection, markers) | **VoxelWorldPOI** (a parent-repository game-level plugin) | knows POI types; bridges claims ↔ VoxelWorlds ↔ PCG |
| `IVoxelTerrainConditioner` hook + generator call site | **VoxelWorlds** (submodule) | boundary-safe interface; impl supplied by game |
| PCG-side exclusion/policy graph patterns | **VoxelPCG / decoration graphs** | reads claims by tag |

## State as of 2026-11-02

Everything the claims model set out to do is built (the PCG roadmap's phases 6a-6e and 7, 2026-06 to 2026-07, and
the planet work of 2026-10-17):

- **The registry** (6a): the component, the volume, display info, actor tagging and the box / point /
  highest-priority queries. A PIE world registers overlapping volumes on BeginPlay and unregisters them on EndPlay.
- **PCG consumption** (6b): the three recipes above, each verified in PIE by per-zone instance counts. A true
  edge-graded Blend falloff is still a refinement for later.
- **Conditioning** (6c): every world mode, CPU and GPU, planets by radial caps
  (`VoxelWorlds.Generation.Conditioning.{FlattensTerrain,FlattensIslandBowl}`, `VoxelWorlds.POI.Planet.ConditionerZones`).
- **Seed-based placement** (6d): VoxelWorldPOI's placement registers each POI's claim and conditioning zone
  (`VoxelWorlds.POI.*`).
- **Re-decoration** (7): `OnClaimsChanged` drives VoxelWorldPOI's `UPOIClaimDecorationRefreshSubsystem`, which
  regenerates the overlapping PCG components after a 0.4 s debounce (PCG's own actor tracking is editor-only), and
  `UPOIScatterClaimSuppressionSubsystem`, which keeps the scatter's exclusion volumes in step with the suppress claims.
- **Consumers**: map and compass markers (`UPOIMapMarkerSubsystem`), edit protection (`Claim.Protection.NoEdit`,
  `FPOIClaimEditValidator`) and the wildlife spawner's claim check. No AI avoidance consumes claims yet.
- **Planets**: a POI's claims stand in its pad's tangent frame; the edit validator, scatter suppression and the
  registry's point query invert the claim's transform, so rotated claims protect and suppress unchanged (the box query
  stays an AABB of the oriented box).

## Open questions

- Claim shape → PCG: actor AABB vs the actual volume/spline (precision); Biome Core exclusion-spline
  falloff as the reference for `Blend`.
- Priority resolution when many claims overlap (PCG difference ordering vs registry priority).
- Replication granularity for construction claims (actor replication vs a replicated registry array).
- Re-decoration regenerates whole PCG components that overlap a change; cell-precise dirtying, and edit-layer changes
  driving re-decoration, are not built.
