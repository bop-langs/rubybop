$LOAD_PATH.map! { |var|
  var = var.gsub('ruby', 'rubybop') if ! var.include?('rubybop')
}
require 'rubygems.rb' if defined?(Gem)
