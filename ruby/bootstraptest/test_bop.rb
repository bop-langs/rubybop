assert_equal %q{[1250, 1250, 1250, 1250, 5000]} ,%q{
  $numbers = 5000
  $groups = 4
  $array = (0..$numbers).to_a.shuffle.take($numbers)
  $arrays = $array.each_slice($numbers/$groups).to_a
  $partial_sums = Array.new($groups, 0)
  $arrays.each_with_index do |arr, spec_group|
    PPR{
      arr.each{|n| $partial_sums[spec_group] += n**10 * n**20 - n**30 +1}
    }
  end

  PPR.over
  $partial_sums << $partial_sums.inject{|sum,x| sum + x }

}


assert_equal %q{4000}, %q{
  def lots_of_computation_on_block( s, e )
    $total = 0;
    for i in (s ... e) do
      $total += ($data[i]**2) + ($data[i]**3) - ($data[i]**5) + 1
    end
    return $total;
  end

  $data_size = 4000
  $num_blocks = 2

  $data = Array.new($data_size, 0)
  $sum = 0;

  $block_size = ($data_size / 2 ).ceil

  while ( $data_size > 0 )
    $block_end =$data_size
    $data_size -= $block_size
    $block_begin =$data_size >= 0 ?$data_size : 0

    PPR {
      $block_sum = lots_of_computation_on_block( $block_begin, $block_end )
      Ordered {
        $sum += $block_sum
      }
    }
  end
  PPR.over
  $sum
}
