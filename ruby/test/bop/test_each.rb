puts "start"
a = 1
b = 2
c = 3
d = 4

PPR.yield{
  sleep(1); puts a; a = "FIRE"
  }
PPR.yield{
  sleep(1); puts b; b="good2"
  }
PPR.yield{
  sleep(1); puts c; c="goodgood3"
  }
PPR.yield{
  sleep(1); puts d; d="goodgoodgood4"
  }





puts a
puts b
puts c
puts d

puts "finish"
