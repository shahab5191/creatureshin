# Realtime Brain — System Specification (v1)

A biologically-inspired **spiking neural network** embodied as a simple 2D creature.
No backprop, no train/infer split — the brain runs continuously and learns online
through local plasticity gated by a global hormone signal.

---

## 1. Design principles

1. **Sparse & activity-scaling.** Cost scales with the *active wavefront* (firing
   neurons × fan-out), never with total neuron/synapse count `N`/`E`.
2. **Lazy timestamp evaluation for decaying state.** Membrane potential and
   eligibility traces are only recomputed when touched, using
   `x(t) = x(t₀)·exp(-(t-t₀)/τ)`. Idle elements cost nothing.
3. **Eager commit for accumulating state.** The *weight* does not decay to a
   baseline, so its reward-driven change is committed **at reward time**, not
   deferred to next access. (Lazy pull would lose finalized updates for dormant
   synapses.)
4. **Global hormone = one signed scalar** broadcast diffusely, exactly like
   dopamine. Its size is independent of `N`.
5. **Motivation is homeostatic, not hardcoded.** Reward is `d(wellbeing)/dt`
   over internal drives. New stimuli are declarative table entries, never core
   changes.
6. **Tick-based clock, sparse processing.** A global discrete clock `dt` makes
   the per-tick math deterministic; each tick processes only the frontier and
   touched elements.

---

## 2. State — what each entity stores

### 2.1 Neuron `i`
| Field | Symbol | Meaning |
|---|---|---|
| potential | `V_i` | membrane potential (decays to 0) |
| potential timestamp | `tV_i` | last tick `V_i` was updated (lazy decay anchor) |
| last spike time | `ts_i` | last tick this neuron fired (drives STDP) |
| threshold | `θ_i` | fire level; rises on firing (adaptation), decays back |
| refractory-until | `tr_i` | cannot fire before this tick |
| bias | `b_i` | tonic drive added each active tick |
| polarity | `σ_i ∈ {+1,-1}` | excitatory / inhibitory (sign of *all* its outgoing signals) |
| position | `x_i` | 2D/3D coordinate; gates which partners are physically reachable for new synapses (§11) |

### 2.2 Synapse `i → j`  (stored CSR-style: per source `i`, contiguous arrays)
| Field | Symbol | Meaning |
|---|---|---|
| weight (magnitude) | `w_ij ≥ 0` | strength; effective signal = `σ_i · w_ij` |
| eligibility tag | `e_ij` | STDP "tag", decays with `τ_e` |
| touch timestamp | `tsyn_ij` | last tick `e_ij`/`w_ij` were updated (lazy anchor for `e` decay **and** LTD) |

> Minimal variant: store only `w_ij` and reconstruct `e` from the two neurons'
> `ts` at reward time. Default keeps `e_ij` so tags can accumulate over multiple
> spike pairs (more accurate). See §5.5.

### 2.3 Global brain state
| Field | Meaning |
|---|---|
| `t` | current tick (integer; wall time = `t·dt`) |
| `F` | **frontier**: set of neuron ids that fired last tick (propagate this tick) |
| `RF` | **recently-fired list** ("last triggered"): `(neuron_id, spike_tick)` for spikes within horizon `H`; the working set for reward commit |
| `R` | current hormone value this tick (signed scalar) |
| `W_prev` | wellbeing at previous tick |

### 2.4 Homeostatic drives (motivation)
Registry of channels; each:
| Field | Meaning |
|---|---|
| `value` | current level |
| `setpoint` | comfortable target |
| `weight` | importance in wellbeing |
| `drift` | passive change per tick (e.g. energy leak, fatigue growth) |

No reward-history buffer is needed — `R` is computed and consumed each tick.

---

## 3. Parameters (suggested starting values)

