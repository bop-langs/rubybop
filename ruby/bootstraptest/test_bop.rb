assert_equal %q{goodgoodgoodgood}, %q{
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

$a+$b+$c+$d
}
