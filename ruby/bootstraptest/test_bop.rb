assert_finish 5, %q{
$a = "1"
$b = "2"
$c = "3"
$d = "4"

PPR {
  $a="good"
  sleep(1)
  }
PPR {
  $b="good"
  sleep(1)
  }
PPR {
  $c="good"
  sleep(1)
  }
PPR {
  $d="good"
  sleep(1);
  }

unless($a+$b+$c+$d == "goodgoodgoodgood")
  sleep(6)
end

}
assert_finish 4 ,%q{
$a = [0, 0, 0, 0]
PPR {
	sleep(1)
	$a[0] = 1
}
PPR {
	sleep(1)
	$a[1] = 2
}
PPR {
	sleep(1)
	$a[2] = 3
}
PPR {
	sleep(1)
	$a[3] = 4
}

$a == [1,2,3,4]
}
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