| Symbol | Value | Meaning |
|---|---|---|
| `dt` | 1 ms | brain tick |
| `τ_V` | 20 ms | membrane time constant |
| `τ_ref` | 4 ms | absolute refractory period |
| `θ₀` | 1.0 | resting threshold |
| `V_reset` | 0.0 | post-spike reset |
| `θ_adapt` | +0.15 / spike | adaptation bump on firing |
| `τ_θ` | 100 ms | threshold decay back to `θ₀` |
| `A₊` | 0.010 | STDP potentiation amplitude |
| `A₋` | 0.012 | STDP depression amplitude (slightly stronger → stability) |
| `τ₊`, `τ₋` | 20 ms | STDP timing windows |
| `τ_e` | 1000 ms | eligibility trace decay |
| `η` | 0.10 | reward-modulated learning rate |
| `α` | 0.02 | unsupervised (reward-independent) Hebbian rate; keep < `η` |
| `w_max` | 1.0 | weight saturation cap |
| `τ_w` | 100 s | slow weight decay / forgetting (LTD of unused) |
| `H` | 5·τ_e = 5 s | recently-fired horizon (`exp(-5) < 1%`) |
| `d_axon` | 1 tick | axonal propagation delay |
| exc:inh | 80:20 | polarity ratio |
| ticks/world-step | 10 | brain runs 10× faster than physics |
| `r_reach` | (world units) | max distance a new synapse can span (axon/dendrite reach) |
| `p_form` | 0.01 | probability of forming a candidate synapse per eligible co-firing |
| `w_init` | 0.05 | initial weight of a newly-formed synapse (small) |
| `w_prune` | 0.01 | prune floor: delete a synapse when `w` decays below this |
| `syn_budget` | 128 | max synapses per neuron (fan-in/out cap; homeostatic) |

---

## 4. Core math (helpers)

**Exponential decay** (all lazy updates):
```
decay(x, Δt, τ) = x · exp(-Δt / τ)
```

**Saturating weight update** (soft cap, prevents runaway):
```
cap(w) = clamp(w, 0, w_max)          # simple hard cap
# optional soft form: Δw_eff = Δw · (1 - w/w_max) for potentiation
```

**STDP kernel** (from two spike times; `Δt = t_post - t_pre`):
```
Δt > 0 (pre before post): +A₊ · exp(-Δt / τ₊)     # potentiation
Δt < 0 (post before pre): -A₋ · exp(+Δt / τ₋)     # depression
```

**Two plasticity routes** — every `ΔSTDP` produced by the kernel is applied
*both* ways, simultaneously:
1. **Unsupervised (immediate):** `w += α·ΔSTDP` — direct "fire together, wire
   together." Strengthens correlated paths with *no reward needed*; builds
   representation/structure from raw correlation.
2. **Reward-modulated (deferred):** `e += ΔSTDP`, later cashed out as
   `w += η·R·e` at reward time (§5.4). Turns correlation into *value*.

Balance knob: `α` (unsupervised) vs `η` (reward). `α=0` → pure reward-gated;
reward held off → pure Hebbian. The unsupervised term is a positive-feedback
loop, so its stability *depends on* the cap `w_max`, `A₋ > A₊`, the slow decay
`τ_w`, and optional per-neuron synaptic scaling.

**Wellbeing & hormone** (homeostatic reward):
```
W    = -Σ_c drive_c.weight · (drive_c.value - drive_c.setpoint)²
R    = k_R · (W - W_prev)      # signed; + when improving, - when worsening
```

---

## 5. The per-tick algorithm

Processed each tick `t`. Only active/touched elements are visited.

### 5.0 Sense
Read world → set input-neuron drive currents (see §6). Input neurons are added to
the touched set with an injected current `I_i`.

### 5.1 Propagate frontier (scatter + pre-side STDP)
For each source `i` in `F` (fired at `t - d_axon`):
```
for each synapse i→j:
    # lazy-decay target potential to now, then inject
    V_j   = decay(V_j, t - tV_j, τ_V) + σ_i · w_ij
    tV_j  = t
    mark j as touched
    # pre-side STDP = DEPRESSION (post fired before this pre spike)
    if ts_j is recent:
        touch_synapse(i→j, t)                       # §5.5 (lazy e + LTD)
        ΔSTDP = -A₋ · exp(-(t - ts_j)/τ₋)
        e_ij += ΔSTDP                               # tag for reward commit (§5.4)
        w_ij  = cap(w_ij + α · ΔSTDP)               # unsupervised direct update
```

