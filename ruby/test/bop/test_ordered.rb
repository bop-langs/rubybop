@a = 0
5.times do
  PPR{ sleep(1); Ordered{ @a += 1 } }
end
PPR.over
puts @a
