n = ARGV[0].to_i
g = ARGV[1].to_i
$partial_sums = Array.new(g+1, 0)
n.times.peach_slice(n/g){|x| 
	o = PPR.spec_order
 	if o >= 0 || o < g
		$partial_sums[o]= x.inject(0){|sum, i| sum += i**30-(i**20*i**10)+1}
	else
		$partial_sums[g]= x.inject(0){|sum, i| sum += i**30-(i**20*i**10)+1}
	end
}
puts $partial_sums
puts $partial_sums.inject(0){|sum, i| sum += i}
