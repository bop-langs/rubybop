t1 = Time.now.to_f

puts "start"
$a = 0


5.times do
  PPR{ sleep(1); Ordered{ $a += 1 } }
end

PPR.puts "a is #{$a}"

t2 = Time.now.to_f

PPR.puts((((t2 - t1)*10).truncate/10.0).to_s + " second(s)")
