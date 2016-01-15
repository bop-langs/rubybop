bop_ldpath = $LOAD_PATH.map { |var|
  var = var.gsub('ruby', 'rubybop') if ! var.include?('rubybop')
  var
}
$LOAD_PATH.concat bop_ldpath
require 'rubygems.rb' if defined?(Gem)
