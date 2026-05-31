---
name: project_gl_instancing
description: Correct behavior of vertex attribute divisors in glDrawArraysInstanced — confirmed by testing in this project
metadata: 
  node_type: memory
  type: project
  originSessionId: 3ecca219-99fc-42d9-b4f9-a712ef198b4d
---

`divisor=0` attributes reset per instance: the same `count` values are re-read for every instance. The buffer does NOT advance across instance boundaries. All instances share the same vertex data (e.g. same 6 positions, same 6 colors).

`divisor=1` gives one value per instance, applied uniformly to all vertices of that instance. This is the right choice for per-instance flat color.

There is no vertex attribute divisor that gives per-vertex-per-instance data in GL 3.3. Options for that:
- TBO (`samplerBuffer` + `texelFetch(sampler, gl_InstanceID * 6 + gl_VertexID)`)
- UBO or uniform array indexed by `gl_InstanceID * 6 + gl_VertexID`
- SSBO — GL 4.3+ only

`gl_VertexID` resets to 0 at the start of each instance in `glDrawArraysInstanced`.

**Why:** Confirmed by the user testing the code — all instances were rendering with the background color because `divisor=0` was repeating the first 6 color entries for every instance. CLAUDE.md was also corrected to remove the incorrect "vertex counter increments continuously" claim.

**How to apply:** When suggesting per-instance or per-vertex-per-instance data layouts, use `divisor=1` for flat per-instance values. Do not suggest `divisor=0` as a way to vary data per instance.
