#!/bin/env ruby

if (ARGV.length < 1) then
	puts "Arguments: [binary]"
end

file = ARGV[0]

symtab = `readelf -W -s #{file}`

funcpref = "_bop_init_"

excludes = []

begin
	File.open("bop_check_excludes").each { |line|
					excludes.push(line.chomp())
	}
rescue Errno::ENOENT
end

globvars = {}

lines = symtab.split("\n")
lines.each { |line|
	sp = line.split()
	if (sp[3] == "OBJECT") then
		if (sp[4] == "GLOBAL") then
			on = sp[7]
			if (!globvars[on]) then
				globvars[on] = false
			end
		end
	elsif (sp[3] == "FUNC") then
		if (sp[4] == "LOCAL") then
			fn = sp[7]
			if (fn.match("^#{funcpref}")) then
				on = fn[funcpref.length(),fn.length()]
				globvars[on] = true
			end
		end
	end
}

globvars.each {|key,val|
	if (!val) then
		matched = false
		excludes.each { |ex|
				if (key.match(ex)) then
						matched = true
						break
				end
		}
		if (!matched) then
				puts key
		end
	end
}
