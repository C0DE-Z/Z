# Z Brand Guide

## Brand character

Z is a focused creative tool: technical, direct, and a little dangerous. The visual system should feel like a dark editing suite with a precise pink signal—not a neon cyberpunk theme.

**Use:** deep black surfaces, restrained pink emphasis, clear typography, thin borders, and generous empty space.

**Avoid:** purple gradients, glow effects, decorative blobs, rounded “pill” UI, and pink body text.

## Core palette

| Token | Value | Purpose |
| --- | --- | --- |
| Canvas | `#08080A` | Page and application background |
| Surface | `#111116` | Navigation, panels, and grouped regions |
| Raised surface | `#19191F` | Inputs, code headers, and hover surfaces |
| Divider | `#303036` | Default borders and separators |
| Strong divider | `#4E4E58` | Container boundaries and inactive controls |
| Primary ink | `#F7F4F6` | Headings and high-emphasis copy |
| Secondary ink | `#C3BEC3` | Body copy and standard navigation |
| Muted ink | `#918B92` | Supporting metadata and secondary labels |
| Z Pink | `#FF4F91` | Primary actions, focus indicators, and brand mark |
| Z Pink Light | `#FF72AA` | Hover states and small highlighted labels |
| Pink wash | `rgba(255, 79, 145, 0.12)` | Selected or informational backgrounds |
| Pink action ink | `#2A0715` | Text/icons placed on Z Pink |

## Color use

- Use **Canvas** as the default background. A page should read primarily as black.
- Use **Surface** for structural separation, not decoration. One surrounding border is enough.
- Use **Z Pink** only for a single primary action per view, focus outlines, active states, and the `Z` wordmark.
- Use **Z Pink Light** for hover and small label treatments; do not use it for paragraph text.
- Pink washes should remain subtle and paired with a pink edge or label so they have a clear meaning.
- Maintain the existing neutral greys for long-form reading and dense controls.

## Typography

| Role | Family | Guidance |
| --- | --- | --- |
| Product and documentation copy | Inter, system UI fallback | Use for all headings, UI labels, and body text. |
| Code, versions, and technical metadata | JetBrains Mono, Consolas fallback | Use sparingly for technical context and code. |

Headings should be high-contrast, compact, and left or centrally aligned according to the page layout. Body text should stay at 14–16px with comfortable line height.

## Components and states

### Primary action

- Background: **Z Pink**
- Foreground: **Pink action ink**
- Hover: **Z Pink Light**
- Shape: 4–6px corner radius; never a capsule unless the control has a clear tag/filter purpose.

### Secondary action

- Transparent or **Surface** background
- **Strong divider** border
- **Primary ink** foreground
- On hover, change the border to **Z Pink** rather than adding a shadow or gradient.

### Panels and cards

- Background: **Surface**
- Border: **Divider**, or **Strong divider** for a major preview/container
- Hover: lift color only to **Raised surface**; no transform, glow, or drop shadow is required.

### Focus and selection

- Keyboard focus: a 2px **Z Pink** outline with a 3px offset.
- Selection: use a **Pink wash** background plus an explicit pink indicator where practical.

## Accessibility checks

- Keep body text to **Secondary ink** or **Primary ink** on Canvas/Surface backgrounds.
- Never depend on pink alone to communicate an error, state, or selection; pair it with labels, borders, icons, or position.
- Verify new text/background combinations with a contrast checker before release.
- Respect `prefers-reduced-motion`; color changes should be brief and non-essential.

## CSS tokens

The website and shader guide define this palette with custom properties. Reuse those tokens instead of introducing raw pinks or purples in new rules.
