# omsx-dump.tcl -- for openMSX (-script): no picture, no sound, as fast as it
# goes; at each time in OMSX_TIMES (seconds) VRAM and the VDP's registers
# (16K + 8 bytes) go to OMSX_PREFIX<time>.vram, and after the last it quits.
# OMSX_KEYS: "time row mask down|up ..." keyboard matrix presses on the way.
set renderer none
set throttle off
set mute on
proc dumpit {t last} {
    global env
    set f [open "$env(OMSX_PREFIX)$t.vram" wb]
    puts -nonewline $f [debug read_block VRAM 0 0x4000]
    puts -nonewline $f [debug read_block {VDP regs} 0 8]
    close $f
    if {$last} { exit }
}
set times [lsort -real $env(OMSX_TIMES)]
set n [llength $times]
for {set i 0} {$i < $n} {incr i} {
    set t [lindex $times $i]
    after time $t [list dumpit $t [expr {$i == $n - 1}]]
}
if {[info exists env(OMSX_KEYS)]} {
    foreach {t row mask how} $env(OMSX_KEYS) {
        if {$how eq "down"} { after time $t [list keymatrixdown $row $mask] } else { after time $t [list keymatrixup $row $mask] }
    }
}
