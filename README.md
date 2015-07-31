# rubybop
Safe parallel Ruby language based on [BOP](http://roclocality.org/2015/05/17/rubybop-introduction/)

[Ruby Version 2.2 commit c5a6913](https://github.com/ruby/ruby/tree/c5a691323201ace5f5299b6914c8e1709918c521)

##Build Status

Master  | Latest
------------- | -------------
[![Master Status](https://travis-ci.org/dcompiler/rubybop.svg?branch=master)](https://travis-ci.org/dcompiler/rubybop)  | [![Latest Status](https://travis-ci.org/dcompiler/rubybop.svg)](https://travis-ci.org/dcompiler/rubybop)


##Build & Test & Install
0. Prereqs: To install rubybop, you will need (on path):
  * rake
  * make
  * autoconf
1. Run `rake`
2. Run `rake test` to ensure everything is working (optional).
3. Run `rake install`. May require `sudo`.

###Unsupported Operations
Rubybop currently does not support:
- Terminal input (which is needed for IRB)
- Ruby's exec calls (use the system call instead. Similar semantics)
- Ruby's signal handlers are not installed as they are needed for the bop library.
- Functions to change the process group id of ruby processes. Again, these need to stay the same for library.

In addition, all ruby code that was executed with the GVL is now forced to use the GVL. The __only__ exception to this rule is IO, since Ruby is too slow if IO requires locks.


Any issues installing should be reported here.
Note: There is currently no working IRB (interactive ruby interpreter).


[(outdated) Work Notes etc.](https://docs.google.com/document/d/1qkXeVAgK56vHWjxyXntOxC4MxRF4oelftWkvHx1V8eM/edit?usp=sharing)
