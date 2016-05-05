size = ARGV[0].to_i
$i = 0
size.times do |row|
  size.times do |col|
    puts "#{row} #{col} #{([row,col].min * 2.0) % 256 } #{([row,col].max * 3.0) % 256} #{$i = ($i + 1) % 256}"
  end
end
puts "0 0 0.0 0.0"
