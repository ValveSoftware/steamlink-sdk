.function update_tap_weights
.dest 4 w float
.source 4 xf float
.floatparam 4 mikro_ef
.temp 4 tmp float

mulf tmp, mikro_ef, xf
addf w, w, tmp
