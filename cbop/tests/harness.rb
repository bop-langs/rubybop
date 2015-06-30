#!/usr/bin/ruby

if (ARGV.length < 1) then
	puts "Usage: harness [testname]"
end

testname = ARGV[0]

if (!File.directory?(testname)) then
	puts "#{testname} is not a directory!"
end

args = ARGV.slice(1,ARGV.length-1).join(" ")

suffix = ".txt"
des_seq = "_seq"
des_bop = "_bop"
out = ".out"
err = ".err"
stdout_seq = "#{testname}#{des_seq}#{out}#{suffix}"
stderr_seq = "#{testname}#{des_seq}#{err}#{suffix}"
stdout_bop = "#{testname}#{des_bop}#{out}#{suffix}"
stderr_bop = "#{testname}#{des_bop}#{err}#{suffix}"

puts "Building #{testname} sequential version..."
`cd #{testname}; rake seq`
puts "Building #{testname} BOP version..."
`cd #{testname}; rake`
puts "Running #{testname} sequential version..."
`cd #{testname}; ./#{testname}_seq #{args} 2>../#{stderr_seq} >../#{stdout_seq}`
puts "Running #{testname} BOP version..."
`cd #{testname}; ./#{testname}_bop #{args} 2>../#{stderr_bop} >../#{stderr_seq}`
puts "Comparing #{testname} stdout..."
`diff #{stdout_seq} #{stdout_bop}`
puts "Comparing #{testname} stderr..."
`diff #{stderr_seq} #{stderr_bop}`
