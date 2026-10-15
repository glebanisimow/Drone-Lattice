//#############################################################################
// res_mesh -- coupled resonator network on a JI lattice window (GW x GH).
// Node n is a bank of NM band-pass resonators (TPT SVF) at
//   f_nh = root * ratio_n * h^(1+stretch) * 2^(fDev * ou_f / 1200),  h = 1..NM
// with T60_h = decay * h^-damp. Each node is excited by seeded noise whose
// level breathes (OU walk) and is weighted by the same focus/radius/purity
// window as lattice_swarm. Nodes feed their 4-neighbours on the lattice:
//   in_n = couple * sum_{m ~ n} dcblock(tanh(drive * y_m) / drive)
// Energy moves only along shared harmonics (h2 of 3/2 = h3 of 1/1), so nodes
// outside the focus sound through their neighbours, and stretch or freqDev
// dephase the coupling. A pair sharing a mode has loop gain couple^2: at
// >= 1 it self-oscillates (bowed), bounded by the tanh. Shared modes need
// h up to the generator: genX = 3 couples with NM >= 3, genY = 5 needs
// NM >= 5 (NP = 128). Stereo position follows the lattice x axis.
// Inputs: mod bus m1..m4 in [-1, 1] -> focusX, focusY, radius, couple.
// Outputs: stereo. Passive bargraphs viz/nNN: node RMS (0.2 s), for UIs.
//#############################################################################

declare name "res_mesh";
declare version "0.1";
declare license "MIT";

import("stdfaust.lib");
ct = library("contract.lib");

NP = 64;                      // resonators, multiple of NN (64 / 128)
GW = 4;
GH = 4;
NN = GW * GH;
NM = int(NP / NN);            // modes per node

LN1000 = 6.907755278982137;   // ln(1000): T60 <-> bandwidth

//--------------------------------- params -----------------------------------
ms(x) = hgroup("[1]mesh", x);
seed    = ms(nentry("[0]seed", 1, 0, 9999, 1)) : int;
decay   = ms(hslider("[1]decay [unit:s] [scale:log]", 8, 0.5, 60, 0.01)) : ct.lag(1);
damp    = ms(hslider("[2]damp", 0.5, 0, 1.5, 0.001)) : ct.lag(1);
tilt    = ms(hslider("[3]tilt", 1, 0, 2, 0.001)) : ct.lag(1);
stretch = ms(hslider("[4]stretch", 0, -0.1, 0.1, 0.0001)) : ct.lag(2);
couple  = ms(hslider("[5]couple", 0.6, 0, 1.5, 0.001));
drive   = ms(hslider("[6]drive", 1, 0.25, 4, 0.001)) : ct.lag(1);
fDev    = ms(hslider("[7]freqDev [unit:cent]", 3, 0, 50, 0.01)) : ct.lag(2);
fTau    = ms(hslider("[8]freqTau [unit:s] [scale:log]", 60, 1, 600, 0.1));
bDev    = ms(hslider("[9]breathDev", 0.8, 0, 1.5, 0.001)) : ct.lag(2);
bTau    = ms(hslider("[10]breathTau [unit:s] [scale:log]", 15, 1, 600, 0.1));
width   = ms(hslider("[11]width", 0.8, 0, 1, 0.001));

lt(x) = hgroup("[2]lattice", x);
focX   = lt(hslider("[0]focusX", 1.5, 0, GW - 1, 0.001));
focY   = lt(hslider("[1]focusY", 1.5, 0, GH - 1, 0.001));
radius = lt(hslider("[2]radius", 1.2, 0.2, 4, 0.001));
purity = lt(hslider("[3]purity", 0.3, 0, 1, 0.001));

md(x) = hgroup("[3]mod", x);
mdX = md(hslider("[0]toFocusX", 0.5, 0, 2, 0.001));
mdY = md(hslider("[1]toFocusY", 0.5, 0, 2, 0.001));
mdR = md(hslider("[2]toRadius [unit:oct]", 0.3, 0, 2, 0.001));
mdC = md(hslider("[3]toCouple", 0.2, 0, 1, 0.001));

//--------------------------------- lattice ----------------------------------
nodeL(n) = ct.lratio(ct.winI(GW, n), ct.winJ(GW, GH, n)) : ct.lag(ct.glide);
nodeF(n) = ct.exp2(ct.lroot + nodeL(n));                       // Hz, per node
harmF(h) = ct.exp2((1.0 + stretch) * ma.log2(h));

