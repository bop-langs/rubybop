# Tests ppr allocation of an instance variable and retrieves it

$number1 = ARGV[0].to_i
$number2 = ARGV[1].to_i


puts "creating new instance variable"

PPR {
    @var1 = $number1
}
PPR {
    @var2 = $number2
}
PPR.over
puts "instance variable 1: #{@var1}"
puts "instance variable 1: #{@var2}"
