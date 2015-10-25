module Enumerable
	def peach
    		each{|y| PPR{yield(y)}}
  	end

  	def peach_with_index
    		each_with_index{|i,n| PPR{yield(i,n)}}
  	end

  	def peach_slice
		n = PPR.get_group_size
		each_slice(to_a.size/n){|e|
			PPR {
				e.each{ |element| yield element }
			}
		}
		PPR.over
	end
end

$numbers = ARGV[0].to_i
$groups = ARGV[1].to_i

puts "initializing array"
$array = (0..$numbers).to_a.shuffle.take($numbers)
puts "array is #{$array.length} long"
$arrays = $array.each_slice($numbers/$groups).to_a
$partial_sums = Array.new($groups, 0)
$arrays.peach_with_index do |arr, spec_group|
    arr.each{|n| $partial_sums[spec_group] += n**10 * n**20 - n**30 +1}
end

PPR.over
puts "partial sums as follows"
puts $partial_sums
puts "final sum"
puts $partial_sums.inject{|sum,x| sum + x }
