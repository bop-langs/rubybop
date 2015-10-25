bop_ldpath = $LOAD_PATH.map { |var|
  var = var.gsub('ruby', 'rubybop') if ! var.include?('rubybop')
  var
}
$LOAD_PATH.concat bop_ldpath
require 'rubygems.rb' if defined?(Gem)
module Enumerable
	def peach
    		each{|y| PPR{yield(y)}}
  	end
  
  	def peach_with_index
    		each_with_index{|i,n| PPR{yield(i,n)}}
  	end

  	def peach_slice(n)
		each_slice(n){|e|
			PPR{yield e}
		}
	end
		
end
