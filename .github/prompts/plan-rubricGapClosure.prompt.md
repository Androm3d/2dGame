## Autonomous Execution Prompt: 2D Game Rubric Closure for 10/10

Implement all automatable work required to close rubric gaps for the 2D game project. Execute continuously until all non-manual tasks are finished.

Scope priority:
1. Pass-critical basic gameplay requirements
2. Missing mechanics and grading shortcuts
3. HUD and feedback quality
4. Polish and stability
5. Packaging preparation that can be automated

Context:
This project depends on user-created assets/maps/animations in some steps. When blocked by non-code assets or manual playtesting, do not stop. Instead:
1. Add clear TODO comments where code expects missing assets.
2. Create or update a markdown handoff file documenting exactly what the user must add and where.
3. Continue with all remaining automatable tasks.

## Non-Negotiable Execution Contract

1. Never stop after planning. Execute edits immediately.
2. Do not ask for confirmation between phases unless a true blocker is hit.
3. Use sub-agents for parallelizable exploration, audits, and verification tasks.
4. After each phase, record a short completion log with:
	1. files changed
	2. behavior implemented
	3. checks run
	4. unresolved risks
5. If a phase fails verification, fix and re-run before moving on.

## Blocker Policy

A true blocker is only one of:
1. Missing secrets/credentials
2. Missing third-party binary dependency that cannot be installed automatically
3. Ambiguous requirement that causes mutually exclusive implementations
4. Tooling/runtime failure after retries

Before declaring blocker:
1. Try at least 3 distinct approaches.
2. Capture the exact failing command/output.
3. Propose one concrete workaround.
4. Continue with other independent tasks.

## Sub-Agent Delegation Policy

Use sub-agents aggressively for:
1. cross-file symbol discovery
2. parallel requirement compliance checks
3. identifying all call sites for mechanical changes
4. final verification sweeps

Main agent remains responsible for final edits and integration.

## Phase Plan

### Phase 1: Core Game Loop Compliance

Tasks:
1. Implement state machine routing for MENU, PLAY, INSTRUCTIONS, CREDITS in [src/Game.cpp](src/Game.cpp#L299) and [src/Game.h](src/Game.h#L18).
2. Implement menu navigation and rendering using existing text pipeline from [src/Text.cpp](src/Text.cpp#L1) and [src/Scene.cpp](src/Scene.cpp#L937).
3. Implement lose flow: when lives reach zero, return to MENU and reset session state consistently in [src/Player.cpp](src/Player.cpp#L390), [src/Game.cpp](src/Game.cpp#L299), and [src/Scene.cpp](src/Scene.cpp#L487).
4. Implement win/level completion flow for 9-level progression (example endpoint: clear map 2_0 and use right portal).

Definition of done:
1. All 4 screens are reachable and navigable.
2. Losing all lives always returns to MENU without crash.
3. Level completion advances/finishes correctly in code flow.

### Phase 2: Mandatory Missing Mechanics

Tasks:
1. Implement weight-to-enemy elimination in [src/Scene.cpp](src/Scene.cpp#L696).
2. Implement functional defensive/helpful items:
	1. Shield absorbs hit or grants timed invulnerability.
	2. Heal increases lives with a defined max cap policy (example: 9).
	3. Sword gating: attack unlocked only when sword is acquired.
3. Add two traversal level items (example: spring + dash tile):
	1. extend tile taxonomy in [src/TileMap.h](src/TileMap.h#L15)
	2. parse and map in [src/TileMap.cpp](src/TileMap.cpp#L215)
	3. implement runtime mechanics in [src/Player.cpp](src/Player.cpp#L154) or [src/Scene.cpp](src/Scene.cpp#L703) as appropriate.

Definition of done:
1. Weight collisions can eliminate enemies.
2. Shield/heal/sword behaviors are mechanically meaningful.
3. Traversal tiles are recognized and applied with cooldown/trigger guards.

### Phase 3: Debug/Teacher Shortcuts

Tasks:
1. Add God mode toggle G with HUD indicator in [src/Game.cpp](src/Game.cpp#L315).
2. Add K to grant all objective keys and synchronize room/world key state.
3. Add 1..9 direct level load mapping:
	1. 1->0,2
	2. 2->1,2
	3. 3->2,2
	4. 4->2,1
	5. 5->1,1
	6. 6->0,1
	7. 7->0,0
	8. 8->1,0
	9. 9->2,0
4. Restrict shortcuts to PLAY state.

Definition of done:
1. G/K/1..9 work deterministically.
2. Shortcuts cannot corrupt menu state.

### Phase 4: HUD and Feedback

Tasks:
1. Extend HUD in [src/Scene.cpp](src/Scene.cpp#L937) for lives, keys, items, debug toggles.
2. Add on-screen feedback for objective completion, shield block, and transitions.
3. Reuse delayed transition guard from [src/Scene.cpp](src/Scene.cpp#L419) for smooth flow.

Definition of done:
1. HUD always reflects current gameplay state.
2. Major events provide immediate readable feedback.

### Phase 5: Polish and Combat Feedback

Tasks:
1. Integrate background music and SFX call sites.
2. If assets are missing, wire placeholders and document replacement workflow in markdown.
3. Add simple event animations/transitions (position pulse, tint flash, or equivalent).
4. Improve game feel tuning: input responsiveness, hit timing, movement readability.
5. Make Enemy1 arrows destroyable by player attack only when sword is acquired in [src/Enemy.cpp](src/Enemy.cpp#L461) interaction flow.

Definition of done:
1. Audio hooks exist at correct event points.
2. Visual feedback exists for core events.
3. Arrow-destruction mechanic works and is sword-gated.

### Phase 6: QA and Packaging Prep

Tasks:
1. Regression test transition input edge cases and collision/hitbox consistency.
2. Add null-check/map-load stability guards around [src/Scene.cpp](src/Scene.cpp#L102) paths.
3. Prepare automatable delivery artifacts:
	1. info.txt scaffold
	2. binary packaging instructions
	3. source packaging checklist
4. For manual-only items (demo video and full playthrough), create a clear handoff markdown checklist.

Definition of done:
1. Build is clean and stable.
2. No missing map-link references in runtime graph.
3. Manual handoff instructions are explicit and complete.

## Dependency Rules

1. Phase 1 must pass before progression validation.
2. Phase 2 tile extensions must land before map-dependent traversal behavior is considered complete.
3. Phase 2 and Phase 3 can run partially in parallel, but final integration must be verified together.
4. Phase 5 polish must not regress pass-critical mechanics from Phases 1-3.

## Verification Protocol

Run after each phase and again at the end:
1. Compile/build check with zero compile errors.
2. Runtime smoke test: start game, enter play, trigger a key action, exit without crash.
3. Feature checks tied to each phase DoD.
4. Final audit summary:
	1. completed items
	2. deferred manual items
	3. known risks
	4. exact files changed

## Output Requirements

At completion, provide:
1. concise changelog by phase
2. verification results by phase
3. manual TODO list for user-owned assets/maps/animations/audio/video
4. remaining risk list (if any)

Do not end early unless a true blocker is reached under the blocker policy.
