#!/usr/bin/ruby
require 'find'

$pre_terms = ["VALUE", "static", "void"]
$search_terms = [/\(.*VALUE.*\)/, /\(.*\)/, /\{/]
$next_terms = ["{"]
$blacklist = ["rb_obj_memsize_of"]
$bad_terms = ["//", "for", "if", "while", "switch", /\t/ , "\\", "*", ";"]+$blacklist
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

class String
  def string_between_markers marker1, marker2
    self[/#{Regexp.escape(marker1)}(.*?)#{Regexp.escape(marker2)}/m, 1]
  end
end

def find_args (line)
  vals = Array.new()
  args = line.string_between_markers("(", ")")
  words = args.split(/\W+/)
  words.each_with_index do |word, word_no|
    if word == "VALUE"
      unless words[word_no+1] == "const"
        vals << words[word_no+1]
      else
        vals << words[word_no+2]
      end
    end
  end
  return vals
end

def parse_file(file_name)
  #has_internal = false
  #first_arg = 0
  file = File.readlines(file_name)
  file.each_with_index do |line, line_no|
    #unless line.scan("internal.h").empty?
    #  has_internal = true
    #end
    prev_line = file[line_no -1]
    next_line = file[line_no +1]
    i = 0
    i += 5*search($search_terms, line)
    i -= 10*search($bad_terms, line)
    unless line_no == 0
      i += 1*search($pre_terms, prev_line)
      i -= 10*search($bad_pre_terms, prev_line)
    end
    unless file[line_no+1].nil?
      i += 5*search($next_terms, next_line)
      i -= 10*search($bad_next_terms, next_line)
    end

    if i >= $cutoff
      args = find_args(line)

      args.each do |arg| file.insert(line_no+2, "BOP_obj_use_promise(#{arg});") end

      #if first_arg == 0
      #  first_arg = line_no
      #end
    end
  end
  #unless has_internal
    #if first_arg > 0
      #file.insert(first_arg - 1, "#include \"internal.h\"")
    #end
  #end
  if File.exists?(file_name)
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
