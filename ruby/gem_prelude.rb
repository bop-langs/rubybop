bop_ldpath = $LOAD_PATH.map { |var|
  var = var.gsub('ruby', 'rubybop') if ! var.include?('rubybop')
  var
}
$LOAD_PATH.concat bop_ldpath
require 'rubygems.rb' if defined?(Gem)

# TODO this is until we find a better place for this

module Enumerable
        def peach
                self.each{|y| PPR{yield(y)}}
        end

        def peach_with_index
                self.each_with_index{|i,n| PPR{yield(i,n)}}
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
