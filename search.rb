#!/usr/bin/ruby
require 'find'

$pre_terms = ["VALUE"]
$search_terms = [/\(.*VALUE.*\)/, /\(.*\)/, /\{/]
$next_terms = ["{"]
$bad_terms = ["//", "for", "if", "while", "switch", /\t/ , "\\", "*"]
$bad_pre_terms = $bad_terms << "{}"
$bad_next_terms = $bad_terms

$cutoff = 14



def search(terms, line)
  i = 0
  terms.each do |term|
    unless line.scan(term).empty?
      i+=1
    end
  end
  return i
end

def find_args (line)
  vals = Array.new()
  words = line.split(/\W+/)
  words.each_with_index do |word, word_no|
    if word == "VALUE"
      vals << words[word_no+1]
    end
  end
  return vals
end

def parse_file(file_name)
  has_bop_api = false
  first_arg = 0
  file = File.readlines(file_name)
  file.each_with_index do |line, line_no|
    unless line.scan("bop_api.h").empty?
      has_bop_api = true
    end
    prev_line = file[line_no -1]
    next_line = file[line_no +1]
    i = 0
    i += 5*search($search_terms, line)
    i -= 10*search($bad_terms, line)
    unless line_no == 0
      i += 2*search($pre_terms, prev_line)
      i -= 10*search($bad_pre_terms, prev_line)
    end
    unless file[line_no+1].nil?
      i += 5*search($next_terms, next_line)
      i -= 10*search($bad_next_terms, next_line)
    end

    if i >= $cutoff
      args = find_args(line)
      puts "score = #{i}"
      puts "#{file_name}:\t #{line_no-1}: #{prev_line}"
      puts "#{file_name}:\t #{line_no}: #{line}"
      puts "#{file_name}:\t #{line_no+1}: #{next_line}"
      puts
      args.each do |arg| file.insert(line_no+2, "BOP_obj_use_promise(#{arg});") end
      puts
      if first_arg == 0
        first_arg = line_no
      end
    end
  end
  unless has_bop_api
    file.insert(first_arg - 2, "#include bop_api.h")
  end
  if File.exists?(file_name<<".test")
    File.delete(file_name)
  end
  File.open(file_name, mode="w+") do |f|
    f.puts(file)
  end
end

def find_files(path)
  files = Array.new()
  cfiles = Array.new()
  Find.find(path) do |file_name|
    if file_name.scan(/\.c\Z/).empty?
      files << file_name
    else
      #puts file_name
      cfiles << file_name
    end
  end
  return files, cfiles
end

other, dotc = find_files(ARGV[0])
dotc.each do |f| parse_file(f) end