### 5.2 Integrate & fire (touched neurons)
For each touched neuron `j` not in refractory (`t ≥ tr_j`):
```
V_j  = decay(V_j, t - tV_j, τ_V) + b_j + I_j        # bias + sensor current
tV_j = t
θ_j  = θ₀ + decay(θ_j - θ₀, t - (last θ update), τ_θ)   # adaptation relaxes
if V_j ≥ θ_j:
    FIRE(j)
```
`FIRE(j)`:
```
V_j  = V_reset
tr_j = t + τ_ref
ts_j = t
θ_j += θ_adapt                        # fatigue: harder to fire again soon
add j to F_next                       # propagates next tick
push (j, t) to RF                     # recently-fired list
# post-side STDP = POTENTIATION (pre fired before this post spike)
for each incoming synapse i→j:
    if ts_i is recent:
        touch_synapse(i→j, t)
        ΔSTDP = +A₊ · exp(-(t - ts_i)/τ₊)
        e_ij += ΔSTDP                           # tag for reward commit (§5.4)
        w_ij  = cap(w_ij + α · ΔSTDP)           # unsupervised direct update
# structural growth: sprout new inputs from nearby co-firing neurons (§11)
for each k in RF with |x_k - x_j| ≤ r_reach and no existing k→j:
    if j.synapse_count < syn_budget and random() < p_form:
        create synapse k→j with w = w_init, e = 0, tsyn = t
```

### 5.3 Homeostasis & hormone
```
for each drive c: c.value += c.drift        # passive drift
W = -Σ_c c.weight·(c.value - c.setpoint)²
R = k_R · (W - W_prev)
W_prev = W
```

### 5.4 Reward commit (eager, at reward time)
Only if `|R| > ε`:
```
for each (n, t_spike) in RF:                # activity-bounded working set
    for each synapse s adjacent to n (touched during tag window):
        touch_synapse(s, t)                 # decays e to now
        s.w = cap(s.w + η · R · s.e)         # + strengthens, - weakens
```
> `R > 0` → recently-active tags strengthened; `R < 0` → weakened. This is the
> only place weights change from reward.

### 5.5 `touch_synapse(i→j, t)` — lazy trace + forgetting
```
Δt        = t - tsyn_ij
e_ij      = decay(e_ij, Δt, τ_e)            # eligibility fades
w_ij      = decay(w_ij, Δt, τ_w)            # slow LTD of unused paths
tsyn_ij   = t
if w_ij < w_prune: delete synapse i→j       # structural pruning (§11)
```

### 5.6 Motor decode → act
Read motor-neuron firing rates over a sliding window → actuator forces (§7).

### 5.7 World step (every `ticks/world-step`)
Apply forces → move creature → resolve collisions → apply stimulus effects to
drives (§8). Evict `RF` entries older than `H`.

---

## 6. Sensors (world → spikes)

All sensors are **rate-coded**: stimulus intensity → injected current `I_i` (or
Poisson spike probability) on a dedicated input neuron.

| Sensor | Encoding | Input neurons |
|---|---|---|
| Chemical/scent gradient | ring of directional receptors; each fires ∝ local gradient toward food | `n_dir` (e.g. 8) |
| Touch / collision | fires on contact, ∝ contact force | 1–4 (by body sector) |
| Proprioception | current speed, turn rate | 2 |
| Interoception | hunger (energy below setpoint), pain (health drop) | 1 per drive |

---

## 7. Motors (spikes → action)

| Motor | Neurons | Decode |
|---|---|---|
| Differential drive | `M_L`, `M_R` (antagonistic) | rate(`M_L`) − rate(`M_R`) → turn; sum → forward thrust |

`rate(m)` = spikes from neuron `m` in the last `W_motor` ms (e.g. 50 ms) / window.
Force = `gain · rate`. Two motor neurons already give full steering.

---

## 8. World & stimulus table

2D continuous plane, top-down. Contains food sources (emit scent gradient),
optional obstacles/hazards.

**Drive registry (v1):**
```
energy   { setpoint: 100, drift: -0.02, weight: 1.0 }
health   { setpoint: 100, drift:  0.0,  weight: 2.0 }
```

