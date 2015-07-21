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


