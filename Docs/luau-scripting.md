# Luau Scripting in `Sources/core/core`

This repository now includes a Luau runtime integration for model components inside the core simulation.

## What was added

A new model family is available through these types:

- `LuauBrain`
- `LuauRegion`
- `LuauNeuron`

Each type accepts a `script` parameter in its JSON params.

## Script lifecycle

- The script is compiled with Luau (`luau_compile`) and loaded into a Luau VM (`luau_load`) when the component is constructed.
- On each simulation step, if your script defines `on_control`, it is called with one argument: `brainStep`.
- During migration/restore (`pup` unpack), the script is reloaded automatically.

## Component contract

### LuauNeuron

`on_control(brainStep: number)` is called from `Neuron::Control`.

Built-in functions injected into the script:

- `log_info(message: string)`
  - Writes a message into the Arnold core log.
- `send_continuous_spike(receiverNeuronId: number, direction: number, intensity: number)`
  - Sends a continuous spike from the current neuron.
  - `direction` values:
    - `0` = forward
    - `1` = backward

Current contribution behavior:

- `ContributeToRegion` returns no data (size `0`).

### LuauRegion

`on_control(brainStep: number)` is called from `Region::Control`.

Current contribution behavior:

- `AcceptContributionFromNeuron` is a no-op.
- `ContributeToBrain` returns no data (size `0`).

### LuauBrain

`on_control(brainStep: number)` is called from `Brain::Control`.

Current contribution behavior:

- `AcceptContributionFromRegion` is a no-op.

## Example scripts

### Minimal neuron script

```lua
function on_control(brainStep)
    if brainStep % 100 == 0 then
        log_info("heartbeat from Luau neuron at step " .. tostring(brainStep))
    end
end
```

### Spike-emitting neuron script

```lua
local targetNeuronId = 4194305 -- replace with a valid NeuronId

function on_control(brainStep)
    if brainStep % 10 == 0 then
        -- forward spike with intensity 1.0
        send_continuous_spike(targetNeuronId, 0, 1.0)
    end
end
```

## Full Luau language/API documentation

For the complete Luau language syntax, semantics, standard library, and type system, use the official Luau documentation:

- Main docs portal: https://luau.org/
- Getting started: https://luau.org/getting-started
- Syntax: https://luau.org/syntax
- Standard library: https://luau.org/library
- Type checking: https://luau.org/typecheck
- Linting: https://luau.org/lint
- Compatibility notes: https://luau.org/compatibility

## Notes

- Scripts run in a sandboxed Luau state (`luaL_sandbox`).
- This integration exposes a focused host API for Arnold simulation control; additional host functions can be added in `luau_model.cpp`.
