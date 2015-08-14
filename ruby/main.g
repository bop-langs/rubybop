set follow-fork-mode child
set detach-on-fork off
catch fork
r test/bop/add_map.rb 2000 2
s
set follow-fork-mode parent
