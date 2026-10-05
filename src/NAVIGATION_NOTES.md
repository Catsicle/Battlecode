# Navigation notes

- `helper.hpp` is organiser-provided and intentionally unchanged.
- The helper exposes raw sonar messages but no message schema, role identity, or
  target assignment field.
- Navigation-target payload encoding is a placeholder.
- A compile-time target fixture exists for local Navigate-mode testing.
- Symmetry inference is disabled.
- One-time map-sized allocation is allowed.
