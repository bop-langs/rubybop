#!/bin/env ruby

if (ARGV.length < 1) then
	puts "Arguments: [file]"
end

file = ARGV[0]

symtab = `readelf -W -s #{file}`

lines = symtab.split("\n")
lines.each { |line|
	sp = line.split()
	if (sp[3] == "OBJECT") then
		if (sp[4] == "GLOBAL") then
			puts sp[7]
		end
	end
}
