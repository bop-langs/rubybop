#!/usr/bin/ruby
require 'find'

$pre_terms = ["VALUE", "static", "void"]
$search_terms = [/\(.*VALUE.*\)/, /\(.*\)/, /\{/, /\(.*\*.*\)/]
$next_terms = ["{"]
$bad_terms = ["//", "for", "if", "while", "switch", /\t/ , "\\", "*", ";"]
$bad_pre_terms = $bad_terms << "{}"
$bad_next_terms = $bad_terms

$black_objs = ["flags"]

#$accepted_files = [/aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa/]
$accepted_files = [/\.c\Z/]
#$accepted_files = ["main.c","thread.c", "loadpath.c", "version.c", "localeinit.c", "dln.c","dmyext.c","dmyext.c", "compile.c", "load.c", "dln_find.c", "goruby.c","eval_jump.c", "enum.c", "enumerator.c","inits.c", "file.c","iseq.c", "pack.c", "safe.c", "marshal.c","random.c", "sparc.c","array.c", "bignum.c", "addr2line.c","error.c", "parse.c", "st.c", "re.c", "node.c", "debug.c"]
$ignored_files = ["miniinit.c", "compar.c", "ppr.c", "ppr_mon.c",
  #"ruby.c", "eval.c", "gc.c", "io.c", "dir.c", "hash.c", "st.c",
  /vm/, /ext/,  /enc/, /missing/, /win32/, /nacl/]

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
      arg = ""
      unless words[word_no+1] == "const"
        arg = words[word_no+1]
      else
        arg =  words[word_no+2]
      end
      unless search($black_objs, arg) > 0
        vals << arg
      end
    end
  end
  return vals
end

def parse_file(file_name)
  file = File.readlines(file_name)
  file.each_with_index do |line, line_no|
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
      args.each do |arg| file.insert(line_no+2, "\tBOP_obj_use_promise(#{arg});") end
    end
  end
  puts "modifying and writing file: #{file_name}"
  if File.exists?(file_name)
    File.delete(file_name)
  end

  File.open(file_name, mode="w+") do |f|
    f.puts(file)
  end
end

def find_files(path)
  puts "reading files..."
  files = Array.new()
  cfiles = Array.new()
  Find.find(path) do |file_name|
    unless (search($accepted_files, file_name) - search($ignored_files, file_name)) <= 0
      #puts file_name
      cfiles << file_name
    else
      files<<file_name
    end
  end
  return files, cfiles
end

other, dotc = find_files(ARGV[0])
puts "writing new files"
dotc.each do |f| parse_file(f) end
