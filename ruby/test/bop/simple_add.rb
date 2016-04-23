$numbers = ARGV[0].to_i * 1000
$groups = ARGV[1].to_i

$array = (0..$numbers).to_a.shuffle.take($numbers)
$arrays = $array.each_slice($numbers/$groups).to_a
$partial_sums = Array.new($groups, 0)

puts"#{Process.pid}: adding #{($numbers/1000)} thousand numbers"
$arrays.each_with_index do |arr, spec_group|
  PPR{
    arr.each{|n| $partial_sums[spec_group] += n**10 * n**20 - n**30 +1}
  }
end

PPR.over
sum = $partial_sums.inject{|sum,x| sum + x }
puts "#{Process.pid}: The sum is #{(sum/1000.0)} thousand (#{sum.to_f}) "