nodeW(n, m1, m2, m3) = ct.winWeight(GW, GH, n, focX + mdX * m1, focY + mdY * m2,
                                     radius * ct.exp2(mdR * m3), purity) : ct.lag(0.5);

tiltG(h) = ct.exp2(0.0 - tilt * ma.log2(h));

// Noise-driven output power: sum_n w_n^2 * sum_h tiltG_h^2 (unit per mode).
norm(m1, m2, m3) = 1.0 / sqrt(sw2 * th2 * (1.0 + bDev * bDev) + 1e-9)
with {
    sw2 = sum(n, NN, nodeW(n, m1, m2, m3) ^ 2);
    th2 = sum(i, NM, tiltG(i + 1) ^ 2);
};

// 4-neighbourhood on the window.
adj(n, i) = float((abs(ct.winX(GW, n) - ct.winX(GW, i))
                 + abs(ct.winY(GW, n) - ct.winY(GW, i))) == 1);

cpl(m4) = max(0.0, couple + mdC * m4) : ct.lag(0.5);

// Feedback send: unity small-signal gain, saturates at 1 / drive.
send = *(drive) : ma.tanh : /(drive) : ct.dcblock(5.0);

//---------------------------------- mode ------------------------------------
// White noise of unit variance through a unity-peak band-pass has output
// variance ENBW / (SR/2) = LN1000 / (T60 * SR), independent of f: gn undoes
// that, so every mode is driven to unit RMS. Q is capped at 1e4 (float32
// precision of g + k); very long decays on high modes are shortened.
mode(n, h, xf, ex, fb) = k * ct.svfbp(ct.tanpi(f / ma.SR), k, gn * ex + fb) * ct.bandGate(f)
with {
    f  = min(nodeF(n) * harmF(h) * ct.centsToRatio(fDev * xf), 0.45 * ma.SR);
    T  = decay * ct.exp2(0.0 - damp * ma.log2(h));
    k  = min(2.0, max(1e-4, LN1000 / (ma.PI * f * T)));
    gn = sqrt(T * ma.SR / LN1000) * tiltG(h);
};

//---------------------------------- node ------------------------------------
node(n, s, m1, m2, m3, m4) = sum(i, NM, mode(n, i + 1, xf, ex, cpl(m4) * s))
with {
    key = ct.hash(ct.hash(seed) + n);
    hv  = ct.hash(key + ct.tickIdx * 1640531527);
    xf  = ct.ou(fTau, ct.ulo(hv) * sqrt(3.0));
    xa  = ct.ou(bTau, ct.uhi(hv) * sqrt(3.0));
    ex  = nodeW(n, m1, m2, m3) * norm(m1, m2, m3) * max(0.0, 1.0 + bDev * xa)
        * ct.noise(key);
};

// in: NN sends, NMOD mods -> out: NN node signals.
nodeIn(n) = (par(i, NN, *(adj(n, i))) :> _), si.bus(ct.NMOD) : node(n);
net = si.bus(NN + ct.NMOD) <: par(n, NN, nodeIn(n));

//---------------------------------- stereo ----------------------------------
// x position in the window -> pan, small seeded offset; symmetric in x, so
// the long-term L/R balance does not depend on the seed.
pan(n) = _ <: *(gl), *(gr)
with {
    key = ct.hash(ct.hash(seed) + n);
    c   = 0.5 * float(GW - 1);
    p   = (float(ct.winX(GW, n)) - c) / c + 0.15 * ct.ulo(ct.hash(key + 7));
    th  = 0.25 * ma.PI * (1.0 + width * max(-1.0, min(1.0, p)));
    gl  = cos(th) : ct.lag(0.5);
    gr  = sin(th) : ct.lag(0.5);
};

//----------------------------------- viz ------------------------------------
// Node RMS for a lattice display; attached in the signal path, no effect on audio.
vz(x) = hgroup("[8]viz", x);
tap(n) = _ <: attach(_, (_ <: * : si.smooth(ba.tau2pole(0.2)) : sqrt : vz(vbargraph("n%2n", 0, 2))));

process = (net ~ par(n, NN, send)) : par(n, NN, tap(n)) : par(n, NN, pan(n)) :> si.bus(2) : ct.outStage;
