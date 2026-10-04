# String storage

Use flash strings for constant text and format strings in gameplay code rather
than storing them in RAM. Wrap literals in `F(...)` (or use `PROGMEM` /
`AVM_PROGMEM` declarations). The AVM runtime library provides C++ overloads
that accept flash strings through the ordinary API names, such as `snprintf`
and `avm_draw_text`; use those overloads. ArduRogue 2's own `status_word` also
provides a flash-string overload. RAM buffers remain appropriate for text
constructed at runtime.
