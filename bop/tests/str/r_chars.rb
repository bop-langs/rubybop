#!/usr/bin/ruby

=begin
6/22/2011
Creates a string of random ASCII characters of user specified length
use '>' operator to redirect to file

writing M after the number of characters means megabytes
so 2M will produce 2 megabytes worth of characters, a K
will produce 2 kilobytes worth of characters
=end

match = /^(\d+)\s*([kKmM])?$/.match(ARGV[0])
unless match
  abort 'Enter number of chars as command line arguments'
end

mult = case match[2]
  when 'k', 'K' then 2**10
  when 'm', 'M' then 2**20
  else 1
end

num = match[1].to_i * mult

# The ASCII characters between 97 and 122 (inclusive) are
# all lowercase ASCII letters
str = ''
1.upto(num) do |n|
	str << (26 * rand).to_i + 97
end

puts str
