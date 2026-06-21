# 07. Future Domain Bridge

This document intentionally separates future domain use from the core CINM
project.

## Current Boundary

CINM is currently domain-neutral.

It should not import assumptions from:

- music generation
- ADG
- Drum Engine
- PC4MS
- Magenta
- EnCodec
- language agents
- any other specific application

The first work is the learning substrate.

## Why Keep The Boundary

If the first prototype is domain-specific, it may be unclear whether results
come from:

- the memory substrate
- domain heuristics
- handcrafted features
- evaluation bias
- existing deterministic machinery

The first research phase should make the substrate visible.

## Later Music Case Study

Music remains an attractive future case study because it naturally contains:

- repeated contextual preference
- small corrections
- personal taste
- non-stationary goals
- local feedback
- structured candidates
- need for inspectability

But music should come after:

- toy task behavior is understood
- update rules are stable enough to compare
- event logs and snapshots are defined
- explanation reports exist

## Later Agent Preference Case Study

Another possible case study is local agent preference memory:

- user chooses between two outputs
- agent records local preference
- future behavior changes in similar contexts
- learned behavior stays inspectable

This may be a simpler first domain case than music.

## Bridge Rule

When a domain bridge is added, it should be one adapter around the substrate,
not a rewrite of the substrate.

```text
domain event
  -> domain encoder
  -> CINM context/features
  -> CINM update/readout
  -> domain adapter output
```

The CINM core should remain testable without the domain.

