//#############################################################################
// mod_bus -- seeded slow modulation source for the contract mod bus.
// 4 outputs in [-1, 1] (VCV: x 5 V), periods from seconds to minutes.
// Each channel morphs continuously between three sources (shape 0..2):
//   0 sine LFO   : period P, phase offset from seed (glides on seed change)
//   1 OU walk    : correlation time P / 4, scaled to std 0.45
//   2 chaos      : Lorenz x, one lobe orbit ~ P (P >= 1.75 s); seeded start + seeded
//                  1e-3 kicks per tick, so trajectories diverge per seed
// OU and Lorenz advance on the contract tick (50 ms) and are smoothed like
// ct.ou: the sequence is identical at every sample rate. Every parameter is
// click-free under steps; outputs have no discontinuities.
// Inputs: none. Outputs: m1..m4.
//#############################################################################

declare name "mod_bus";
declare version "0.1";
declare license "MIT";

import("stdfaust.lib");
ct = library("contract.lib");

NCH = ct.NMOD;

gl(x) = hgroup("[0]bus", x);
seed = gl(nentry("[0]seed", 1, 0, 9999, 1)) : int;
rate = gl(hslider("[1]rate [scale:log]", 1, 0.1, 10, 0.001));   // divides all periods

PERIOD = (37, 53, 71, 97);    // s, mutually non-harmonic defaults
SHAPE  = (0, 1, 2, 1.5);

ch(c, x) = hgroup("[%c1]m%c1", x) with { c1 = c + 1; };
period(c) = ch(c, hslider("[0]period [unit:s] [scale:log]", ba.take(c + 1, PERIOD), 1, 600, 0.01)) / rate;
shape(c)  = ch(c, hslider("[1]shape", ba.take(c + 1, SHAPE), 0, 2, 0.001)) : ct.lag(1);
depth(c)  = ch(c, hslider("[2]depth", 1, 0, 1, 0.001)) : ct.lag(1);
offset(c) = ch(c, hslider("[3]offset", 0, -1, 1, 0.001)) : ct.lag(1);

// Same 2-pole lag as ct.ou (removes the tick staircase), but starts at the
// input value, so the bus has no startup ramp.
tickSmooth = ct.lag(0.5 * ct.TICK) : ct.lag(0.5 * ct.TICK);

// Phase in cycles advanced per tick (increment latched per tick, linear
// inside the tick): exact period in float32 and identical at every SR. A
// per-sample phasor would add 1 / (P * SR) ~ 1e-7 and lose ~10 % to rounding.
tickPhase(P) = loop ~ (_, _) : (!, !, ma.frac)
with {
    loop(ph0, inc0) = ph, inc, ph + inc * frac
    with {
        inc = select2(ct.isFirst, select2(ct.tickEdge, inc0, ct.tickDt / P), ct.tickDt / P);
        ph  = select2(ct.tickEdge, ph0, ma.frac(ph0 + inc0));
        frac = float(ct.tickPhase) / float(ct.tickLen);
    };
};

//--------------------------------- sources ----------------------------------
lfo(c, key) = ct.sin2pi(ma.frac(tickPhase(period(c)) + phi0))
with {
    phi0 = 0.5 * (1.0 + ct.ulo(ct.hash(key + 11))) : ct.lag(2);
};

walk(c, key) = 0.45 * ct.ou(0.25 * period(c), ct.ulo(ct.hash(key + ct.tickIdx * 1640531527)) * sqrt(3.0));

// Lorenz (10, 28, 8/3), RK4 once per tick. Orbit time of a lobe ~ 0.7 units,
// so dt = TICK * 0.7 / P, capped at 0.02 (RK4 diverges above ~0.12 and the
// NaN would survive a zero crossfade weight): chaos is never faster than
// P = 1.75 s, shorter periods just run at that rate.
lorenz(c, key) = loop ~ (_, _, _) : (_, !, !) : tickSmooth : /(20.0)
with {
    dt = min(0.02, ct.tickDt * 0.7 / period(c));
    kick = 1e-3 * ct.ulo(ct.hash(key + ct.tickIdx * 1640531527 + 3));
    fx(x, y, z) = 10.0 * (y - x);
    fy(x, y, z) = x * (28.0 - z) - y;
    fz(x, y, z) = x * y - (8.0 / 3.0) * z;
    loop(x0, y0, z0) = select2(ct.tickEdge, x, ux + kick),
                       select2(ct.tickEdge, y, uy),
                       select2(ct.tickEdge, z, uz)
    with {
        x = select2(ct.isFirst, x0, 8.0 * ct.ulo(ct.hash(key + 21)));
        y = select2(ct.isFirst, y0, 8.0 * ct.ulo(ct.hash(key + 22)));
        z = select2(ct.isFirst, z0, 25.0 + 5.0 * ct.ulo(ct.hash(key + 23)));
        h = 0.5 * dt;
        k1x = fx(x, y, z);  k1y = fy(x, y, z);  k1z = fz(x, y, z);
        x2 = x + h * k1x;   y2 = y + h * k1y;   z2 = z + h * k1z;
        k2x = fx(x2, y2, z2);  k2y = fy(x2, y2, z2);  k2z = fz(x2, y2, z2);
        x3 = x + h * k2x;   y3 = y + h * k2y;   z3 = z + h * k2z;
        k3x = fx(x3, y3, z3);  k3y = fy(x3, y3, z3);  k3z = fz(x3, y3, z3);
        x4 = x + dt * k3x;  y4 = y + dt * k3y;  z4 = z + dt * k3z;
        k4x = fx(x4, y4, z4);  k4y = fy(x4, y4, z4);  k4z = fz(x4, y4, z4);
        ux = x + dt / 6.0 * (k1x + 2.0 * k2x + 2.0 * k3x + k4x);
        uy = y + dt / 6.0 * (k1y + 2.0 * k2y + 2.0 * k3y + k4y);
        uz = z + dt / 6.0 * (k1z + 2.0 * k2z + 2.0 * k3z + k4z);
    };
};

//--------------------------------- channel ----------------------------------
// Triangular crossfade 0 -> 1 -> 2, then depth, offset, soft limit to |y| < 1.
chan(c) = ct.softclip(0.9, depth(c) * (w0 * lfo(c, key) + w1 * walk(c, key) + w2 * lorenz(c, key))
                           + offset(c))
with {
    key = ct.hash(ct.hash(seed) + 7919 * (c + 1));
    s  = shape(c);
    w0 = max(0.0, 1.0 - s);
    w1 = max(0.0, 1.0 - abs(s - 1.0));
    w2 = max(0.0, s - 1.0);
};

process = par(c, NCH, chan(c));
