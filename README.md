quick start:

    ./compile && ./a.out

then press keys z x c v b n m , . / to play a sound.

optional: change the oscillator settings:

change the type:

    type=sine, triangle, saw_up, saw_down, square, pulse12, pulse25, or random

output the signal as audio:

    output=0.0 to 1.0

adjust the pitch (frequency) using a different oscillator:

    phase_input=osc1, osc2, ..., or osc10
    phase_input_m=<float>, which multiplies the oscillator, e.g. 1.0, 13.5

adjust the amplitude (volume) using a different oscillator:

    amp_input=osc1, osc2, ..., or osc10
    amp_input_m=<float>, which multiplies the input oscillator level

envelope settings

    attack=1.0
    release=1.0
    sustain=0.2
    decay=1.0

creating an LFO

    freq=<float>, if set the oscillator will use this freq instead of input keys

freq multiplier

    freq_m=<float>
