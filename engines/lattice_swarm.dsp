//#############################################################################
// lattice_swarm -- JI lattice window (GW x GH nodes) driving an additive swarm.
// Partial k: node n = k mod NN, harmonic h = k div NN + 1.
//   f_k = root * ratio_n * h^(1+stretch) * 2^(fDev * ou_f / 1200)
//   a_k = w_n * h^-tilt * max(0, 1 + aDev * ou_a) * band(f_k) * norm
//   w_n = gauss(|node - focus| / radius) * 2^(-purity * hdist_n)
// Inputs: mod bus m1..m4 in [-1, 1] -> focusX, focusY, radius, stretch.
// Outputs: stereo. Passive bargraphs viz/nNN: node weight (incl. mod), for UIs.
//#############################################################################

declare name "lattice_swarm";
declare version "0.1";
declare license "MIT";

import("stdfaust.lib");
ct = library("contract.lib");

NP = 64;                      // partials, multiple of NN (64 / 128 / 256)
GW = 4;
GH = 4;
NN = GW * GH;
NH = int(NP / NN);            // harmonics per node

//--------------------------------- params -----------------------------------
sw(x) = hgroup("[1]swarm", x);
seed    = sw(nentry("[0]seed", 1, 0, 9999, 1)) : int;
stretch = sw(hslider("[1]stretch", 0, -0.1, 0.1, 0.0001)) : ct.lag(2);
tilt    = sw(hslider("[2]tilt", 1, 0, 2, 0.001)) : ct.lag(1);
fDev    = sw(hslider("[3]freqDev [unit:cent]", 6, 0, 50, 0.01)) : ct.lag(2);
fTau    = sw(hslider("[4]freqTau [unit:s] [scale:log]", 40, 1, 600, 0.1));
aDev    = sw(hslider("[5]ampDev", 0.6, 0, 1.5, 0.001)) : ct.lag(2);
aTau    = sw(hslider("[6]ampTau [unit:s] [scale:log]", 20, 1, 600, 0.1));
width   = sw(hslider("[7]width", 0.8, 0, 1, 0.001));

lt(x) = hgroup("[2]lattice", x);
focX   = lt(hslider("[0]focusX", 1.5, 0, GW - 1, 0.001));
focY   = lt(hslider("[1]focusY", 1.5, 0, GH - 1, 0.001));
radius = lt(hslider("[2]radius", 1.2, 0.2, 4, 0.001));
purity = lt(hslider("[3]purity", 0.3, 0, 1, 0.001));

md(x) = hgroup("[3]mod", x);
mdX = md(hslider("[0]toFocusX", 0.5, 0, 2, 0.001));
mdY = md(hslider("[1]toFocusY", 0.5, 0, 2, 0.001));
mdR = md(hslider("[2]toRadius [unit:oct]", 0.3, 0, 2, 0.001));
mdS = md(hslider("[3]toStretch", 0, 0, 0.05, 0.0001));

//--------------------------------- lattice ----------------------------------
nodeL(n) = ct.lratio(ct.winI(GW, n), ct.winJ(GW, GH, n)) : ct.lag(ct.glide);
nodeF(n) = ct.exp2(ct.lroot + nodeL(n));                       // Hz, per node
harmF(h, m4) = ct.exp2((1.0 + stretch + mdS * m4) * ma.log2(h)); // per harmonic

nodeW(n, m1, m2, m3) = ct.winWeight(GW, GH, n, focX + mdX * m1, focY + mdY * m2,
                                     radius * ct.exp2(mdR * m3), purity) : ct.lag(0.5);

tiltG(h) = ct.exp2(0.0 - tilt * ma.log2(h));

// RMS normaliser: sum_k a_k^2 = sum_n w_n^2 * sum_h tiltG_h^2.
norm(m1, m2, m3) = 1.0 / sqrt(sw2 * th2 * (1.0 + aDev * aDev) + 1e-9)
with {
    sw2 = sum(n, NN, nodeW(n, m1, m2, m3) ^ 2);
    th2 = sum(i, NH, tiltG(i + 1) ^ 2);
};

//--------------------------------- partial ----------------------------------
partial(k, m1, m2, m3, m4) = s * a * gl, s * a * gr
with {
    n = k % NN;
    h = int(k / NN) + 1;

    key = ct.hash(ct.hash(seed) + k);
    hv  = ct.hash(key + ct.tickIdx * 1640531527);
    xf  = ct.ou(fTau, ct.ulo(hv) * sqrt(3.0));
    xa  = ct.ou(aTau, ct.uhi(hv) * sqrt(3.0));

    f = nodeF(n) * harmF(h, m4) * ct.centsToRatio(fDev * xf);

    phi0 = ma.frac(float(k) * 0.6180339887);   // fixed: seed changes never jump phase
    s    = ct.sin2pi(ct.wrap1(ct.phasor(min(f / ma.SR, 0.5)) + phi0));

    a = nodeW(n, m1, m2, m3) * tiltG(h) * norm(m1, m2, m3)
      * max(0.0, 1.0 + aDev * xa) * ct.bandGate(f);

    side = float(2 * (k % 2) - 1);   // alternate sides: L/R balance for any seed
    th = 0.25 * ma.PI * (1.0 + width * side * abs(ct.ulo(ct.hash(key + 7))));
    gl = cos(th) : ct.lag(0.5);
    gr = sin(th) : ct.lag(0.5);
};

//----------------------------------- viz ------------------------------------
// Node weights for a lattice display; attached to the output, no effect on audio.
vz(x) = hgroup("[8]viz", x);
meters(m1, m2, m3, m4) = par(n, NN, nodeW(n, m1, m2, m3) : vz(vbargraph("n%2n", 0, 1))) :> _;

process = si.bus(ct.NMOD) <: (par(k, NP, partial(k)) :> si.bus(2) : ct.outStage), meters
        : _, attach;
