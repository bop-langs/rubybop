set follow-fork-mode child
set detach-on-fork off
catch fork
r test/bop/simple_add.rb 5000 2
s
set follow-fork-mode parent
