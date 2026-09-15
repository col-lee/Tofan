# ToFan SD Music Smooth Patch

Apply these files over the latest `ToFan_ChatHistory_Stability_Fixed` project, preserving paths.

Main changes:
- SD SPI target 20 MHz with 10/4 MHz fallbacks.
- Local audio low-water telemetry and cooperative refill passes.
- Microphone capture pinned to Core 1 at priority 5 so it cannot preempt Core 0 music service.
- `/api/status` diagnostics for SD/audio starvation.

Build with PlatformIO after applying the patch.
