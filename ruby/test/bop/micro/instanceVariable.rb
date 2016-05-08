
class InstTest
    def set_foo(n)
      @foo = n
    end
    def read_foo()
      return @foo
    end
end

ASIZE = 4000000


t1 = Time.now
0.upto(PPR.get_group_size-1) do |i| 
    PPR {
    tmp = InstTest.new
    0.upto(ASIZE) do |j|
	tmp1 = InstTest.new
        tmp.set_foo(tmp1)
        tmp.read_foo()
    end
    }
end
PPR.over
t2 = Time.now
puts t2-t1



