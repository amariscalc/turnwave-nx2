.section .rodata
.balign 16
.global tw_home
tw_home:
.incbin "build/ui/home.rgba"
.balign 16
.global tw_font16
tw_font16:
.incbin "build/ui/font16.alpha"
.balign 16
.global tw_font22
tw_font22:
.incbin "build/ui/font22.alpha"
.balign 16
.global tw_font32
tw_font32:
.incbin "build/ui/font32.alpha"
.balign 16
.global tw_width16
tw_width16:
.incbin "build/ui/font16.widths"
.balign 16
.global tw_width22
tw_width22:
.incbin "build/ui/font22.widths"
.balign 16
.global tw_width32
tw_width32:
.incbin "build/ui/font32.widths"
.section .note.GNU-stack,"",%progbits
