# this is adapted from krzYszcz's code for coll in cyclone

# package provide pdtk_textwindow 0.1

proc pdtk_textwindow_out {name} {
    if {[winfo exists $name]} {
       set lin [$name.text get 1.0 end]
           if {$lin != ""}
               pdsend [concat $name textout $lin]
            }
        }
    pdtk_textwindow_setdirty $name 0
}

