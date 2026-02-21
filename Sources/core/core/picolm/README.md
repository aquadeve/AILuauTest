# PicoLM integration notes

This directory contains a vendored copy of the PicoLM standalone runtime.

## How core now communicates with PicoLM

`Sources/core/core/body.cpp` adds a new body type named `PicoLmBody`.
It bridges Arnold's sensor/actuator loop with PicoLM:

- pulls prompt text from an actuator terminal (default: `Prompt`)
- runs PicoLM with that prompt (`picolmBinary` + `picolmModel`)
- pushes generated text back into a sensor terminal (default: `Response`)

If PicoLM binary/model are not configured or the process fails, it falls back to
an inline response string so the simulation loop still works.

## Expected body params

Use `MultiByte` spike terminals for text, for example:

- actuator `Prompt` with `spikeType: "MultiByte"`, `spikeAllocCount: 1`
- sensor `Response` with `spikeType: "MultiByte"`, `spikeAllocCount: 1`

The terminal `size` controls max bytes transferred each step.
