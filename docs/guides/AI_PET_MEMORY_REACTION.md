# AIpet false busy reaction during Gemini Live

The old mood check remembered the highest free heap and reported pressure below
58% of that value. Allocating TLS and live audio buffers could keep a healthy
session below that percentage indefinitely, repeatedly selecting
`so busy... tiny break?` instead of the listening/speaking mood.

The high-water percentage comparison is removed. The existing sustained low
memory checks remain: internal heap below 36 KiB or available PSRAM below
384 KiB for more than 2200 ms. Normal Gemini allocations no longer trigger
the expression merely because they reduce free memory from the idle baseline.
This changes the facial reaction; it does not stop audio or alter Gemini setup.

`test/test_pet_memory.py` exercises the production check with 200 KB idle heap,
80 KB during repeated connections, short low-memory spikes, genuinely sustained
low heap/PSRAM, recovery, devices without PSRAM, and timer wrap.
Hardware session validation remains necessary; no board was flashed.
