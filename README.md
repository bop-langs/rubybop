# Rubybop

[![Join the chat at https://gitter.im/bop-langs/rubybop](https://badges.gitter.im/bop-langs/rubybop.svg)](https://gitter.im/bop-langs/rubybop?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge)
Safe parallel Ruby language based on [BOP](http://roclocality.org/2015/05/17/rubybop-introduction/)

[Ruby Version 2.2 commit c5a6913](https://github.com/ruby/ruby/tree/c5a691323201ace5f5299b6914c8e1709918c521)

##Build Status

Master  | Latest | Trunk |  CBop Dependency 
------------- | ------------- | ------------- | -------------
[![Master Status](https://travis-ci.org/bop-langs/rubybop.svg?branch=master)](https://travis-ci.org/bop-langs/rubybop)| [![Latest Status](https://travis-ci.org/bop-langs/rubybop.svg)](https://travis-ci.org/bop-langs/rubybop) | [![Trunk Status](https://travis-ci.org/bop-langs/rubybop.svg?branch=trunk)](https://travis-ci.org/bop-langs/rubybop ) |[![CBop Status](https://travis-ci.org/bop-langs/cbop.svg?branch=master)](https://travis-ci.org/bop-langs/cbop)

##Example
Several exaple scripts are included in [ruby/test/bop](/ruby/test/bop). The simplest test is the [simple_add.rb](/ruby/test/bop/simple_add.rb) which adds a large set of numbers in parallel using arrays. The total sum is the first number passed in as an argument to the script. The difference between a sequential program (in regular Ruby) and the Rubybop equivalent is just adding a PPR call:
```ruby
PPR{
  arr.each{|n| $partial_sums[spec_group] += n**10 * n**20 - n**30 +1}
}
```
The programmer does not need to handle race conditions or any other problem from parallel programming since everything is handled by the runtime.

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
- Ruby's exec calls (use the system call instead. Similar semantics)
- Ruby's signal handlers are not installed as they are needed for the bop library.
- Functions to change the process group id of ruby processes. Again, these need to stay the same for library.

###Terminal IO Limitations
A program using BOP will always be able to write to the terminal. Writing to the terminal is more tricky, and is not supported during PPR tasks. What happens a process tries to read from the terminal while in PPR (or in understudy -- any time multiple processes are reading from STDIN) is undefined.

IRB/IRBOP is supported in early stages. To run IRB with the Rubybop interpreter, Rubybop must be installed. To run the actual script, run the irb in <repo>/ruby/irbop. This will use the installed interpreter. You will know that the bop interpreter is running if BOP_Version is defined (eg puts BOP_Version doesn't error). Also note that scrolling through commands (up and down arrows) currently does not work and will terminal the program once enter is hit.


###Threading
In addition, all ruby code that was executed without the GVL is now forced to use the GVL. The __only__ exception to this rule is IO, since Ruby is too slow if IO requires the GVL. If a script attempts to enter a parallel region with multiple threads running, an `ThreadError` is thrown and Rubybop will terminate. Although there are currently no checks, the data structures used by BOP do not support concurrent modification, so only one thread should be active inside a PPR region.



Any issues installing or running should be reported [here](https://github.com/bop-langs/rubybop/issues).
