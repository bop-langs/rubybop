
t1 = Time.now.to_f

puts "start"
$a = 0

PPR.new{ sleep(1); $a = 1 }.call
PPR.new{ sleep(1); $b = 2 }.call
PPR.new{ sleep(1); $c = 3 }.call
PPR.new{ sleep(1); $d = 4 }.call

PPR.puts "a is #{$a}\nb is #{$b}\nc is #{$c}\nd is #{$d}"

t2 = Time.now.to_f

PPR.puts((((t2 - t1)*10).truncate/10.0).to_s + " second(s)")
