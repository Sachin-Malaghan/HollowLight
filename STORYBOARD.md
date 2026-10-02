# EMBERHOME — storyboard (plan, not yet built)

Asked for on 2026-10-02: turn each level into a **chapter of ten stages**, tracked one by one.
This is the plan to agree on before building. Nothing here is in the game yet except where marked *(exists)*.

## Shape

- **11 chapters**, each a place on the way home. Each chapter has **10 stages**.
- A stage is short (30–60 seconds), has **one idea**, and ends at a cairn. Lighting the cairn saves it.
- Stage 10 of every chapter ends at a lamp post. Lighting it finishes the chapter.
- 110 stages in all. The chapter screen shows ten pips per chapter; the title shows "N of 110 lights".
- What exists today: 11 levels with 5–8 checkpoints each, progress saved per checkpoint, pips on the
  level select. So the tracking is already there; the work is building the stages.

## The story, chapter by chapter

| # | Chapter | What happens | New thing it teaches | Ghost? |
|---|---|---|---|---|
| 1 | The Edge of the Wood *(exists as a level)* | The child finds the lantern lit and the lamp post dark | run, jump, crate, traps, swinging log | no |
| 2 | A Stray *(exists)* | A dog starts following | WHISTLE, levers, the dog fits where you can't | no |
| 3 | Teeth in the Grass *(exists)* | The wood is full of old traps | crates spring traps; the dog points | no |
| 4 | Over the Ridge *(exists)* | First high ground | slopes, ladders, carrying a tool | no |
| 5 | Still Water *(exists)* | A flooded hollow | floating crates, rafts | no |
| 6 | The Warehouse *(exists)* | Shelter that isn't empty | plates, the dog stays, crowbar | **yes** |
| 7 | Sidings *(exists)* | A dead railway yard | sliding under wagons, the dog runs errands | **yes** |
| 8 | The Station *(exists)* | A platform with nobody waiting | two plates at once, footbridge | **yes** |
| 9 | The Mountain *(exists)* | The only way on is over | ledge climbing, scree slides, chasm jump | no |
| 10 | The Storm *(exists)* | Everything at once | all of it, in lightning | **yes** |
| 11 | Homecoming *(exists)* | Dawn, a village, the lamp post | a calm last walk | no |

## Ten stages in a chapter (the pattern)

1. **Arrive** — walk in, see the place, no danger.
2. **The idea** — the chapter's new thing, alone, impossible to fail badly.
3. **Again, harder** — same idea with one twist.
4. **With the dog** — the idea needs the dog.
5. **A rest** — something to look at (the old car, the shed), a short safe run.
6. **Two ideas** — the new thing plus one from an earlier chapter.
7. **The long one** — a puzzle with three steps; the top-right script gives the first step only.
8. **Under pressure** — in ghost chapters the ghost comes sooner here; elsewhere a timed door or rising water.
9. **The trick** — the stage that looks like stage 3 but isn't.
10. **The lamp** — a short run to the lamp post.

## Tracking

- Save: per chapter, the highest stage cleared; the stage CONTINUE resumes at *(exists as checkpoints)*.
- Chapter screen: ten pips per chapter, lit as cleared *(exists as checkpoint pips)*.
- A stage can be replayed from the chapter screen once cleared (new: a stage picker inside a chapter).
- Best time per stage (new).

## New pieces the stages will want (to be built as needed)

- Timed doors, rising water, a lift the dog can ride, a rope to swing on, a cart on rails to ride and jump from.
- More things for the ghost: a second kind that only moves when you don't look at it; lamps you can light
  to keep a room safe.
- Top-right script lines for every stage *(the system exists: `FLevelDef::Notes`)*.

## Build order (suggested)

1. Stage picker + per-stage best times (small).
2. Chapters 1–3 rebuilt as ten stages each (the existing levels become stages 2, 5, 7 and so on).
3. Chapters 4–6, then 7–9, then 10–11. Each batch goes to the closed test before the next starts.

Every stage must pass the same rule as today: the autopilot finishes it without dying, from every cairn.
