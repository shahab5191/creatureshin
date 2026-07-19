#pragma once
#include <cstdint>

// Global parameters — see SPEC.md §3. All time constants are in *ticks*
// (dt = 1 ms). This is the single place to tune the creature.
namespace annshin::config {

using tick_t = std::int64_t;

inline constexpr double DT_MS = 1.0;  // ms per tick (reporting only)

// --- Membrane / integrate-and-fire (§2.1, §5.2) ---
inline constexpr double TAU_V       = 20.0;  // membrane time constant
inline constexpr tick_t TAU_REF     = 4;     // absolute refractory period
inline constexpr double THETA0      = 1.0;   // resting threshold
inline constexpr double V_RESET     = 0.0;   // post-spike reset
inline constexpr double THETA_ADAPT = 0.15;  // threshold bump per spike (fatigue)
inline constexpr double TAU_THETA   = 100.0; // threshold relaxation

// --- STDP (§4) ---
inline constexpr double A_PLUS    = 0.010;   // potentiation amplitude
inline constexpr double A_MINUS   = 0.012;   // depression (slightly stronger → stability)
inline constexpr double TAU_PLUS  = 20.0;
inline constexpr double TAU_MINUS = 20.0;
inline constexpr tick_t STDP_WINDOW = 100;   // skip pairs older than this (~5·τ)

// --- Plasticity rates (§4 "two routes") ---
inline constexpr double ALPHA = 0.02;   // unsupervised Hebbian rate (keep < ETA)
inline constexpr double ETA   = 0.10;   // reward-modulated rate

// --- Weights ---
inline constexpr double W_MAX   = 1.0;       // saturation cap
inline constexpr double W_INIT  = 0.05;      // initial weight of a synapse (§3)
inline constexpr double TAU_W   = 100000.0;  // slow forgetting / LTD (~100 s)
inline constexpr double W_PRUNE = 0.01;      // prune floor (structural, §11)

// --- Eligibility / reward (§5.4) ---
inline constexpr double TAU_E    = 1000.0;   // eligibility trace decay (~1 s)
inline constexpr tick_t H_RECENT = 5000;     // recently-fired horizon (~5·τ_e)
inline constexpr double K_REWARD = 0.5;      // wellbeing-change → hormone gain
inline constexpr double R_EPS    = 1e-6;     // skip reward commit when |R| below this

// --- Misc ---
inline constexpr unsigned RNG_SEED = 1234;   // deterministic init

// --- Structure ---
inline constexpr double INHIB_FRAC = 0.2;    // fraction of inhibitory neurons
inline constexpr int    FANOUT     = 15;     // initial outgoing synapses per neuron

// --- Structural plasticity (§11) ---
// Positions are initialized in the unit cube [0,1]³, so R_REACH is in those units.
inline constexpr double R_REACH    = 0.2;    // max spatial span for a new synapse
inline constexpr double P_FORM     = 0.01;   // formation prob per eligible co-firing
inline constexpr int    SYN_BUDGET = 128;    // max fan-in per neuron (homeostatic)

// --- Motors (§7) ---
inline constexpr double TAU_MOTOR = 50.0;    // motor firing-rate smoothing

// --- Loop (§5.7) ---
inline constexpr int TICKS_PER_WORLD_STEP = 10;

} // namespace annshin::config
