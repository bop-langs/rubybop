require "mkmf"

abort "missing malloc()" unless have_func "malloc"
abort "missing free()" unless have_func "free"

create_makefile "my_malloc/my_malloc"
