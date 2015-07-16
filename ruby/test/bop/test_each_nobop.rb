puts "start"
a = 1
b = 2
c = 3
d = 4
puts a

#PPR.yield
#  {
  sleep(1); a=2; a = "FIRE" ; PPR.promise(a)
#  }
#PPR.yield
#  {
  sleep(1); puts b; b="good2"; PPR.promise(b)
#  }
#PPR.yield
#  {
  sleep(1); puts c; c="goodgood3"; PPR.promise(c)
#  }
#PPR.yield
#  {
  sleep(1); puts d; d="goodgoodgood4"; PPR.promise(d)
#  }





puts a
puts b
puts c
puts d

puts "finish"
