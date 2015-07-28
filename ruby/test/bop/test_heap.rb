#!/usr/bin/ruby



#Class RaceTest holds an instance int that should vary along each iteration of the loop
#After allocation occurs in PPR mode, we should expect that every single of the RaceTest instances have a different number
#If any number is missing, it could be a sign that an instance was allocated into the same address in the RHeap as another, causing the overwrite
class RaceTest
  def initialize(x)
    @num = x
  end
  def printNo()
    puts @num
  end
end


$i = 0
$n = 5000
#Declare a test instance n times, assign it a unique identifier
while  ($i <= $n)
  inst = RaceTest.new($i)
  inst.printNo
  $i += 1
end
