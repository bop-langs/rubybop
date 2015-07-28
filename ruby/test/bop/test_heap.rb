#!/usr/bin/ruby


class RaceTest
  def initialize(x)
    @num = x
  end
  def printNo()
    puts @num
  end
end


$i = 0
while  ($i <= 5000)
  inst = RaceTest.new($i)
  inst.printNo
  $i += 1
end
