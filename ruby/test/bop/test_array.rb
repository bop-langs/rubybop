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
PPR.over
puts $a
