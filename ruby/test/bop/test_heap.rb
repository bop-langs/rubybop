#!/usr/bin/ruby


class RaceTest
  def initialize(x)
    @num = x
  end
  def printNo()
    puts @num
  end
end



inst = RaceTest.new(5)
inst.printNo
