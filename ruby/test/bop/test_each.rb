puts "start"
$a = "1"
$b = "2"
$c = "3"
$d = "4"

PPR.yield{
  $a="good"
  sleep(1)
  }
PPR.yield{
  $b="good"
  sleep(1)
  }
PPR.yield{
  $c="good"
  sleep(1)
  }
PPR.yield{
  $d="good"
  sleep(1);
  }

puts $a
puts $b
puts $c
puts $d

puts "finish"
