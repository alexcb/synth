quick start:

    ./compile && ./a.out

then press keys z x c v b n m , . / to play a sound, or q to quit.

optional: change the oscillator settings:

First define a VFO, which will be set to the frequency being played, e.g.

    [vfo1]

change the type:

    type=sine, triangle, saw_up, saw_down, square, pulse12, pulse25, or random (defaults to sine)

output the signal as audio:

    output=0.0 to 1.0 (defaults to 1.0)

envelope settings:

    attack=1.0      (default 0.0)
    release=1.0     (default 0.0)
    sustain=0.2     (default 1.0)
    decay=1.0       (default 0.0)

freq multiplier:

    freq_m=<float> (default 1.0)

Creating a LFO:

    [lfo1]

Assign the LFO a frequency:

    freq=<float>, recommended range of 0.1 to 20.0 (default to 1.0)

The VFO or LFO can then be adjusted by a different (or even the same) LFO (or VFO)

adjust the pitch (frequency) using a different oscillator:

    phase_input=lfo1, lfo2, ..., or lfo10
    phase_input_m=<float>, which multiplies the oscillator, e.g. 1.0, 13.5

adjust the amplitude (volume) using a different oscillator:

    amp_input=lfo1, lfo2, ..., or lfo10
    amp_input_m=<float>, which multiplies the input oscillator level

