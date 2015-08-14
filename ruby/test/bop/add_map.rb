$numbers = ARGV[0].to_i
$groups = ARGV[1].to_i

puts "initializing array"
$array = (0..$numbers).to_a.shuffle.take($numbers)
puts "array is #{$array.length} long"
$arrays = $array.each_slice($numbers/$groups).to_a
$partial_sums = Array.new($groups, 0)
$arrays.ppr_each{
    arr.each{|n| $partial_sums[PPR.spec_order] += n**10 * n**20 - n**30 +1}
}

PPR.over
puts "partial sums as follows"
puts $partial_sums
puts "final sum"
puts $partial_sums.inject{|sum,x| sum + x }