**Stimulus → drive effects (declarative; extend freely):**
```
food:     { energy: +30 }
fire:     { health: -20 }
predator: { health: -50 }
water:    { hydration: +25 }     # add a hydration drive to enable
rest:     { fatigue: -10 }       # add a fatigue drive to enable
```
Adding a stimulus = one table entry. The creature learns its value via §5.3–5.4.
Curiosity (small `+R` for novelty/prediction-error) and relief (removal of a
negative drive → automatic `+R`) require no extra wiring.

---

## 9. Module layout

```
brain/     # SoA neuron arrays + CSR synapses; §4–§5. World-agnostic.
body/      # sensor placement, motor mapping, drive registry, interoception
world/     # 2D space, food, hazards, physics, stimulus effects
loop/      # sensorimotor tick: sense → brain ticks → motor → world step
render/    # visualization (pygame); fully optional / headless-capable
```

**Data layout:** Structure-of-Arrays for neuron state (SIMD/cache friendly);
CSR edge blocks per source neuron for synapses. Optional locality reordering of
neuron ids so connected clusters are contiguous.

---

## 10. Scaling notes

- Per-tick cost ∝ frontier size × fan-out (active edges), independent of `N`.
- Reward commit cost ∝ `|RF|` × fan-out, only on ticks with `R ≠ 0` (sparse).
- Memory: `O(E)` sparse edges, not `O(N²)`. Hormone/`RF` sizes are `O(1)`/`O(activity)` in `N`.
- Parallelism (later): partition neurons by owner; each worker owns its slice's
  state and applies the shared read-only `R`. Numba/Rust/GPU for the hot scatter.

---

## 11. Structural plasticity (growth & pruning)

Topology is **emergent**, not designed: the brain grows and removes connections
in response to activity, exactly like spine turnover in a real brain. Structure
(which edges exist) and weight (their strength) are **separate** — a weak edge is
not the same as an absent one, and a weak edge can recover.

### 11.1 Preconditions — proximity
Neurons have positions `x_i`. A new synapse `k→j` can only form if
`|x_k - x_j| ≤ r_reach` — the model of finite axon/dendrite reach. Use a spatial
grid/hash so "neurons near `j`" is an O(1) neighborhood query, not an O(N) scan.

### 11.2 Formation — structural Hebb (in `FIRE`, §5.2)
When `j` fires, its recently-fired, in-reach, not-yet-connected neighbors are
candidates for a **new** synapse. With probability `p_form` and while under the
per-neuron `syn_budget`, instantiate `k→j` with a small `w_init` and `e=0`. The
new synapse is then ordinary: STDP + the reward hormone decide whether it grows
or fades. It survives **only** if `k` and `j` keep co-firing — structural
"fire together, wire together."

### 11.3 Pruning — structural forgetting (in `touch_synapse`, §5.5)
The slow weight decay (`τ_w`) already drives unused weights toward zero. When
`w_ij` falls below `w_prune`, the edge is **deleted** and its storage freed.
Formation + pruning together keep `E` bounded and the wiring adaptive.

### 11.4 Homeostatic budget
Each neuron caps its synapses at `syn_budget`. To sprout a new synapse when full,
the weakest existing one must be pruned first (finite synaptic real estate). This
balances growth against removal and prevents unbounded densification.

### 11.5 Cost & storage
- Formation is checked **only at spike events among `RF`** (activity-bounded) with
  an O(1) spatial neighbor lookup — no O(N²) scan.
- CSR is append-hostile: use **preallocated slack per neuron**, a growable
  per-neuron edge list, or **periodic compaction** to absorb add/delete churn.

### 11.6 What it unlocks
The creature **grows its own circuitry**: useful sensor→motor pathways sprout
reinforcing connections; irrelevant regions thin out. Combined with the
homeostatic hormone, the brain physically reshapes itself around its niche.

---

## 12. Deferred upgrades (wired-in hooks, off by default)

- **Reward = prediction error (RPE):** replace raw `R` with `R - expected(R)` to
  keep learning past plateaus (TD-style). Same commit machinery.
- **Multiple hormones:** run §5.4 per neuromodulator with its own sign/gain/`τ`.
- **STDP variants, synaptic scaling / homeostatic normalization** of incoming
  weights per neuron (lazy, periodic).
- **Neurogenesis:** add whole new neurons (not just synapses) in limited regions.
- **3D world / richer body.**
```
