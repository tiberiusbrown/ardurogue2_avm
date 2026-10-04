# String storage

Use flash strings for constant text and format strings in gameplay code rather
than storing them in RAM. Wrap literals in `F(...)` (or use `PROGMEM` /
`AVM_PROGMEM` declarations). The AVM runtime library provides C++ overloads
that accept flash strings through the ordinary API names, such as `snprintf`
and `avm_draw_text`; use those overloads. ArduRogue 2's own `status_word` also
provides a flash-string overload. RAM buffers remain appropriate for text
constructed at runtime.

# Equipment

Equipment type/subtype definitions determine inherent damage range, accuracy,
and armor rating. `Item::info` stores only per-instance state such as signed
enchantment, curse, and identification. Never derive inherent capability from
`info` or pack ratings/tiers together with enchantment. Enchantment changes only
the roll distribution within the type's fixed range.
