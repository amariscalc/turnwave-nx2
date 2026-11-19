.section .rodata
.balign 16
.global ui_home
ui_home:
.incbin "build/ui/home.rgba"
.balign 16
.global ui_opening
ui_opening:
.incbin "build/ui/opening.rgba"
.balign 16
.global ui_failed
ui_failed:
.incbin "build/ui/failed.rgba"
.section .note.GNU-stack,"",%progbits
