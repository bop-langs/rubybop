def init( data_size )
  puts "#{Process.pid}: initializing #{(@data_size/1000)} thousand numbers"
  $data = Array.new(data_size, 0)
  #assert($data != nil);
  $sum = 0;

end

def lots_of_computation_on_block( s, e )
  total = 0;
  for i in (s ... e) do
    total += ($data[i]**2) + ($data[i]**3) - ($data[i]**5) + 1
  end
  return total;
end


if (ARGV.length>3 || ARGV.length<2 )
  abort("Usage: %s array_size-in-thousands num-blocks", ARGV[0])
end

@data_size = (ARGV[0].to_i)*1000
#assert(@data_size>0)
@num_blocks = ARGV[1].to_i
#assert(@num_blocks>0)

init(@data_size)

puts"#{Process.pid}: adding #{(@data_size/1000)} thousand numbers"
@block_size = ( @data_size / @num_blocks ).ceil

while ( @data_size > 0 )
  @block_end = @data_size
  @data_size -= @block_size
  @block_begin = @data_size >= 0 ? @data_size : 0

  PPR {
    @block_sum = lots_of_computation_on_block( @block_begin, @block_end )
    Ordered {
      $sum += @block_sum
    }
  }
end
PPR.over
puts "#{Process.pid}: The sum is #{($sum/1000.0)} thousand (#{$sum.to_f}) "
