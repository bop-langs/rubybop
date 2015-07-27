assert_finish 4, %q{
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
