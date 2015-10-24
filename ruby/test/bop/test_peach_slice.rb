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

puts $numbers
puts $groups

puts "initializing array"
$array = (0..$numbers).to_a.shuffle.take($numbers)
puts "array is #{$array.length} long"
$partial_sums = Array.new($groups, 0)

$array.peach_slice do |n|
	$partial_sums[PPR.spec_order] += n**10 * n**20 - n**30 +1
end

puts "partial sums"
puts $partial_sum
puts "final sum"
puts $partial_sums.inject{|sum,x| sum + x }
