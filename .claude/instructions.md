# WorldClaims — Claude instructions

> State as of 2026-11-02 (code review CR5).

## Plugin identity
- A generic, reusable plugin (git submodule): one runtime module `WorldClaims`, log category `LogWorldClaims`,
  `EnabledByDefault`. It depends on `Core`, `CoreUObject`, `Engine` and `GameplayTags` only.
- **Nothing game-specific, no PCG, no VoxelWorlds.** Consumers depend on this plugin, never the reverse. The game's
  bridges (scatter suppression, PCG re-decoration, edit protection, map markers, the POI claims) live in VoxelWorldPOI.

## What it is
- `UWorldClaimRegistry`: world subsystem (game worlds) holding every active claim; `QueryClaimsInBox`,
  `GetHighestPriorityClaimAt`, `GetAllClaims`, `GetClaimCount`; `OnClaimsChanged(FBox)` fires on every register and
  unregister with the bounds it touched.
- `UWorldClaimComponent`: makes any actor a claim. `ClaimTags`, `Priority`, `DisplayInfo`
  (`FWorldClaimDisplayInfo`: label, icon, colour), `ClaimExtent` (an oriented box in the component's frame);
  registers on BeginPlay, unregisters on EndPlay, and mirrors its tags onto the owning actor (plus `PCGActorTag`,
  default `Claim`) so PCG graphs find claims by actor tag with no code dependency.
- `AWorldClaimVolume`: an actor that is a claim, with a hidden, non-colliding footprint box sized to `ClaimExtent`
  (what PCG reads as the actor's bounds).
- Native tags (`WorldClaimTags.h`): `Claim`, `Claim.POI.{City,Dungeon}`, `Claim.Construction.Camp`,
  `Claim.Decoration.{Suppress,OwnGraph,InheritBiome,Blend}`. Decoration policy is tags, not a PCG field.

## Reading order
1. `Documentation/CLAIMS_ARCHITECTURE.md` (this plugin): the model, the PCG recipes, conditioning, the state.
2. `Plugins/VoxelWorldPOI/Documentation/POI_SYSTEM.md`: the consumers and the POI claims.

## Rules
- Keep the dependency list as it is. A consumer that needs claims calls the registry or reads actor tags; a new
  policy is a new tag, read by whoever consumes it.
- Game-specific tags (`Claim.POI.Tower`, `Claim.Protection.NoEdit`, `Claim.Map.Hidden`, ...) are declared by the game
  layer, not here.
- Point tests go through the claim's transform (claims can be rotated: a planet's stand in their pad's tangent frame);
  the box query tests an AABB of the oriented box.
- Blueprint category root: `World Claims` (CR3, POI-17).
- No tests live in this plugin; the registry is exercised by VoxelWorldPOI's `VoxelWorlds.POI.*` claim tests
  (`ClaimEditValidator`, `ScatterClaimBridge`, `MapMarkers`, `Planet.Claims`).
