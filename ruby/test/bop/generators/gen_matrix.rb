size = ARGV[0].to_i
puts "#{size} #{size} #{size*size}"
size.times do |ii|
  i = ii+1
  size.times do |jj|
    j = jj+1
    puts "#{i} #{j} #{[i,j].min * 2.0}"
  end
end
puts "0 0 0"
