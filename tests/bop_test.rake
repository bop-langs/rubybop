require 'pathname'
#### Global variables

# Compiler config
$cc = ENV['CC']
$cc = 'gcc' if $cc =='cc'
$c_flags = '-g3 -fPIC' if $c_flags.nil?
#This is a horrible hack...maybe change this? 
if RUBY_PLATFORM =~ /darwin/ then 
	$ldflags = '-lm -Wl --no-as-needed -ldl -pthread'
else 
	$ldflags = '-lm -Wl,--no-as-needed -ldl -pthread'
end 
$incl = "../bop/"
$params = '' if $params.nil?

# Location of BOP
$bop_dir = (Pathname.new(__FILE__).dirname + '../bop/').cleanpath if $bop_dir.nil?
$bop_lib = $bop_dir + "inst.a" if $bop_lib.nil?

# Objects and programs for clean and realclean
$objs = [] if $objs.nil?
$progs = [] if $progs.nil?


#### API
# bop_test(name, sources, orig):
#   name:    name of test, will have _bop or _orig appended
#   sources: list of C files to compile together.  If nil will be #{name}.c
#   orig:    If a non-BOP version (_orig) should be compiled.  Defaults true
#
# Examples:
#
# Build simple_bop and simple_orig from simple.c
#   bop_test 'simple'
#
# Build complex_* from all C files in current directory
#   bop_test 'complex', FileList.new('*.c')
#
# Build boponly_bop from boponly.c (and no boponly_orig)
#   bop_test 'boponly', nil, false

def bop_test(name, sources = nil, orig = true)
  name = name.to_s

  sources = name.ext('.c') if sources.nil?
  sources = FileList.new(sources)

  objs = sources.ext('.o')

  if orig
    orig_prog = name + '_orig'

    $progs << orig_prog
    $objs.push(*objs)

    Rake::Task[:orig].enhance([orig_prog])
    file orig_prog => objs do
      sh "#{$cc} -I#{$incl} -o #{orig_prog} #{objs * ' '}  #{$ldflags} "
    end
  end

  bop_prog = name + '_bop'
  bop_objs = objs.map {|i| add_prefix(i, 'bop')}

  $progs << bop_prog
  $objs.push(*bop_objs)

  Rake::Task[:bop].enhance([bop_prog])
  file bop_prog => bop_objs + [:boplib] do
    sh "#{$cc} -I#{$incl} -o #{bop_prog} #{bop_objs * ' '} #{$bop_lib} #{$ldflags} "
  end
end

#### Support code

# dir/file => dir/prefix_file
def add_prefix(file, prefix)
  return file if prefix == nil
  pn = Pathname.new(file)
  return pn.dirname + "#{prefix}_#{pn.basename}"
end

#### Rake tasks

desc "Compile BOP test(s)"
task :bop # Prereqs to be added by bop_test

desc "Compile non-BOP test(s)"
task :orig # Prereqs to be added by bop_test

task :boplib do
  sh "cd #{$bop_dir}; make"
end

desc "Remove object files"
task :clean do
  $objs.each {|obj| rm_f obj}
  $progs.each do |fnm|
    rm_f fnm
  end
end

desc "Build both versions of test(s) (default)"
task :all => [:orig, :bop] # :clean
task :default => :all

desc "Force a rebuild"
task :force do
  cd '../../bop' do
    sh 'make -B'
  end
  Rake::Task[:clean].invoke
  Rake::Task[:all].invoke
end

# Teach rake to compile both bop_*.o and *.o from a *.c
bop_o = /^(.*\/)?(bop_)?(.*?)\.o$/
# \1 = directory with last / (optional)
# \2 = bop_ (optional)
# \3 = name of file (without extension)
rule( bop_o => proc {|n| n.sub(bop_o, '\1\3.c')} ) do |t|
  bop_flags = /bop_/ =~ t.name ? '-DBOP' : ''
  sh "#{$cc} #{bop_flags} #{$c_flags} -I#{$bop_dir} -I#{$incl} -c -o #{t.name} #{t.source}"
end
task :run do
  run()

end
def run
=begin FIXME the bop tests are not set up to correcly handle the terminal. Not valid for unit testing
  puts "$prog = " + $progs.to_s
  ENV["BOP_Verbose"]=1.to_s
  $progs.each do |prog|
    cmd = "./#{prog} #{$params}"
    sh cmd do |ok, res|
      if ! ok  && res.exitstatus != 40 then
        fail "cbop test #{cmd} failed with code #{res.exitstatus}"
      else
        puts "\ncbop test #{cmd} successful" #new line
      end
    end
  end
=end
end
