assert_finish 5, %q{
  r, w = IO.pipe
  t1 = Thread.new { r.sysread(1) }
  t2 = Thread.new { r.sysread(1) }
  sleep 0.01 until t1.stop? and t2.stop?
  w.write "a"
  w.write "a"
}, '[ruby-dev:31866]'

assert_finish 10, %q{
  begin
    require "io/nonblock"
    require "timeout"
    timeout(3) do
      r, w = IO.pipe
      w.nonblock?
      w.nonblock = true
      w.write_nonblock("a" * 100000)
      w.nonblock = false
      t1 = Thread.new { w.write("b" * 4096) }
      t2 = Thread.new { w.write("c" * 4096) }
      sleep 0.5
      r.sysread(4096).length
      sleep 0.5
      r.sysread(4096).length
      t1.join
      t2.join
    end
  rescue LoadError, Timeout::Error, NotImplementedError
  end
}, '[ruby-dev:32566]'

assert_finish 1, %q{
  r, w = IO.pipe
  Thread.new {
    w << "ab"
    sleep 0.01
    w << "ab"
  }
  r.gets("abab")
}

#Failed test 1/2
=begin
assert_equal 'ok', %q{
  require 'tmpdir'
  begin
    tmpname = "#{Dir.tmpdir}/ruby-btest-#{$$}-#{rand(0x100000000).to_s(36)}"
    rw = File.open(tmpname, File::RDWR|File::CREAT|File::EXCL)
  rescue Errno::EEXIST
    retry
  end
  save = STDIN.dup
  STDIN.reopen(rw)
  STDIN.reopen(save)
  rw.close
  File.unlink(tmpname) unless RUBY_PLATFORM['nacl']
  :ok
}
=end

#Failed test 2/2
=begin
assert_equal 'ok', %q{
  require 'tmpdir'
  begin
    tmpname = "#{Dir.tmpdir}/ruby-btest-#{$$}-#{rand(0x100000000).to_s(36)}"
    rw = File.open(tmpname, File::RDWR|File::CREAT|File::EXCL)
  rescue Errno::EEXIST
    retry
  end
  save = STDIN.dup
  STDIN.reopen(rw)
  STDIN.print "a"
  STDIN.reopen(save)
  rw.close
  File.unlink(tmpname) unless RUBY_PLATFORM['nacl']
  :ok
}
=end

assert_equal 'ok', %q{
  dup = STDIN.dup
  dupfd = dup.fileno
  dupfd == STDIN.dup.fileno ? :ng : :ok
}, '[ruby-dev:46834]'

assert_normal_exit %q{
  ARGF.set_encoding "foo"
}

assert_normal_exit %q{
  r, w = IO.pipe
  STDOUT.reopen(w)
  STDOUT.reopen(__FILE__, "r")
}, '[ruby-dev:38131]'

#based on http://stackoverflow.com/questions/16948645/how-do-i-test-a-function-with-gets-chomp-in-it
def test_read_user_input
  with_stdin do |user|
    user.puts "user input"
    assert_equal(View.new.read_user_input, "user input")
  end
end

def with_stdin
  stdin = $stdin             # remember $stdin
  $stdin, write = IO.pipe    # create pipe assigning its "read end" to $stdin
  yield write                # pass pipe's "write end" to block
ensure
  write.close                # close pipe
  $stdin = stdin             # restore $stdin
end
