class BOP_add
  $sum = 0
  def main()
    if (ARGV.length>3 || ARGV.length<2 )
      abort("Usage: %s array_size-in-millions num-blocks")
    end

    @data_size = (ARGV[0].to_f)*1000000
    @num_blocks = ARGV[1].to_i

    init(@data_size)

    puts"#{Process.pid}: adding #{(@data_size/1000000)} million numbers"
    @block_size = ( @data_size / @num_blocks ).ceil

    while ( @data_size > 0 )
      @block_end = @data_size
      @data_size -= @block_size
      @block_begin = @data_size >= 0 ? @data_size : 0

      PPR {
        @block_sum = lots_of_computation_on_block( @block_begin, @block_end )
        # what are the functions for bop_ordered and bop_write?
	puts @block_sum
        $sum += @block_sum
      }


    end #end while
    puts "#{Process.pid}: The sum is #{(sum/1000000.0)} million (#{@sum.to_f}) "

  end #end main

  def init( data_size )
    puts "#{Process.pid}: initializing #{(data_size/1000000)} million numbers"
    $data = Array.new(data_size, 0)
    $data.each { |i| $data[i] = i }
    $sum = 0;

  end

  def lots_of_computation_on_block( s, e )
    @total = 0;
    @j = s
    while @j < e do
      @total += (($data[@j] ** 10) * ($data[@j] ** 10) - $data[@j]**20) + 1
      @j += 1
    end
    # BOP_record_read( &data[j], sizeof( double )*(end - start) ); What are the ruby commands that we have for bop_read, bop_write, and bop_ordered?
    return @total;
  end

end #class end

run = BOP_add.new
run.main
