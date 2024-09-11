.section .rodata
.global rpConfigStart
rpConfigStart:
.incbin "config.json"
.global rpConfigEnd
rpConfigEnd:
.global invadersHStart
invadersHStart:
.incbin "invaders-h.bin"
.global invadersHEnd
invadersHEnd:
.global invadersGStart
invadersGStart:
.incbin "invaders-g.bin"
.global invadersGEnd
invadersGEnd:
.global invadersFStart
invadersFStart:
.incbin "invaders-f.bin"
.global invadersFEnd
invadersFEnd:
.global invadersEStart
invadersEStart:
.incbin "invaders-e.bin"
.global invadersEEnd
invadersEEnd:
