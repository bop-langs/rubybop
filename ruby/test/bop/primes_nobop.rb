require "prime"
$input = ARGF.map{|i| i.to_i}
$output_array = Array.new $input.size, 0
$sum = 0

$input.each_with_index do |i, j|
    if i == 1
        $output_array[j]= 1
        next
    end
    if Prime.prime?(i)
        $output_array[j]= i
        next
    end
    t = i/2
    t.times do |x|
        if i.gcd(x) == x
            if Prime.prime?(i/x)
               $output_array[j]= i/x
               break
            end
        end
    end
end

puts $output_array
