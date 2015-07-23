puts $LOAD_PATH
$LOAD_PATH.map! { |var|
  var = var.gsub('ruby', 'rubybop') if ! var.include?('rubybop')
}
puts "new loadpath"
puts $LOAD_PATH
require 'rubygems.rb' if defined?(Gem)
