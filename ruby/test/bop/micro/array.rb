
ASIZE = 10000000

a = Array.new(ASIZE)
b = Array.new(ASIZE)

0.upto(a.length-1) do |i|
   a[i] = 1
   b[i] = 0
end

t1 = Time.now
0.upto(PPR.get_group_size-1) do |i|
    PPR {
    0.upto(a.length-1) do |i|
        a[i] = b[i]
    end
    }
end
PPR.over
t2 = Time.now
puts (t2-t1)


