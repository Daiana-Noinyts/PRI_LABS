# Forest gameplay pack (C++ only, no Blueprints)

Drop every .h/.cpp into `Source/Forest/` (or your Public/Private folders), then regenerate project files and build.
No Build.cs changes needed (Engine/Core only). Classes use FOREST_API, so the module must be named `Forest`.

| Category | Class | Interaction |
|---|---|---|
| Static obstacles | `AStaticObstacle` | Collision = mesh-authored / Box / Sphere / Capsule; per-channel blocking (pawn, camera, traces); step-up on/off |
| Movable | `APushableCrate` | Push by walking into it (physics, 90 kg, stays upright); projectile impacts nudge it |
| Movable | `AKickableBall` | Run into it at speed -> kicked (velocity-change impulse), bouncy physical material |
| Movable | `AMovingPlatform` | Kinematic ride: eases between start and start+TargetOffset, carries the player |
| Collectible | `AHealthPickup` | Cream first-aid box + red cross; restores HealthCounter |
| Collectible | `AAmmoPickup` | Olive ammo box + brass rounds; restores BulletCounter |
| Teleport | `ATeleporterPad` | Overlap trigger; pair two pads far apart; auto-links both ways in editor |

## Setup
1. Place two `TeleporterPad` actors far apart (warns in the log if < 50 m). Set `Destination` on one; the other links itself.
2. Place `HealthPickup` / `AmmoPickup` (RestoreFraction 1.0 = full refill; a pickup stays put if you're already full).
3. Obstacle variety: rock -> Sphere, stump/tree trunk -> Capsule, log wall -> Box, camera-blocking off for canopies.

## Notes
- Health/ammo are read and written by reflection (`ForestStatAccess.h`): HealthCounter, MaxHealth, BulletCounter, MaxBullets.
- Pickups only react to player-controlled pawns, so enemies can't steal them.
- Visuals are flat-colour engine primitives (BasicShapeMaterial "Color"); all colours are editable per instance.
