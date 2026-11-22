# 1. Record architecture decisions

* Status: Accepted
* Date: 2026-11-22

## Context

The project is about to grow from a Python simulator into a multi-language,
multi-target code base (Python on Windows, C/C++ on bare-metal Arm cores). Decisions
such as "why no heap", "why double-buffering" or "why this core runs the loop" are
easy to forget and expensive to re-argue.

## Decision

Significant decisions are recorded as numbered Markdown files in `docs/adr/`, one
decision per file, with context, decision and consequences.

## Consequences

Reviewers can point a pull request at an ADR instead of repeating the discussion. A
decision that turns out to be wrong is superseded by a new record, so the history of
the reasoning stays visible.
